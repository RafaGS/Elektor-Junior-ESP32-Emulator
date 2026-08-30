#include "CPU6502.h"

namespace {
constexpr uint8_t FLAG_CARRY = 0x01;
constexpr uint8_t FLAG_ZERO = 0x02;
constexpr uint8_t FLAG_INTERRUPT = 0x04;
constexpr uint8_t FLAG_DECIMAL = 0x08;
constexpr uint8_t FLAG_BREAK = 0x10;
constexpr uint8_t FLAG_CONSTANT = 0x20;
constexpr uint8_t FLAG_OVERFLOW = 0x40;
constexpr uint8_t FLAG_SIGN = 0x80;
constexpr uint16_t BASE_STACK = 0x100;

#define saveaccum(n) a_ = static_cast<uint8_t>((n) & 0x00FF)

#define setcarry() status_ |= FLAG_CARRY
#define clearcarry() status_ &= static_cast<uint8_t>(~FLAG_CARRY)
#define setzero() status_ |= FLAG_ZERO
#define clearzero() status_ &= static_cast<uint8_t>(~FLAG_ZERO)
#define setinterrupt() status_ |= FLAG_INTERRUPT
#define clearinterrupt() status_ &= static_cast<uint8_t>(~FLAG_INTERRUPT)
#define setdecimal() status_ |= FLAG_DECIMAL
#define cleardecimal() status_ &= static_cast<uint8_t>(~FLAG_DECIMAL)
#define setoverflow() status_ |= FLAG_OVERFLOW
#define clearoverflow() status_ &= static_cast<uint8_t>(~FLAG_OVERFLOW)
#define setsign() status_ |= FLAG_SIGN
#define clearsign() status_ &= static_cast<uint8_t>(~FLAG_SIGN)

#define zerocalc(n)                                                         \
    do {                                                                    \
        if ((n) & 0x00FF)                                                   \
            clearzero();                                                    \
        else                                                                \
            setzero();                                                      \
    } while (0)

#define signcalc(n)                                                         \
    do {                                                                    \
        if ((n) & 0x0080)                                                   \
            setsign();                                                      \
        else                                                                \
            clearsign();                                                    \
    } while (0)

#define carrycalc(n)                                                        \
    do {                                                                    \
        if ((n) & 0xFF00)                                                   \
            setcarry();                                                     \
        else                                                                \
            clearcarry();                                                   \
    } while (0)

#define overflowcalc(n, m, o)                                               \
    do {                                                                    \
        if (((n) ^ static_cast<uint16_t>(m)) & ((n) ^ (o)) & 0x0080)         \
            setoverflow();                                                  \
        else                                                                \
            clearoverflow();                                                \
    } while (0)
}  // namespace

CPU6502::CPU6502() : machine_(nullptr), pc_(0), sp_(0), a_(0), x_(0), y_(0), status_(0), instruction_count_(0), clock_ticks_(0), clock_goal_(0), old_pc_(0), effective_address_(0), relative_address_(0), value_(0), result_(0), opcode_(0), old_status_(0), penalty_op_(0), penalty_addr_(0) {}

CPU6502::CPU6502(Machine& machine) : CPU6502() {
    attach(machine);
}

void CPU6502::attach(Machine& machine) {
    machine_ = &machine;
}

uint8_t CPU6502::read8(uint16_t address) const {
    if (machine_ == nullptr) {
        return 0;
    }
    return machine_->read(address);
}

void CPU6502::write8(uint16_t address, uint8_t value) const {
    if (machine_ == nullptr) {
        return;
    }
    machine_->write(address, value);
}

uint16_t CPU6502::read16(uint16_t address) const {
    return static_cast<uint16_t>(read8(address)) | (static_cast<uint16_t>(read8(address + 1)) << 8);
}

void CPU6502::push16(uint16_t value) {
    write8(BASE_STACK + sp_, static_cast<uint8_t>((value >> 8) & 0xFF));
    write8(BASE_STACK + static_cast<uint16_t>((sp_ - 1) & 0xFF), static_cast<uint8_t>(value & 0xFF));
    sp_ -= 2;
}

void CPU6502::push8(uint8_t value) {
    write8(BASE_STACK + sp_--, value);
}

uint16_t CPU6502::pull16() {
    uint16_t temp16 = 0;
    temp16 = read8(BASE_STACK + static_cast<uint16_t>((sp_ + 1) & 0xFF)) |
             (static_cast<uint16_t>(read8(BASE_STACK + static_cast<uint16_t>((sp_ + 2) & 0xFF))) << 8);
    sp_ += 2;
    return temp16;
}

uint8_t CPU6502::pull8() {
    return read8(BASE_STACK + static_cast<uint16_t>(++sp_));
}

void CPU6502::reset() {
    if (machine_ == nullptr) {
        return;
    }

    pc_ = static_cast<uint16_t>(machine_->read(0xFFFC)) | (static_cast<uint16_t>(machine_->read(0xFFFD)) << 8);
    a_ = 0;
    x_ = 0;
    y_ = 0;
    sp_ = 0xFD;
    status_ |= FLAG_CONSTANT;
    instruction_count_ = 0;
    clock_ticks_ = 0;
    clock_goal_ = 0;
}

