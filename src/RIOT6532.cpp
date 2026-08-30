#include "RIOT6532.h"

#include <cstring>

RIOT6532::RIOT6532()
    : ram_{0},
      port_a_data_(0),
      port_a_ddr_(0),
      port_b_data_(0),
      port_b_ddr_(0),
      timer_(0),
      divider_(1),
      divider_progress_(0),
      timer_expired_(false),
      irq_enabled_(false),
      interrupt_pending_(false) {
    reset();
}

void RIOT6532::reset() {
    std::memset(ram_, 0, sizeof(ram_));
    port_a_data_ = 0;
    port_a_ddr_ = 0;
    port_b_data_ = 0;
    port_b_ddr_ = 0;
    timer_ = 0;
    divider_ = 1;
    divider_progress_ = 0;
    timer_expired_ = false;
    irq_enabled_ = false;
    interrupt_pending_ = false;
}

uint8_t RIOT6532::read(uint16_t offset) {
    const uint16_t normalized = offset & 0x00FF;

    if (normalized < 0x80) {
        return ram_[normalized];
    }

    if (normalized == kReadFlagOffset) {
        // BIT7 = flag de timer expirado; BIT6 = flag de PA7 (no
        // implementado, el edge-detect de PA7 no se emula). Leer aqui
        // limpia el flag de timer y cualquier IRQ pendiente por el
        // timer, tal como documenta monitor.asm ("READ FLAG REGISTER
        // AND CLEAR TIMER & IRQ FLAG").
        const uint8_t value = timer_expired_ ? 0x80 : 0x00;
        timer_expired_ = false;
        interrupt_pending_ = false;
        return value;
    }

    switch (normalized) {
        case kPortADataOffset:
            return port_a_data_;
        case kPortADdrOffset:
            return port_a_ddr_;
        case kPortBDataOffset:
            return port_b_data_;
        case kPortBDdrOffset:
            return port_b_ddr_;
        default:
            return 0;
    }
}

void RIOT6532::write(uint16_t offset, uint8_t value) {
    const uint16_t normalized = offset & 0x00FF;

    if (normalized < 0x80) {
        ram_[normalized] = value;
        return;
    }

    switch (normalized) {
        case kPortADataOffset:
            port_a_data_ = value;
            return;
        case kPortADdrOffset:
            port_a_ddr_ = value;
            return;
        case kPortBDataOffset:
            port_b_data_ = value;
            return;
        case kPortBDdrOffset:
            port_b_ddr_ = value;
            return;
        case kWriteDiv1Offset:
            startTimer(1, value, false);
            return;
        case kWriteDiv8Offset:
            startTimer(8, value, false);
            return;
        case kWriteDiv64Offset:
            startTimer(64, value, false);
            return;
        case kWriteDiv1024Offset:
            startTimer(1024, value, false);
            return;
        case kWriteDiv1IrqOffset:
            startTimer(1, value, true);
            return;
        case kWriteDiv8IrqOffset:
            startTimer(8, value, true);
            return;
        case kWriteDiv64IrqOffset:
            startTimer(64, value, true);
            return;
        case kWriteDiv1024IrqOffset:
            startTimer(1024, value, true);
            return;
        default:
            return;
    }
}

void RIOT6532::startTimer(uint16_t divider, uint8_t count, bool irq_enabled) {
    timer_ = count;
    divider_ = divider;
    divider_progress_ = 0;
    irq_enabled_ = irq_enabled;
    timer_expired_ = false;
    // Escribir un nuevo conteo cancela cualquier IRQ pendiente anterior,
    // igual que en el 6532 real.
    interrupt_pending_ = false;
}

void RIOT6532::tick() {
    // No es cycle-accurate: aqui "un tick" equivale a una instruccion de
    // CPU ejecutada, no a un ciclo de reloj real. El divisor (1/8/64/1024)
    // se respeta de forma relativa entre si (el timer de division 1024
    // tarda 1024 veces mas ticks en expirar que el de division 1), pero
    // no corresponde a ciclos de reloj reales del 6502. Coherente con el
    // resto del emulador (ver kInstructionsPerTick en main.cpp), que
    // tampoco es cycle-accurate.
    if (timer_expired_) {
        // Tras expirar, el 6532 real sigue decrementando en cada ciclo
        // (ya sin divisor) mientras nadie lea el flag, dando la vuelta
        // sin parar. Aqui basta con reflejar ese "sigue corriendo libre"
        // de forma aproximada sin necesidad de precision adicional.
        --timer_;
        return;
    }

    ++divider_progress_;
    if (divider_progress_ < divider_) {
        return;
    }
    divider_progress_ = 0;

    if (timer_ > 0) {
        --timer_;
        return;
    }

    // timer_ ya estaba a 0 antes de este intervalo: acaba de expirar.
    timer_expired_ = true;
    if (irq_enabled_) {
        interrupt_pending_ = true;
    }
    --timer_;  // empieza a dar la vuelta libremente, como el hardware real
}
