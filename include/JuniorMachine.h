#pragma once

#include <array>
#include <atomic>
#include <cstddef>
#include <cstdint>

#include "CPU6502.h"
#include "Decoder7442.h"
#include "DisplayMux.h"
#include "JuniorExternal.h"
#include "JuniorKeyboard.h"
#include "Machine.h"
#include "RIOT6532.h"
#include "junior_rom.h"

class JuniorMachine : public Machine {
public:
    static constexpr uint16_t kRamStart = 0x0000;
    static constexpr uint16_t kRamEnd = 0x03FF;
    static constexpr uint16_t kRiotStart = 0x1A00;
    static constexpr uint16_t kRiotEnd = 0x1BFF;
    static constexpr uint16_t kMonitorStart = 0x1C00;
    static constexpr uint16_t kMonitorEnd = 0x1FFF;
    static constexpr std::size_t kRamSize = 1024;

    JuniorMachine();
    explicit JuniorMachine(const uint8_t* monitor_rom, std::size_t monitor_rom_size);
    ~JuniorMachine() override = default;

    uint8_t read(uint16_t address) override;
    void write(uint16_t address, uint8_t value) override;
    void reset() override;
    void tick() override;

    CPU6502& cpu() noexcept { return cpu_; }
    const CPU6502& cpu() const noexcept { return cpu_; }

    RIOT6532& riot() noexcept { return riot_; }
    const RIOT6532& riot() const noexcept { return riot_; }

    Decoder7442& decoder7442() noexcept { return decoder7442_; }
    const Decoder7442& decoder7442() const noexcept { return decoder7442_; }

    DisplayMux& display() noexcept { return display_; }
    const DisplayMux& display() const noexcept { return display_; }

    JuniorKeyboard& keyboard() noexcept { return external_.keyboard(); }
    const JuniorKeyboard& keyboard() const noexcept { return external_.keyboard(); }

    JuniorExternal& external() noexcept { return external_; }
    const JuniorExternal& external() const noexcept { return external_; }

    std::array<uint8_t, kRamSize>& ram() noexcept { return ram_; }
    const std::array<uint8_t, kRamSize>& ram() const noexcept { return ram_; }

    // Traza de accesos a PA/PB/DDRA/DDRB del RIOT (registro, PC, opcode y
    // registros de CPU). En build nativo sale por std::cout; en el
    // firmware ESP32 sale por Serial.printf. Por defecto activa (igual
    // que el comportamiento previo, que era std::cout incondicional);
    // ESP32WebWrapper la desactiva al arrancar para no inundar el
    // puerto serie ni ralentizar el bucle, y la reactiva bajo demanda.
    // Se lee desde la tarea de CPU (machineTask) y se escribe desde el
    // hilo del WebSocket (KEY_TRACE) - std::atomic para visibilidad
    // correcta entre nucleos sin necesidad de deshabilitar
    // interrupciones (ver ESP32WebWrapper::runCpuBatch()).
    void setTraceEnabled(bool enabled) noexcept { trace_enabled_.store(enabled, std::memory_order_release); }
    bool traceEnabled() const noexcept { return trace_enabled_.load(std::memory_order_acquire); }

private:
    void traceRiotAccess(const char* direction, uint16_t offset, uint8_t value) const;

    CPU6502 cpu_;
    RIOT6532 riot_;
    Decoder7442 decoder7442_;
    DisplayMux display_;
    JuniorExternal external_;
    std::array<uint8_t, kRamSize> ram_{};
    const uint8_t* monitor_rom_;
    std::size_t monitor_rom_size_;
    std::atomic<bool> trace_enabled_{true};
};