void CPU6502::imp() {
    // implied addressing mode
}

void CPU6502::acc() {
    // accumulator addressing mode
}

void CPU6502::imm() {
    effective_address_ = pc_++;
}

void CPU6502::zp() {
    effective_address_ = static_cast<uint16_t>(read8(static_cast<uint16_t>(pc_++)));
}

void CPU6502::zpx() {
    effective_address_ = (static_cast<uint16_t>(read8(static_cast<uint16_t>(pc_++))) + static_cast<uint16_t>(x_)) & 0xFF;
}

void CPU6502::zpy() {
    effective_address_ = (static_cast<uint16_t>(read8(static_cast<uint16_t>(pc_++))) + static_cast<uint16_t>(y_)) & 0xFF;
}

void CPU6502::rel() {
    relative_address_ = static_cast<uint16_t>(read8(pc_++));
    if (relative_address_ & 0x80) {
        relative_address_ |= 0xFF00;
    }
}

void CPU6502::abso() {
    effective_address_ = static_cast<uint16_t>(read8(pc_)) | (static_cast<uint16_t>(read8(pc_ + 1)) << 8);
    pc_ += 2;
}

void CPU6502::absx() {
    uint16_t startpage = 0;
    effective_address_ = static_cast<uint16_t>(read8(pc_)) | (static_cast<uint16_t>(read8(pc_ + 1)) << 8);
    startpage = effective_address_ & 0xFF00;
    effective_address_ += static_cast<uint16_t>(x_);

    if (startpage != (effective_address_ & 0xFF00)) {
        penalty_addr_ = 1;
    }

    pc_ += 2;
}

void CPU6502::absy() {
    uint16_t startpage = 0;
    effective_address_ = static_cast<uint16_t>(read8(pc_)) | (static_cast<uint16_t>(read8(pc_ + 1)) << 8);
    startpage = effective_address_ & 0xFF00;
    effective_address_ += static_cast<uint16_t>(y_);

    if (startpage != (effective_address_ & 0xFF00)) {
        penalty_addr_ = 1;
    }

    pc_ += 2;
}

void CPU6502::ind() {
    uint16_t eahelp = 0;
    uint16_t eahelp2 = 0;
    eahelp = static_cast<uint16_t>(read8(pc_)) | (static_cast<uint16_t>(read8(pc_ + 1)) << 8);
    eahelp2 = (eahelp & 0xFF00) | ((eahelp + 1) & 0x00FF);
    effective_address_ = static_cast<uint16_t>(read8(eahelp)) | (static_cast<uint16_t>(read8(eahelp2)) << 8);
    pc_ += 2;
}

void CPU6502::indx() {
    uint16_t eahelp = 0;
    eahelp = static_cast<uint16_t>((static_cast<uint16_t>(read8(pc_++)) + static_cast<uint16_t>(x_)) & 0xFF);
    effective_address_ = static_cast<uint16_t>(read8(eahelp & 0x00FF)) |
                        (static_cast<uint16_t>(read8((eahelp + 1) & 0x00FF)) << 8);
}

void CPU6502::indy() {
    uint16_t eahelp = 0;
    uint16_t eahelp2 = 0;
    uint16_t startpage = 0;
    eahelp = static_cast<uint16_t>(read8(pc_++));
    eahelp2 = (eahelp & 0xFF00) | ((eahelp + 1) & 0x00FF);
    effective_address_ = static_cast<uint16_t>(read8(eahelp)) | (static_cast<uint16_t>(read8(eahelp2)) << 8);
    startpage = effective_address_ & 0xFF00;
    effective_address_ += static_cast<uint16_t>(y_);

    if (startpage != (effective_address_ & 0xFF00)) {
        penalty_addr_ = 1;
    }
}

uint16_t CPU6502::get_value() {
    if (kAddressTable[opcode_] == &CPU6502::acc) {
        return static_cast<uint16_t>(a_);
    }
    return static_cast<uint16_t>(read8(effective_address_));
}

uint16_t CPU6502::get_value16() {
    return static_cast<uint16_t>(read8(effective_address_)) |
           (static_cast<uint16_t>(read8(effective_address_ + 1)) << 8);
}

void CPU6502::put_value(uint16_t value) {
    if (kAddressTable[opcode_] == &CPU6502::acc) {
        a_ = static_cast<uint8_t>(value & 0x00FF);
    } else {
        write8(effective_address_, static_cast<uint8_t>(value & 0x00FF));
    }
}

