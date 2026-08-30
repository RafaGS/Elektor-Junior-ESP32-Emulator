#include "JuniorMachine.h"

#include <cstring>
#include <iomanip>
#include <iostream>

#if defined(ARDUINO)
#include <Arduino.h>
#endif

#include "../junior_rom.h"

void JuniorMachine::traceRiotAccess(const char* direction, uint16_t offset, uint8_t value) const {
    if (!trace_enabled_.load(std::memory_order_acquire)) {
        return;
    }

    // El refresco de display (CONVD) escribe seis veces por barrido, sin
    // parar, casi siempre con el mismo contenido (0x40/0xFF) - es la
    // inmensa mayoria del volumen de traza y aporta poco una vez
    // sabemos que el display funciona. Con TRACE activo, cada linea es
    // un Serial.printf bloqueante (~8ms a 115200 baudios); saturar el
    // UART con estas lineas ralentiza el bucle real de la CPU en
    // ordenes de magnitud, lo que rompe justo la ventana de antirrebote
    // del teclado que se quiere depurar (efecto observador). Se filtran
    // aqui, dejando solo lo relevante para teclado: escritura de PA80
    // es siempre refresco de display (nunca teclado) y las escrituras
    // de PB82 con un valor de seleccion de digito (0x08,0x0A,0x0C,0x0E,
    // 0x10,0x12) siempre acompañan a ese refresco. Todo lo demas
    // (lecturas de PA80, DDRA/PADD, y PB82 con valores de fila de
    // teclado/GETKEY) se sigue trazando igual que antes.
    static constexpr uint8_t kDigitSelectValues[] = {0x08, 0x0A, 0x0C, 0x0E, 0x10, 0x12};
    const bool is_display_segment_write = (offset == 0x80) && (direction[0] == 'W');
    bool is_digit_select_write = false;
    if (offset == 0x82 && direction[0] == 'W') {
        for (uint8_t digit_pb : kDigitSelectValues) {
            if (value == digit_pb) {
                is_digit_select_write = true;
                break;
            }
        }
    }
    if (is_display_segment_write || is_digit_select_write) {
        return;
    }

#if defined(ARDUINO)
    Serial.printf("[RiotTrace] step=%lu PC=0x%04X opcode=0x%02X A=%d X=%d Y=%d P=%d SP=%d %s $1A%02X = 0x%02X\n",
                  static_cast<unsigned long>(cpu_.instruction_count()),
                  cpu_.pc(),
                  static_cast<int>(cpu_.opcode()),
                  static_cast<int>(cpu_.a()),
                  static_cast<int>(cpu_.x()),
                  static_cast<int>(cpu_.y()),
                  static_cast<int>(cpu_.status()),
                  static_cast<int>(cpu_.sp()),
                  direction,
                  static_cast<int>(offset),
                  static_cast<int>(value));
#else
    std::cout << "step=" << cpu_.instruction_count()
              << " PC=0x" << std::hex << std::setw(4) << std::setfill('0') << cpu_.pc()
              << " opcode=0x" << std::setw(2) << static_cast<int>(cpu_.opcode())
              << " A=" << std::dec << static_cast<int>(cpu_.a())
              << " X=" << static_cast<int>(cpu_.x())
              << " Y=" << static_cast<int>(cpu_.y())
              << " P=" << static_cast<int>(cpu_.status())
              << " SP=" << static_cast<int>(cpu_.sp())
              << " " << direction << " $1A" << std::hex << std::setw(2) << std::setfill('0') << static_cast<int>(offset)
              << " = 0x" << std::setw(2) << static_cast<int>(value)
              << std::dec << "\n";
#endif
}

JuniorMachine::JuniorMachine()
    : cpu_(*this),
      riot_(),
      decoder7442_(),
      display_(),
      external_(),
      ram_{},
      monitor_rom_(junior_monitor_1c00_bin),
      monitor_rom_size_(junior_monitor_1c00_bin_len) {
    reset();
}

JuniorMachine::JuniorMachine(const uint8_t* monitor_rom, std::size_t monitor_rom_size)
    : cpu_(*this),
      riot_(),
      decoder7442_(),
      display_(),
      external_(),
      ram_{},
      monitor_rom_(monitor_rom),
      monitor_rom_size_(monitor_rom_size) {
    reset();
}

