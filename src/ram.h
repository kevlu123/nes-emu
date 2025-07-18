#pragma once
#include "pch.h"
#include "bus.h"

namespace nes
{
    struct ram_t
    {
        static constexpr int RAM_SIZE = 0x800;

        ram_t();
        void reset();
        bool read(uint16_t addr, uint8_t& value, bool readonly);
        bool write(uint16_t addr, uint8_t value);

        uint8_t data[RAM_SIZE];
    };
}