void CPU6502::adc() {
    penalty_op_ = 1;
    value_ = get_value();
    const uint16_t carry_in = static_cast<uint16_t>(status_ & FLAG_CARRY);
    result_ = static_cast<uint16_t>(a_) + value_ + carry_in;

    // Peculiaridad real y documentada del 6502 NMOS: en modo decimal, el
    // flag Z se calcula sobre la suma BINARIA sin ajustar, no sobre el
    // resultado BCD final. Por eso se calcula aqui, con el mismo criterio
    // tanto en modo binario como decimal, antes de tocar nada mas.
    zerocalc(result_);

    if (status_ & FLAG_DECIMAL) {
#ifndef NES_CPU
        // Algoritmo documentado de suma BCD para 6502 NMOS (ver
        // "Decimal Mode" de Bruce Clark, 6502.org). N y V se calculan
        // sobre el resultado intermedio de la fila baja+alta ANTES del
        // ajuste final de decena (+$60) - asi es como se comporta el
        // hardware real, no un descuido.
        uint16_t al = (static_cast<uint16_t>(a_) & 0x0FU) + (value_ & 0x0FU) + carry_in;
        if (al >= 0x0AU) {
            al = ((al + 0x06U) & 0x0FU) + 0x10U;
        }

        uint16_t sum = (static_cast<uint16_t>(a_) & 0xF0U) + (value_ & 0xF0U) + al;

        signcalc(sum);
        overflowcalc(sum, a_, value_);

        if (sum >= 0xA0U) {
            sum += 0x60U;
        }

        if (sum >= 0x100U) {
            setcarry();
        } else {
            clearcarry();
        }

        ++clock_ticks_;
        saveaccum(sum);
        return;
#endif
    }

    carrycalc(result_);
    overflowcalc(result_, a_, value_);
    signcalc(result_);
    saveaccum(result_);
}

void CPU6502::_and() {
    penalty_op_ = 1;
    value_ = get_value();
    result_ = static_cast<uint16_t>(a_) & value_;

    zerocalc(result_);
    signcalc(result_);

    saveaccum(result_);
}

void CPU6502::asl() {
    value_ = get_value();
    result_ = value_ << 1;

    carrycalc(result_);
    zerocalc(result_);
    signcalc(result_);

    put_value(result_);
}

void CPU6502::bcc() {
    if ((status_ & FLAG_CARRY) == 0) {
        old_pc_ = pc_;
        pc_ += relative_address_;
        if ((old_pc_ & 0xFF00) != (pc_ & 0xFF00)) {
            clock_ticks_ += 2;
        } else {
            ++clock_ticks_;
        }
    }
}

void CPU6502::bcs() {
    if ((status_ & FLAG_CARRY) == FLAG_CARRY) {
        old_pc_ = pc_;
        pc_ += relative_address_;
        if ((old_pc_ & 0xFF00) != (pc_ & 0xFF00)) {
            clock_ticks_ += 2;
        } else {
            ++clock_ticks_;
        }
    }
}

void CPU6502::beq() {
    if ((status_ & FLAG_ZERO) == FLAG_ZERO) {
        old_pc_ = pc_;
        pc_ += relative_address_;
        if ((old_pc_ & 0xFF00) != (pc_ & 0xFF00)) {
            clock_ticks_ += 2;
        } else {
            ++clock_ticks_;
        }
    }
}

void CPU6502::_bit() {
    value_ = get_value();
    result_ = static_cast<uint16_t>(a_) & value_;

    zerocalc(result_);
    status_ = static_cast<uint8_t>((status_ & 0x3F) | (value_ & 0xC0));
}

void CPU6502::bmi() {
    if ((status_ & FLAG_SIGN) == FLAG_SIGN) {
        old_pc_ = pc_;
        pc_ += relative_address_;
        if ((old_pc_ & 0xFF00) != (pc_ & 0xFF00)) {
            clock_ticks_ += 2;
        } else {
            ++clock_ticks_;
        }
    }
}

void CPU6502::bne() {
    if ((status_ & FLAG_ZERO) == 0) {
        old_pc_ = pc_;
        pc_ += relative_address_;
        if ((old_pc_ & 0xFF00) != (pc_ & 0xFF00)) {
            clock_ticks_ += 2;
        } else {
            ++clock_ticks_;
        }
    }
}

void CPU6502::bpl() {
    if ((status_ & FLAG_SIGN) == 0) {
        old_pc_ = pc_;
        pc_ += relative_address_;
        if ((old_pc_ & 0xFF00) != (pc_ & 0xFF00)) {
            clock_ticks_ += 2;
        } else {
            ++clock_ticks_;
        }
    }
}

void CPU6502::brk() {
    ++pc_;
    push16(pc_);
    push8(status_ | FLAG_BREAK);
    setinterrupt();
    pc_ = static_cast<uint16_t>(read8(0xFFFE)) | (static_cast<uint16_t>(read8(0xFFFF)) << 8);
}

void CPU6502::bvc() {
    if ((status_ & FLAG_OVERFLOW) == 0) {
        old_pc_ = pc_;
        pc_ += relative_address_;
        if ((old_pc_ & 0xFF00) != (pc_ & 0xFF00)) {
            clock_ticks_ += 2;
        } else {
            ++clock_ticks_;
        }
    }
}

void CPU6502::bvs() {
    if ((status_ & FLAG_OVERFLOW) == FLAG_OVERFLOW) {
        old_pc_ = pc_;
        pc_ += relative_address_;
        if ((old_pc_ & 0xFF00) != (pc_ & 0xFF00)) {
            clock_ticks_ += 2;
        } else {
            ++clock_ticks_;
        }
    }
}

void CPU6502::clc() {
    clearcarry();
}

void CPU6502::cld() {
    cleardecimal();
}

void CPU6502::_cli() {
    clearinterrupt();
}