uint8_t JuniorMachine::read(uint16_t address) {
    // El Junior real solo tiene 1KB de ROM de monitor, con las lineas de
    // direccion superiores sin decodificar del todo: por eso la misma ROM
    // "aparece" tambien en la cima del espacio de direcciones ($FFFA-$FFFF),
    // que es donde el 6502 espera los vectores NMI/RESET/IRQ. En vez de
    // fijar aqui direcciones magicas (que solo valdrian para ESTA imagen de
    // ROM concreta), se leen directamente los 6 ultimos bytes de la ROM
    // cargada - funciona igual con la ROM de referencia o con una ROM
    // personalizada (usada en los tests nativos).
    if (address >= 0xFFFA && monitor_rom_ != nullptr && monitor_rom_size_ >= 6) {
        const std::size_t index = monitor_rom_size_ - 6 + static_cast<std::size_t>(address - 0xFFFA);
        return monitor_rom_[index];
    }

    if (address >= kRamStart && address <= kRamEnd) {
        return ram_[address];
    }

    if (address >= kRiotStart && address <= kRiotEnd) {
        const uint16_t offset = static_cast<uint16_t>(address - kRiotStart);
        uint8_t result = riot_.read(offset);

        if (offset == 0x80) {
            external_.setPortB(riot_.port_b_data());
            external_.setDDRA(riot_.port_a_ddr());
            external_.setPortA(riot_.port_a_data());
            result = external_.readPortA();
        }

        if (offset == 0x80 || offset == 0x81 || offset == 0x82 || offset == 0x83) {
            traceRiotAccess("READ", offset, result);
        }
        return result;
    }

    if (address >= kMonitorStart && address <= kMonitorEnd) {
        if (monitor_rom_ == nullptr || monitor_rom_size_ == 0) {
            return 0;
        }

        const std::size_t index = static_cast<std::size_t>(address - kMonitorStart);
        if (index < monitor_rom_size_) {
            return monitor_rom_[index];
        }
        return 0;
    }

    return 0;
}

void JuniorMachine::write(uint16_t address, uint8_t value) {
    if (address == 0xFFFC || address == 0xFFFD) {
        // El vector de reset se toma del final de la ROM del monitor.
        // No se permite sobreescribirlo desde el bus en esta capa.
        return;
    }

    if (address >= kRamStart && address <= kRamEnd) {
        ram_[address] = value;
        return;
    }

    if (address >= kRiotStart && address <= kRiotEnd) {
        const uint16_t offset = static_cast<uint16_t>(address - kRiotStart);
        if (offset == 0x80 || offset == 0x81 || offset == 0x82 || offset == 0x83) {
            traceRiotAccess("WRITE", offset, value);
        }

        riot_.write(offset, value);

        if (offset == 0x80) {
            external_.setPortA(value);
            display_.setSegments(value);
        }
        if (offset == 0x82) {
            external_.setPortB(value);

            // El 7442 real ve (PB >> 1) & 0x0F como codigo BCD, no PB
            // directamente (bit0 de PB no forma parte del bus de
            // seleccion). Ver Decoder7442.h para el detalle completo.
            const uint8_t bcd = static_cast<uint8_t>((value >> 1) & 0x0FU);
            decoder7442_.setInput(bcd);

            // Lineas 4..9 del 7442 son los 6 digitos del display
            // (address3..data0). Lineas 0..2 son filas de teclado y la
            // linea 3 (o cualquier BCD invalido) es el estado "apagado"
            // que usa el monitor entre refrescos de digito.
            if (decoder7442_.hasActiveLine() &&
                decoder7442_.activeLine() >= 4 && decoder7442_.activeLine() <= 9) {
                const uint8_t digit_index = static_cast<uint8_t>(decoder7442_.activeLine() - 4);
                display_.setSelectedDigit(digit_index);
            } else {
                display_.clearSelectedDigit();
            }
        }
        if (offset == 0x81) {
            external_.setDDRA(value);
        }

        return;
    }

    if (address >= kMonitorStart && address <= kMonitorEnd) {
        // ROM del monitor: escritura no permitida
        return;
    }
}

void JuniorMachine::reset() {
    std::memset(ram_.data(), 0, ram_.size());
    riot_.reset();
    if (monitor_rom_ == nullptr) {
        // si la ROM no se carga explícitamente, no se hace nada más
    }
    cpu_.reset();

    // La ROM real del monitor NO inicializa el vector NMI (NMIL/NMIH en
    // $1A7A/$1A7B) en su propia rutina RESET - deja su configuracion en
    // manos del usuario, igual que el vector de IRQ/BRK (IRQL/IRQH).
    // Pero, segun el manual del Elektor Junior, la tecla ST esta
    // cableada directamente a la linea NMI del 6502 (sin pasar por el
    // teclado ni la PIA) precisamente para devolver el control al
    // monitor de forma INCONDICIONAL, funcione lo que funcione en ese
    // momento - una via de escape que debe funcionar de fabrica, no
    // solo si el usuario se ha acordado de configurar el vector antes.
    //
    // Por eso aqui, a nivel de emulador (no de la ROM en si, que sigue
    // intacta y de solo lectura), se apunta NMIL/NMIH a SAVE
    // (kMonitorStart, $1C00: el punto de entrada que captura registros
    // y vuelve al monitor) en cada reset. IRQL/IRQH se dejan
    // deliberadamente SIN inicializar: ese vector si es una
    // caracteristica "configurable por el usuario" documentada como tal
    // en el propio monitor (para BRK/IRQ de programas de usuario), no
    // una garantia de retorno incondicional como NMI/ST.
    riot_.write(0x7AU, static_cast<uint8_t>(kMonitorStart & 0x00FFU));
    riot_.write(0x7BU, static_cast<uint8_t>((kMonitorStart >> 8) & 0x00FFU));
}

void JuniorMachine::tick() {
    riot_.tick();
}
