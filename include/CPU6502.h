#pragma once

#include <cstdint>

#include "Machine.h"

class CPU6502 {
public:
    using AddressingMode = void (CPU6502::*)();
    using OpcodeHandler = void (CPU6502::*)();

    CPU6502();
    explicit CPU6502(Machine& machine);

    void attach(Machine& machine);
    void reset();
    void step();
    void irq();
    void nmi();

    uint16_t pc() const noexcept { return pc_; }
    uint8_t sp() const noexcept { return sp_; }
    uint8_t a() const noexcept { return a_; }
    uint8_t x() const noexcept { return x_; }
    uint8_t y() const noexcept { return y_; }
    uint8_t status() const noexcept { return status_; }
    uint8_t opcode() const noexcept { return opcode_; }

    uint32_t instruction_count() const noexcept { return instruction_count_; }
    uint32_t clock_ticks() const noexcept { return clock_ticks_; }

private:
    Machine* machine_;

    uint16_t pc_;
    uint8_t sp_;
    uint8_t a_;
    uint8_t x_;
    uint8_t y_;
    uint8_t status_;

    uint32_t instruction_count_;
    uint32_t clock_ticks_;
    uint32_t clock_goal_;
    uint16_t old_pc_;
    uint16_t effective_address_;
    uint16_t relative_address_;
    uint16_t value_;
    uint16_t result_;
    uint8_t opcode_;
    uint8_t old_status_;
    uint8_t penalty_op_;
    uint8_t penalty_addr_;

    static const AddressingMode kAddressTable[256];
    static const OpcodeHandler kOpcodeTable[256];
    static const uint8_t kTickTable[256];

    uint8_t read8(uint16_t address) const;
    void write8(uint16_t address, uint8_t value) const;
    uint16_t read16(uint16_t address) const;

    void push16(uint16_t value);
    void push8(uint8_t value);
    uint16_t pull16();
    uint8_t pull8();

    void imp();
    void acc();
    void imm();
    void zp();
    void zpx();
    void zpy();
    void rel();
    void abso();
    void absx();
    void absy();
    void ind();
    void indx();
    void indy();

    void adc();
    void _and();
    void asl();
    void bcc();
    void bcs();
    void beq();
    void _bit();
    void bmi();
    void bne();
    void bpl();
    void brk();
    void bvc();
    void bvs();
    void clc();
    void cld();
    void _cli();
    void clv();
    void cmp();
    void cpx();
    void cpy();
    void dec();
    void dex();
    void dey();
    void eor();
    void inc();
    void inx();
    void iny();
    void jmp();
    void jsr();
    void lda();
    void ldx();
    void ldy();
    void lsr();
    void nop();
    void ora();
    void pha();
    void php();
    void pla();
    void plp();
    void rol();
    void ror();
    void rti();
    void rts();
    void sbc();
    void sec();
    void sed();
    void _sei();
    void sta();
    void stx();
    void sty();
    void tax();
    void tay();
    void tsx();
    void txa();
    void txs();
    void tya();
    uint16_t get_value();
    uint16_t get_value16();
    void put_value(uint16_t value);

#ifdef UNDOCUMENTED
    void lax();
    void sax();
    void dcp();
    void isb();
    void slo();
    void rla();
    void sre();
    void rra();
#endif
};