void CPU6502::clv() {
    clearoverflow();
}

void CPU6502::cmp() {
    penalty_op_ = 1;
    value_ = get_value();
    result_ = static_cast<uint16_t>(a_) - value_;

    if (a_ >= static_cast<uint8_t>(value_ & 0x00FF)) {
        setcarry();
    } else {
        clearcarry();
    }
    if (a_ == static_cast<uint8_t>(value_ & 0x00FF)) {
        setzero();
    } else {
        clearzero();
    }
    signcalc(result_);
}

void CPU6502::cpx() {
    value_ = get_value();
    result_ = static_cast<uint16_t>(x_) - value_;

    if (x_ >= static_cast<uint8_t>(value_ & 0x00FF)) {
        setcarry();
    } else {
        clearcarry();
    }
    if (x_ == static_cast<uint8_t>(value_ & 0x00FF)) {
        setzero();
    } else {
        clearzero();
    }
    signcalc(result_);
}

void CPU6502::cpy() {
    value_ = get_value();
    result_ = static_cast<uint16_t>(y_) - value_;

    if (y_ >= static_cast<uint8_t>(value_ & 0x00FF)) {
        setcarry();
    } else {
        clearcarry();
    }
    if (y_ == static_cast<uint8_t>(value_ & 0x00FF)) {
        setzero();
    } else {
        clearzero();
    }
    signcalc(result_);
}

void CPU6502::dec() {
    value_ = get_value();
    result_ = value_ - 1;

    zerocalc(result_);
    signcalc(result_);

    put_value(result_);
}

void CPU6502::dex() {
    --x_;

    zerocalc(x_);
    signcalc(x_);
}

void CPU6502::dey() {
    --y_;

    zerocalc(y_);
    signcalc(y_);
}

void CPU6502::eor() {
    penalty_op_ = 1;
    value_ = get_value();
    result_ = static_cast<uint16_t>(a_) ^ value_;

    zerocalc(result_);
    signcalc(result_);

    saveaccum(result_);
}

void CPU6502::inc() {
    value_ = get_value();
    result_ = value_ + 1;

    zerocalc(result_);
    signcalc(result_);

    put_value(result_);
}

void CPU6502::inx() {
    ++x_;

    zerocalc(x_);
    signcalc(x_);
}

void CPU6502::iny() {
    ++y_;

    zerocalc(y_);
    signcalc(y_);
}

void CPU6502::jmp() {
    pc_ = effective_address_;
}

void CPU6502::jsr() {
    push16(pc_ - 1);
    pc_ = effective_address_;
}

void CPU6502::lda() {
    penalty_op_ = 1;
    value_ = get_value();
    a_ = static_cast<uint8_t>(value_ & 0x00FF);

    zerocalc(a_);
    signcalc(a_);
}

void CPU6502::ldx() {
    penalty_op_ = 1;
    value_ = get_value();
    x_ = static_cast<uint8_t>(value_ & 0x00FF);

    zerocalc(x_);
    signcalc(x_);
}

void CPU6502::ldy() {
    penalty_op_ = 1;
    value_ = get_value();
    y_ = static_cast<uint8_t>(value_ & 0x00FF);

    zerocalc(y_);
    signcalc(y_);
}

void CPU6502::lsr() {
    value_ = get_value();
    result_ = value_ >> 1;

    if (value_ & 1) {
        setcarry();
    } else {
        clearcarry();
    }
    zerocalc(result_);
    signcalc(result_);

    put_value(result_);
}

void CPU6502::nop() {
    switch (opcode_) {
        case 0x1C:
        case 0x3C:
        case 0x5C:
        case 0x7C:
        case 0xDC:
        case 0xFC:
            penalty_op_ = 1;
            break;
        default:
            break;
    }
}

void CPU6502::ora() {
    penalty_op_ = 1;
    value_ = get_value();
    result_ = static_cast<uint16_t>(a_) | value_;

    zerocalc(result_);
    signcalc(result_);

    saveaccum(result_);
}

void CPU6502::pha() {
    push8(a_);
}

void CPU6502::php() {
    push8(status_ | FLAG_BREAK);
}

void CPU6502::pla() {
    a_ = pull8();

    zerocalc(a_);
    signcalc(a_);
}

void CPU6502::plp() {
    status_ = pull8() | FLAG_CONSTANT;
}

void CPU6502::rol() {
    value_ = get_value();
    result_ = (value_ << 1) | (status_ & FLAG_CARRY);

    carrycalc(result_);
    zerocalc(result_);
    signcalc(result_);

    put_value(result_);
}

void CPU6502::ror() {
    value_ = get_value();
    result_ = (value_ >> 1) | ((status_ & FLAG_CARRY) << 7);

    if (value_ & 1) {
        setcarry();
    } else {
        clearcarry();
    }
    zerocalc(result_);
    signcalc(result_);

    put_value(result_);
}

void CPU6502::rti() {
    status_ = pull8();
    value_ = pull16();
    pc_ = value_;
}

void CPU6502::rts() {
    value_ = pull16();
    pc_ = value_ + 1;
}

