#pragma once

#include <atomic>
#include <cstdint>

class DisplayMux {
public:
    static constexpr std::size_t kDigitCount = 6;
    static constexpr std::size_t kSegmentCount = 7;

    DisplayMux() = default;

    void setSegments(uint8_t segments) {
        const uint8_t masked = segments & 0x7FU;
        segment_pattern_.store(static_cast<uint8_t>((0x7FU ^ masked) & 0x7FU), std::memory_order_release);
    }

    // digit_index debe ser ya la posicion fisica del digito (0..5),
    // NO la linea cruda del 7442. Ver JuniorMachine::write() para el mapeo.
    void setSelectedDigit(uint8_t digit_index) {
        selected_digit_.store(digit_index, std::memory_order_release);
        digit_valid_.store(digit_index < kDigitCount, std::memory_order_release);
    }

    // Se debe llamar cuando el 7442 esta en una linea que no corresponde
    // a ningun digito (filas de teclado, "apagado", o BCD invalido).
    // Sin esto, un consumidor externo (p.ej. el snapshot para el
    // websocket) puede quedarse pegado a un digito que ya no esta
    // realmente seleccionado en el hardware.
    void clearSelectedDigit() {
        digit_valid_.store(false, std::memory_order_release);
    }

    uint8_t segmentPattern() const noexcept {
        return segment_pattern_.load(std::memory_order_acquire);
    }

    uint8_t selectedDigit() const noexcept {
        return selected_digit_.load(std::memory_order_acquire);
    }

    bool digitSelected() const noexcept {
        return digit_valid_.load(std::memory_order_acquire);
    }

    struct DigitState {
        uint8_t digit_index = 0;
        uint8_t segments = 0;
        bool valid = false;
    };

    DigitState current() const noexcept {
        return { 
            selected_digit_.load(std::memory_order_acquire), 
            segment_pattern_.load(std::memory_order_acquire), 
            digit_valid_.load(std::memory_order_acquire) 
        };
    }

private:
    std::atomic<uint8_t> segment_pattern_{0};
    std::atomic<uint8_t> selected_digit_{0};
    std::atomic<bool> digit_valid_{false};
};
