#pragma once

#include <atomic>
#include <array>
#include <cstdint>
#include <string>
#include <unordered_map>

#include "JuniorKeypadLayout.h"

// El estado de las teclas se escribe desde la tarea/hilo del WebSocket y
// se lee desde la tarea de CPU (machineTask), que en el ESP32 pueden
// correr en nucleos distintos. Se usa std::atomic<uint8_t> por celda en
// vez de un uint8_t normal para garantizar visibilidad entre nucleos sin
// necesidad de deshabilitar interrupciones (portENTER_CRITICAL): una
// seccion critica que abarque muchas instrucciones de CPU es peligrosa
// en cuanto haya de por medio E/S por Serial (esta puede necesitar
// interrupciones para vaciar el buffer UART), y deshabilitarlas durante
// demasiado tiempo dispara el watchdog del nucleo (Guru Meditation
// Error: Interrupt wdt timeout). Los atomics dan la misma garantia de
// visibilidad sin ese riesgo.
class JuniorKeyboard {
public:
    static constexpr std::size_t kRows = 8;
    static constexpr std::size_t kCols = 8;

    struct KeyLocation {
        uint8_t row;
        uint8_t col;
    };

    JuniorKeyboard() {
        clear();
    }

    std::pair<uint8_t, uint8_t> defaultCoordsForKey(const std::string& key_name) const {
        return layout_.coordsForKey(key_name);
    }

    void setKeyState(const std::string& key_name, bool pressed) {
        const auto [row, col] = defaultCoordsForKey(key_name);
        if (row >= kRows || col >= kCols) {
            return;
        }
        key_matrix_[row][col].store(pressed ? 1U : 0U, std::memory_order_release);
    }

    void setKeyPressed(uint8_t row, uint8_t col, bool pressed) {
        if (row >= kRows || col >= kCols) {
            return;
        }
        key_matrix_[row][col].store(pressed ? 1U : 0U, std::memory_order_release);
    }

    void clear() {
        for (auto& row : key_matrix_) {
            for (auto& cell : row) {
                cell.store(0U, std::memory_order_relaxed);
            }
        }
    }

    uint8_t scanRow(uint8_t row) const {
        if (row >= kRows) {
            return 0xFFU;
        }

        uint8_t mask = 0xFFU;
        for (uint8_t col = 0; col < kCols; ++col) {
            if (key_matrix_[row][col].load(std::memory_order_acquire) != 0U) {
                mask &= static_cast<uint8_t>(~(1U << col));
            }
        }
        return mask;
    }

    uint8_t scanRowKeyCode(uint8_t row) const {
        if (row >= kRows) {
            return 0xFFU;
        }

        for (uint8_t col = 0; col < kCols; ++col) {
            if (key_matrix_[row][col].load(std::memory_order_acquire) != 0U) {
                return static_cast<uint8_t>(0x10U + col);
            }
        }
        return 0xFFU;
    }

    uint8_t scanForSelection(uint8_t selected_row) const {
        return scanRow(selected_row);
    }

    bool isPressed(const std::string& key_name) const {
        const auto [row, col] = defaultCoordsForKey(key_name);
        if (row >= kRows || col >= kCols) {
            return false;
        }
        return key_matrix_[row][col].load(std::memory_order_acquire) != 0U;
    }

private:
    JuniorKeypadLayout layout_;
    std::array<std::array<std::atomic<uint8_t>, kCols>, kRows> key_matrix_;
};
