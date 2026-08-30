#pragma once

#include <array>
#include <cstdint>

#include "Decoder7442.h"
#include "DisplayMux.h"
#include "JuniorKeyboard.h"

class JuniorExternal {
public:
    JuniorExternal() = default;

    void setPortA(uint8_t value) {
        pa_ = value;
    }

    void setPortB(uint8_t value) {
        pb_ = value;

        // Mismo ajuste que en JuniorMachine::write(): el 7442 decodifica
        // (PB >> 1) & 0x0F, no PB directamente.
        const uint8_t bcd = static_cast<uint8_t>((value >> 1) & 0x0FU);
        decoder_.setInput(bcd);

        if (decoder_.hasActiveLine() &&
            decoder_.activeLine() >= 4 && decoder_.activeLine() <= 9) {
            display_.setSelectedDigit(static_cast<uint8_t>(decoder_.activeLine() - 4));
        } else {
            display_.clearSelectedDigit();
        }
    }

    void setDDRA(uint8_t value) {
        ddra_ = value;
    }

    uint8_t readPortA() const {
        if (ddra_ == 0x00U) {
            // Solo las lineas 0..2 del 7442 son filas de teclado.
            if (!decoder_.hasActiveLine() || decoder_.activeLine() > 2) {
                return 0xFFU;
            }
            const uint8_t row = decoder_.activeLine();

            // El monitor (rutina GETKEY/ONEKEY) hace "AND PAD" acumulando
            // el resultado de leer PA en cada fila, y luego localiza la
            // tecla contando en que posicion de bit hay un 0 (bit a
            // nivel bajo = tecla pulsada). Para eso necesita el patron
            // de columnas real (active-low), NO un codigo de tecla ya
            // traducido. scanRowKeyCode() devuelve un codigo (0x10+col)
            // pensado para otros usos, y rompe por completo el algoritmo
            // de deteccion de GETKEY: con eso el monitor nunca resuelve
            // una tecla valida y el display se queda a 0 al pulsar.
            return keyboard_.scanRow(row);
        }

        return pa_;
    }

    void setKeyPressed(uint8_t row, uint8_t col, bool pressed) {
        keyboard_.setKeyPressed(row, col, pressed);
    }

    void clearKeys() {
        keyboard_.clear();
    }

    Decoder7442& decoder() noexcept { return decoder_; }
    const Decoder7442& decoder() const noexcept { return decoder_; }

    DisplayMux& display() noexcept { return display_; }
    const DisplayMux& display() const noexcept { return display_; }

    JuniorKeyboard& keyboard() noexcept { return keyboard_; }
    const JuniorKeyboard& keyboard() const noexcept { return keyboard_; }

private:
    uint8_t pa_ = 0;
    uint8_t pb_ = 0;
    uint8_t ddra_ = 0;

    Decoder7442 decoder_;
    DisplayMux display_;
    JuniorKeyboard keyboard_;
};