void CPU6502::sbc() {
    penalty_op_ = 1;
    const uint16_t operand = get_value();
    value_ = operand ^ 0x00FF;
    const uint16_t carry_in = static_cast<uint16_t>(status_ & FLAG_CARRY);
    result_ = static_cast<uint16_t>(a_) + value_ + carry_in;

    // A diferencia de ADC, en SBC los flags C/Z/N/V del 6502 NMOS real
    // coinciden con los de la resta binaria estandar, tambien en modo
    // decimal - la peculiaridad de BCD aqui afecta solo al VALOR final
    // guardado en A, no a los flags.
    carrycalc(result_);
    zerocalc(result_);
    overflowcalc(result_, a_, value_);
    signcalc(result_);

    if (status_ & FLAG_DECIMAL) {
#ifndef NES_CPU
        // Algoritmo documentado de resta BCD para 6502 NMOS (ver
        // "Decimal Mode" de Bruce Clark, 6502.org). Usa el operando
        // decimal real (sin invertir), no su complemento a unos.
        int32_t al = (static_cast<int32_t>(a_) & 0x0F) - (static_cast<int32_t>(operand) & 0x0F) +
                     static_cast<int32_t>(carry_in) - 1;
        if (al < 0) {
            al = ((al - 0x06) & 0x0F) - 0x10;
        }

        int32_t sum = (static_cast<int32_t>(a_) & 0xF0) - (static_cast<int32_t>(operand) & 0xF0) + al;
        if (sum < 0) {
            sum -= 0x60;
        }

        ++clock_ticks_;
        a_ = static_cast<uint8_t>(sum & 0xFF);
        return;
#endif
    }

    saveaccum(result_);
}

void CPU6502::sec() {
    setcarry();
}

void CPU6502::sed() {
    setdecimal();
}

void CPU6502::_sei() {
    setinterrupt();
}

void CPU6502::sta() {
    put_value(a_);
}

void CPU6502::stx() {
    put_value(x_);
}

void CPU6502::sty() {
    put_value(y_);
}

void CPU6502::tax() {
    x_ = a_;

    zerocalc(x_);
    signcalc(x_);
}

void CPU6502::tay() {
    y_ = a_;

    zerocalc(y_);
    signcalc(y_);
}

void CPU6502::tsx() {
    x_ = sp_;

    zerocalc(x_);
    signcalc(x_);
}

void CPU6502::txa() {
    a_ = x_;

    zerocalc(a_);
    signcalc(a_);
}

void CPU6502::txs() {
    sp_ = x_;
}

void CPU6502::tya() {
    a_ = y_;

    zerocalc(a_);
    signcalc(a_);
}

#ifdef UNDOCUMENTED
void CPU6502::lax() {
    lda();
    ldx();
}

void CPU6502::sax() {
    sta();
    stx();
    put_value(a_ & x_);
    if (penalty_op_ && penalty_addr_) {
        --clock_ticks_;
    }
}

void CPU6502::dcp() {
    dec();
    cmp();
    if (penalty_op_ && penalty_addr_) {
        --clock_ticks_;
    }
}

void CPU6502::isb() {
    inc();
    sbc();
    if (penalty_op_ && penalty_addr_) {
        --clock_ticks_;
    }
}

void CPU6502::slo() {
    asl();
    ora();
    if (penalty_op_ && penalty_addr_) {
        --clock_ticks_;
    }
}

void CPU6502::rla() {
    rol();
    _and();
    if (penalty_op_ && penalty_addr_) {
        --clock_ticks_;
    }
}

void CPU6502::sre() {
    lsr();
    eor();
    if (penalty_op_ && penalty_addr_) {
        --clock_ticks_;
    }
}

void CPU6502::rra() {
    ror();
    adc();
    if (penalty_op_ && penalty_addr_) {
        --clock_ticks_;
    }
}
#endif

void CPU6502::irq() {
    push16(pc_);
    push8(status_);
    status_ |= FLAG_INTERRUPT;
    pc_ = static_cast<uint16_t>(read8(0xFFFE)) | (static_cast<uint16_t>(read8(0xFFFF)) << 8);
}

void CPU6502::nmi() {
    push16(pc_);
    push8(status_);
    status_ |= FLAG_INTERRUPT;
    pc_ = static_cast<uint16_t>(read8(0xFFFA)) | (static_cast<uint16_t>(read8(0xFFFB)) << 8);
}

void CPU6502::step() {
    if (machine_ == nullptr) {
        return;
    }

    opcode_ = read8(pc_++);
    status_ |= FLAG_CONSTANT;

    penalty_op_ = 0;
    penalty_addr_ = 0;

    const auto addressing = kAddressTable[opcode_];
    const auto handler = kOpcodeTable[opcode_];

    if (addressing != nullptr) {
        (this->*addressing)();
    }
    if (handler != nullptr) {
        (this->*handler)();
    }

    clock_ticks_ += kTickTable[opcode_];
    if (penalty_op_ && penalty_addr_) {
        ++clock_ticks_;
    }
    clock_goal_ = clock_ticks_;
    ++instruction_count_;
}

#ifndef UNDOCUMENTED
#define lax nop
#define sax nop
#define dcp nop
#define isb nop
#define slo nop
#define rla nop
#define sre nop
#define rra nop
#endif

