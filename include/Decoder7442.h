#pragma once

#include <array>
#include <cstdint>

// Modelo del 7442 (BCD a 1-de-10) tal como lo usa el Junior real.
//
// IMPORTANTE: el codigo BCD que ve el 7442 NO es el valor crudo que la
// CPU escribe en PB. El bus real desplaza el codigo un bit: bit0 de PB
// no forma parte de las lineas de entrada A..D del 7442. Por eso el
// codigo de entrada correcto es (PB >> 1) & 0x0F, y asi es como debe
// llamarse a setInput() desde quien lea/escriba PB.
//
// Referencia (monitor.asm / UCJunior C#):
//   PB=0,2,4            -> BCD 0,1,2 -> filas de teclado (AK/ONEKEY)
//   PB=6                -> BCD 3     -> "apagado" (linea sin digito conectado)
//   PB=8,10,12,14,16,18 -> BCD 4..9  -> los 6 digitos del display
//     (address3, address2, address1, address0, data1, data0)
//
// Para codigos BCD invalidos (10-15) el 7442 real no activa ninguna
// salida: eso es justo lo que el monitor aprovecha para "apagar" lineas.
class Decoder7442 {
public:
    static constexpr std::size_t kLineCount = 10;
    static constexpr uint8_t kNoLine = 0xFFU;

    Decoder7442() = default;

    // code debe venir YA como BCD de 4 bits, es decir (PB >> 1) & 0x0F.
    // No pasar aqui el valor crudo de PB.
    void setInput(uint8_t code) {
        input_code_ = code & 0x0FU;
        outputs_.fill(0U);

        if (input_code_ < kLineCount) {
            active_line_ = input_code_;
            outputs_[active_line_] = 1U;
        } else {
            // BCD invalido (10-15): ninguna salida activa, como en el chip real.
            active_line_ = kNoLine;
        }
    }

    bool hasActiveLine() const noexcept {
        return active_line_ != kNoLine;
    }

    // Devuelve kNoLine (0xFF) si no hay linea activa (codigo BCD invalido).
    uint8_t activeLine() const noexcept {
        return active_line_;
    }

    bool output(std::size_t index) const noexcept {
        return index < outputs_.size() && outputs_[index] != 0U;
    }

private:
    uint8_t input_code_ = 0;
    uint8_t active_line_ = kNoLine;
    std::array<uint8_t, kLineCount> outputs_{};
};
