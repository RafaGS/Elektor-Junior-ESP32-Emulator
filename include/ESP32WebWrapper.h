#pragma once

#include <array>
#include <atomic>
#include <cstdint>

#if defined(ARDUINO)
#include <Arduino.h>
#include <ESPAsyncWebServer.h>
#include <LittleFS.h>
#include <WiFi.h>
#include <ArduinoJson.h>
#endif

class JuniorMachine;

class ESP32WebWrapper {
public:
    explicit ESP32WebWrapper(JuniorMachine* machine = nullptr);

    void begin();
    void loop();
    void broadcastDisplay();
    void maintainConnections();
    void runCpuBatch(int instruction_count);
    bool isPaused() const noexcept { return paused_.load(std::memory_order_acquire); }
    bool isDisplayEnabled() const noexcept { return display_enabled_.load(std::memory_order_acquire); }

private:
#if defined(ARDUINO)
    static ESP32WebWrapper* instance_;

    static void onWebSocketEvent(AsyncWebSocket* server,
                                AsyncWebSocketClient* client,
                                AwsEventType type,
                                void* arg,
                                uint8_t* data,
                                size_t len);

    void handleWebSocketMessage(const uint8_t* data, size_t len);
    void handleKeyInput(const char* key, bool pressed);
    void sendDisplayState();
    void writeKeyboardMatrixEntry(const char* key, bool pressed);

    AsyncWebServer server_{80};
    AsyncWebSocket ws_{"/ws"};
    std::array<uint8_t, 6> display_snapshot_{};
    std::atomic<bool> paused_{false};
    std::atomic<bool> display_enabled_{true};
#endif

    JuniorMachine* machine_ = nullptr;
};
