#pragma once
#include "pch.h"
#include "bus.h"

namespace nes
{
    struct apu_t
    {
        apu_t();
        void reset();
        bool read(uint16_t addr, uint8_t& value);
        bool write(uint16_t addr, uint8_t value);
    };
}
