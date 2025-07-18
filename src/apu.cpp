#include "pch.h"
#include "apu.h"

namespace nes
{
    apu_t::apu_t()
    {
    }

    void apu_t::reset()
    {
    }

    bool apu_t::read(uint16_t addr, uint8_t& value, bool readonly)
    {
        return false;
    }

    bool apu_t::write(uint16_t addr, uint8_t value)
    {
        return false;
    }
}
