#pragma once

#include <cstdint>

class RIOT6532 {
public:
    static constexpr uint16_t kBaseAddress = 0x1A00;
    static constexpr uint16_t kAddressWindow = 0x0200;
    static constexpr uint16_t kRamSize = 128;
    static constexpr uint16_t kRamOffset = 0x00;
    static constexpr uint16_t kPortADataOffset = 0x80;
    static constexpr uint16_t kPortADdrOffset = 0x81;
    static constexpr uint16_t kPortBDataOffset = 0x82;
    static constexpr uint16_t kPortBDdrOffset = 0x83;

    // Direcciones reales del timer segun los EQU de monitor.asm (la
    // decodificacion de direcciones de esta placa concreta, no el mapa
    // generico de un 6532 en otro sistema). RDFLAG lee el registro de
    // flags Y limpia el flag de timer/IRQ; CNTA-D arrancan el timer sin
    // habilitar IRQ, CNTE-H lo arrancan con IRQ habilitada.
    static constexpr uint16_t kReadFlagOffset = 0xD5;
    static constexpr uint16_t kWriteDiv1Offset = 0xF4;
    static constexpr uint16_t kWriteDiv8Offset = 0xF5;
    static constexpr uint16_t kWriteDiv64Offset = 0xF6;
    static constexpr uint16_t kWriteDiv1024Offset = 0xF7;
    static constexpr uint16_t kWriteDiv1IrqOffset = 0xFC;
    static constexpr uint16_t kWriteDiv8IrqOffset = 0xFD;
    static constexpr uint16_t kWriteDiv64IrqOffset = 0xFE;
    static constexpr uint16_t kWriteDiv1024IrqOffset = 0xFF;

    RIOT6532();

    void reset();
    uint8_t read(uint16_t offset);
    void write(uint16_t offset, uint8_t value);
    void tick();

    uint8_t port_a_data() const noexcept { return port_a_data_; }
    uint8_t port_a_ddr() const noexcept { return port_a_ddr_; }
    uint8_t port_b_data() const noexcept { return port_b_data_; }
    uint8_t port_b_ddr() const noexcept { return port_b_ddr_; }
    uint8_t timer() const noexcept { return timer_; }
    bool interrupt_pending() const noexcept { return interrupt_pending_; }

private:
    void startTimer(uint16_t divider, uint8_t count, bool irq_enabled);

    uint8_t ram_[kRamSize];
    uint8_t port_a_data_;
    uint8_t port_a_ddr_;
    uint8_t port_b_data_;
    uint8_t port_b_ddr_;
    uint8_t timer_;
    uint16_t divider_;
    uint16_t divider_progress_;
    bool timer_expired_;
    bool irq_enabled_;
    bool interrupt_pending_;
};
