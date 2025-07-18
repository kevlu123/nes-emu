#pragma once
#include "pch.h"
#include "bus.h"

namespace nes
{
    struct controller_t
    {
        controller_t();
        void reset();
        bool read(uint16_t addr, uint8_t& value, bool readonly);
        bool write(uint16_t addr, uint8_t value);
    };
}