const CPU6502::AddressingMode CPU6502::kAddressTable[256] = {
    &CPU6502::imp, &CPU6502::indx, &CPU6502::imp, &CPU6502::indx, &CPU6502::zp, &CPU6502::zp, &CPU6502::zp, &CPU6502::zp, &CPU6502::imp, &CPU6502::imm, &CPU6502::acc, &CPU6502::imm, &CPU6502::abso, &CPU6502::abso, &CPU6502::abso, &CPU6502::abso,
    &CPU6502::rel, &CPU6502::indy, &CPU6502::imp, &CPU6502::indy, &CPU6502::zpx, &CPU6502::zpx, &CPU6502::zpx, &CPU6502::zpx, &CPU6502::imp, &CPU6502::absy, &CPU6502::imp, &CPU6502::absy, &CPU6502::absx, &CPU6502::absx, &CPU6502::absx, &CPU6502::absx,
    &CPU6502::abso, &CPU6502::indx, &CPU6502::imp, &CPU6502::indx, &CPU6502::zp, &CPU6502::zp, &CPU6502::zp, &CPU6502::zp, &CPU6502::imp, &CPU6502::imm, &CPU6502::acc, &CPU6502::imm, &CPU6502::abso, &CPU6502::abso, &CPU6502::abso, &CPU6502::abso,
    &CPU6502::rel, &CPU6502::indy, &CPU6502::imp, &CPU6502::indy, &CPU6502::zpx, &CPU6502::zpx, &CPU6502::zpx, &CPU6502::zpx, &CPU6502::imp, &CPU6502::absy, &CPU6502::imp, &CPU6502::absy, &CPU6502::absx, &CPU6502::absx, &CPU6502::absx, &CPU6502::absx,
    &CPU6502::imp, &CPU6502::indx, &CPU6502::imp, &CPU6502::indx, &CPU6502::zp, &CPU6502::zp, &CPU6502::zp, &CPU6502::zp, &CPU6502::imp, &CPU6502::imm, &CPU6502::acc, &CPU6502::imm, &CPU6502::abso, &CPU6502::abso, &CPU6502::abso, &CPU6502::abso,
    &CPU6502::rel, &CPU6502::indy, &CPU6502::imp, &CPU6502::indy, &CPU6502::zpx, &CPU6502::zpx, &CPU6502::zpx, &CPU6502::zpx, &CPU6502::imp, &CPU6502::absy, &CPU6502::imp, &CPU6502::absy, &CPU6502::absx, &CPU6502::absx, &CPU6502::absx, &CPU6502::absx,
    &CPU6502::imp, &CPU6502::indx, &CPU6502::imp, &CPU6502::indx, &CPU6502::zp, &CPU6502::zp, &CPU6502::zp, &CPU6502::zp, &CPU6502::imp, &CPU6502::imm, &CPU6502::acc, &CPU6502::imm, &CPU6502::ind, &CPU6502::abso, &CPU6502::abso, &CPU6502::abso,
    &CPU6502::rel, &CPU6502::indy, &CPU6502::imp, &CPU6502::indy, &CPU6502::zpx, &CPU6502::zpx, &CPU6502::zpx, &CPU6502::zpx, &CPU6502::imp, &CPU6502::absy, &CPU6502::imp, &CPU6502::absy, &CPU6502::absx, &CPU6502::absx, &CPU6502::absx, &CPU6502::absx,
    &CPU6502::imm, &CPU6502::indx, &CPU6502::imm, &CPU6502::indx, &CPU6502::zp, &CPU6502::zp, &CPU6502::zp, &CPU6502::zp, &CPU6502::imp, &CPU6502::imm, &CPU6502::imp, &CPU6502::imm, &CPU6502::abso, &CPU6502::abso, &CPU6502::abso, &CPU6502::abso,
    &CPU6502::rel, &CPU6502::indy, &CPU6502::imp, &CPU6502::indy, &CPU6502::zpx, &CPU6502::zpx, &CPU6502::zpy, &CPU6502::zpy, &CPU6502::imp, &CPU6502::absy, &CPU6502::imp, &CPU6502::absy, &CPU6502::absx, &CPU6502::absx, &CPU6502::absy, &CPU6502::absy,
    &CPU6502::imm, &CPU6502::indx, &CPU6502::imm, &CPU6502::indx, &CPU6502::zp, &CPU6502::zp, &CPU6502::zp, &CPU6502::zp, &CPU6502::imp, &CPU6502::imm, &CPU6502::imp, &CPU6502::imm, &CPU6502::abso, &CPU6502::abso, &CPU6502::abso, &CPU6502::abso,
    &CPU6502::rel, &CPU6502::indy, &CPU6502::imp, &CPU6502::indy, &CPU6502::zpx, &CPU6502::zpx, &CPU6502::zpy, &CPU6502::zpy, &CPU6502::imp, &CPU6502::absy, &CPU6502::imp, &CPU6502::absy, &CPU6502::absx, &CPU6502::absx, &CPU6502::absy, &CPU6502::absy,
    &CPU6502::imm, &CPU6502::indx, &CPU6502::imm, &CPU6502::indx, &CPU6502::zp, &CPU6502::zp, &CPU6502::zp, &CPU6502::zp, &CPU6502::imp, &CPU6502::imm, &CPU6502::imp, &CPU6502::imm, &CPU6502::abso, &CPU6502::abso, &CPU6502::abso, &CPU6502::abso,
    &CPU6502::rel, &CPU6502::indy, &CPU6502::imp, &CPU6502::indy, &CPU6502::zpx, &CPU6502::zpx, &CPU6502::zpx, &CPU6502::zpx, &CPU6502::imp, &CPU6502::absy, &CPU6502::imp, &CPU6502::absy, &CPU6502::absx, &CPU6502::absx, &CPU6502::absx, &CPU6502::absx,
    &CPU6502::imm, &CPU6502::indx, &CPU6502::imm, &CPU6502::indx, &CPU6502::zp, &CPU6502::zp, &CPU6502::zp, &CPU6502::zp, &CPU6502::imp, &CPU6502::imm, &CPU6502::imp, &CPU6502::imm, &CPU6502::abso, &CPU6502::abso, &CPU6502::abso, &CPU6502::abso,
    &CPU6502::rel, &CPU6502::indy, &CPU6502::imp, &CPU6502::indy, &CPU6502::zpx, &CPU6502::zpx, &CPU6502::zpx, &CPU6502::zpx, &CPU6502::imp, &CPU6502::absy, &CPU6502::imp, &CPU6502::absy, &CPU6502::absx, &CPU6502::absx, &CPU6502::absx, &CPU6502::absx
};

