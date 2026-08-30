#pragma once

#include <cstdint>

class Machine {
public:
    virtual ~Machine() = default;

    virtual uint8_t read(uint16_t address) = 0;
    virtual void write(uint16_t address, uint8_t value) = 0;
    virtual void reset() = 0;
    virtual void tick() = 0;
};
