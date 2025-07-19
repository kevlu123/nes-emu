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
        if (addr >= 0x4000 && addr <= 0x4015)
        {
            // SPDLOG_WARN("APU read stubbed");
            return true;
        }
        return false;
    }

    bool apu_t::write(uint16_t addr, uint8_t value)
    {
        if (addr >= 0x4000 && addr <= 0x4017 && addr != 0x4016)
        {
            // SPDLOG_WARN("APU write stubbed");
            return true;
        }
        return false;
    }
}