const CPU6502::OpcodeHandler CPU6502::kOpcodeTable[256] = {
    &CPU6502::brk, &CPU6502::ora, &CPU6502::nop, &CPU6502::slo, &CPU6502::nop, &CPU6502::ora, &CPU6502::asl, &CPU6502::slo, &CPU6502::php, &CPU6502::ora, &CPU6502::asl, &CPU6502::nop, &CPU6502::nop, &CPU6502::ora, &CPU6502::asl, &CPU6502::slo,
    &CPU6502::bpl, &CPU6502::ora, &CPU6502::nop, &CPU6502::slo, &CPU6502::nop, &CPU6502::ora, &CPU6502::asl, &CPU6502::slo, &CPU6502::clc, &CPU6502::ora, &CPU6502::nop, &CPU6502::slo, &CPU6502::nop, &CPU6502::ora, &CPU6502::asl, &CPU6502::slo,
    &CPU6502::jsr, &CPU6502::_and, &CPU6502::nop, &CPU6502::rla, &CPU6502::_bit, &CPU6502::_and, &CPU6502::rol, &CPU6502::rla, &CPU6502::plp, &CPU6502::_and, &CPU6502::rol, &CPU6502::nop, &CPU6502::_bit, &CPU6502::_and, &CPU6502::rol, &CPU6502::rla,
    &CPU6502::bmi, &CPU6502::_and, &CPU6502::nop, &CPU6502::rla, &CPU6502::nop, &CPU6502::_and, &CPU6502::rol, &CPU6502::rla, &CPU6502::sec, &CPU6502::_and, &CPU6502::nop, &CPU6502::rla, &CPU6502::nop, &CPU6502::_and, &CPU6502::rol, &CPU6502::rla,
    &CPU6502::rti, &CPU6502::eor, &CPU6502::nop, &CPU6502::sre, &CPU6502::nop, &CPU6502::eor, &CPU6502::lsr, &CPU6502::sre, &CPU6502::pha, &CPU6502::eor, &CPU6502::lsr, &CPU6502::nop, &CPU6502::jmp, &CPU6502::eor, &CPU6502::lsr, &CPU6502::sre,
    &CPU6502::bvc, &CPU6502::eor, &CPU6502::nop, &CPU6502::sre, &CPU6502::nop, &CPU6502::eor, &CPU6502::lsr, &CPU6502::sre, &CPU6502::_cli, &CPU6502::eor, &CPU6502::nop, &CPU6502::sre, &CPU6502::nop, &CPU6502::eor, &CPU6502::lsr, &CPU6502::sre,
    &CPU6502::rts, &CPU6502::adc, &CPU6502::nop, &CPU6502::rra, &CPU6502::nop, &CPU6502::adc, &CPU6502::ror, &CPU6502::rra, &CPU6502::pla, &CPU6502::adc, &CPU6502::ror, &CPU6502::nop, &CPU6502::jmp, &CPU6502::adc, &CPU6502::ror, &CPU6502::rra,
    &CPU6502::bvs, &CPU6502::adc, &CPU6502::nop, &CPU6502::rra, &CPU6502::nop, &CPU6502::adc, &CPU6502::ror, &CPU6502::rra, &CPU6502::_sei, &CPU6502::adc, &CPU6502::nop, &CPU6502::rra, &CPU6502::nop, &CPU6502::adc, &CPU6502::ror, &CPU6502::rra,
    &CPU6502::nop, &CPU6502::sta, &CPU6502::nop, &CPU6502::sax, &CPU6502::sty, &CPU6502::sta, &CPU6502::stx, &CPU6502::sax, &CPU6502::dey, &CPU6502::nop, &CPU6502::txa, &CPU6502::nop, &CPU6502::sty, &CPU6502::sta, &CPU6502::stx, &CPU6502::sax,
    &CPU6502::bcc, &CPU6502::sta, &CPU6502::nop, &CPU6502::nop, &CPU6502::sty, &CPU6502::sta, &CPU6502::stx, &CPU6502::sax, &CPU6502::tya, &CPU6502::sta, &CPU6502::txs, &CPU6502::nop, &CPU6502::nop, &CPU6502::sta, &CPU6502::nop, &CPU6502::nop,
    &CPU6502::ldy, &CPU6502::lda, &CPU6502::ldx, &CPU6502::lax, &CPU6502::ldy, &CPU6502::lda, &CPU6502::ldx, &CPU6502::lax, &CPU6502::tay, &CPU6502::lda, &CPU6502::tax, &CPU6502::nop, &CPU6502::ldy, &CPU6502::lda, &CPU6502::ldx, &CPU6502::lax,
    &CPU6502::bcs, &CPU6502::lda, &CPU6502::nop, &CPU6502::lax, &CPU6502::ldy, &CPU6502::lda, &CPU6502::ldx, &CPU6502::lax, &CPU6502::clv, &CPU6502::lda, &CPU6502::tsx, &CPU6502::lax, &CPU6502::ldy, &CPU6502::lda, &CPU6502::ldx, &CPU6502::lax,
    &CPU6502::cpy, &CPU6502::cmp, &CPU6502::nop, &CPU6502::dcp, &CPU6502::cpy, &CPU6502::cmp, &CPU6502::dec, &CPU6502::dcp, &CPU6502::iny, &CPU6502::cmp, &CPU6502::dex, &CPU6502::nop, &CPU6502::cpy, &CPU6502::cmp, &CPU6502::dec, &CPU6502::dcp,
    &CPU6502::bne, &CPU6502::cmp, &CPU6502::nop, &CPU6502::dcp, &CPU6502::nop, &CPU6502::cmp, &CPU6502::dec, &CPU6502::dcp, &CPU6502::cld, &CPU6502::cmp, &CPU6502::nop, &CPU6502::dcp, &CPU6502::nop, &CPU6502::cmp, &CPU6502::dec, &CPU6502::dcp,
    &CPU6502::cpx, &CPU6502::sbc, &CPU6502::nop, &CPU6502::isb, &CPU6502::cpx, &CPU6502::sbc, &CPU6502::inc, &CPU6502::isb, &CPU6502::inx, &CPU6502::sbc, &CPU6502::nop, &CPU6502::sbc, &CPU6502::cpx, &CPU6502::sbc, &CPU6502::inc, &CPU6502::isb,
    &CPU6502::beq, &CPU6502::sbc, &CPU6502::nop, &CPU6502::isb, &CPU6502::nop, &CPU6502::sbc, &CPU6502::inc, &CPU6502::isb, &CPU6502::sed, &CPU6502::sbc, &CPU6502::nop, &CPU6502::isb, &CPU6502::nop, &CPU6502::sbc, &CPU6502::inc, &CPU6502::isb
};

