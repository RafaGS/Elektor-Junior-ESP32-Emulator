#pragma once

#include <array>
#include <cstdint>
#include <string>
#include <unordered_map>

// Mapa fila/columna del teclado matricial real del Elektor Junior.
//
// Solo existen 3 filas fisicas (seleccionadas via PB, BCD 0,1,2 tras
// (PB>>1)&0x0F, ver Decoder7442.h/JuniorMachine.cpp), con 7 columnas
// cada una (bits 6..0 de PA; el bit7 lo fuerza el monitor a 1 con
// ORA #$80 y no corresponde a ninguna columna real).
//
// El orden bit/columna se ha extraido directamente de UpdateKeyboard()
// en JuniorComputer/Src/MainForm.cs (emulador C# de referencia), donde
// cada tecla pone a 0 un bit concreto de RAM[0x1A80] con RAM[0x1A82]
// fijo a la fila correspondiente. El orden es DECRECIENTE: la primera
// tecla de cada fila usa el bit6, la ultima el bit0 (no coincide con
// numerar las columnas en el mismo orden que las teclas).
//
//   fila 0 (PB=0/BCD0): 0->bit6 1->bit5 2->bit4 3->bit3 4->bit2 5->bit1 6->bit0
//   fila 1 (PB=2/BCD1): 7->bit6 8->bit5 9->bit4 A->bit3 B->bit2 C->bit1 D->bit0
//   fila 2 (PB=4/BCD2): E->bit6 F->bit5 AD->bit4 DA->bit3 PL->bit2 GO->bit1 PC->bit0
//
// "col" en este mapa es directamente el numero de bit (JuniorKeyboard::
// scanRow() hace mask &= ~(1<<col)), por eso las columnas van en el
// mismo orden decreciente que los bits, no 0..6 en el orden de las teclas.
//
// RST y NMI no forman parte de esta matriz: en el hardware real van
// cableados directamente a las lineas de control de la CPU, no se leen
// via PA/PB. Por eso ESP32WebWrapper los intercepta antes de llegar
// aqui y no tienen entrada en este mapa.
class JuniorKeypadLayout {
public:
    static constexpr std::size_t kRows = 3;
    static constexpr std::size_t kCols = 7;

    JuniorKeypadLayout() {
        key_map_ = {
            // Fila 0
            {"KEY_0", {0, 6}}, {"KEY_1", {0, 5}}, {"KEY_2", {0, 4}}, {"KEY_3", {0, 3}},
            {"KEY_4", {0, 2}}, {"KEY_5", {0, 1}}, {"KEY_6", {0, 0}},
            // Fila 1
            {"KEY_7", {1, 6}}, {"KEY_8", {1, 5}}, {"KEY_9", {1, 4}}, {"KEY_A", {1, 3}},
            {"KEY_B", {1, 2}}, {"KEY_C", {1, 1}}, {"KEY_D", {1, 0}},
            // Fila 2
            {"KEY_E", {2, 6}}, {"KEY_F", {2, 5}}, {"KEY_AD", {2, 4}}, {"KEY_DA", {2, 3}},
            {"KEY_PL", {2, 2}}, {"KEY_GO", {2, 1}}, {"KEY_PC", {2, 0}},
        };
    }

    std::pair<uint8_t, uint8_t> coordsForKey(const std::string& key_name) const {
        const auto it = key_map_.find(key_name);
        if (it == key_map_.end()) {
            return {0xFFU, 0xFFU};
        }
        return it->second;
    }

    bool isValid(uint8_t row, uint8_t col) const {
        return row < kRows && col < kCols;
    }

private:
    std::unordered_map<std::string, std::pair<uint8_t, uint8_t>> key_map_;
};