const uint8_t CPU6502::kTickTable[256] = {
    7, 6, 2, 8, 3, 3, 5, 5, 3, 2, 2, 2, 4, 4, 6, 6,
    2, 5, 2, 8, 4, 4, 6, 6, 2, 4, 2, 7, 4, 4, 7, 7,
    6, 6, 2, 8, 3, 3, 5, 5, 4, 2, 2, 2, 4, 4, 6, 6,
    2, 5, 2, 8, 4, 4, 6, 6, 2, 4, 2, 7, 4, 4, 7, 7,
    6, 6, 2, 8, 3, 3, 5, 5, 3, 2, 2, 2, 3, 4, 6, 6,
    2, 5, 2, 8, 4, 4, 6, 6, 2, 4, 2, 7, 4, 4, 7, 7,
    6, 6, 2, 8, 3, 3, 5, 5, 4, 2, 2, 2, 5, 4, 6, 6,
    2, 5, 2, 8, 4, 4, 6, 6, 2, 4, 2, 7, 4, 4, 7, 7,
    2, 6, 2, 6, 3, 3, 3, 3, 2, 2, 2, 2, 4, 4, 4, 4,
    2, 6, 2, 6, 4, 4, 4, 4, 2, 5, 2, 5, 5, 5, 5, 5,
    2, 6, 2, 6, 3, 3, 3, 3, 2, 2, 2, 2, 4, 4, 4, 4,
    2, 5, 2, 5, 4, 4, 4, 4, 2, 4, 2, 4, 4, 4, 4, 4,
    2, 6, 2, 8, 3, 3, 5, 5, 2, 2, 2, 2, 4, 4, 6, 6,
    2, 5, 2, 8, 4, 4, 6, 6, 2, 4, 2, 7, 4, 4, 7, 7,
    2, 6, 2, 8, 3, 3, 5, 5, 2, 2, 2, 2, 4, 4, 6, 6,
    2, 5, 2, 8, 4, 4, 6, 6, 2, 4, 2, 7, 4, 4, 7, 7
};
