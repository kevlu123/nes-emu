#include "pch.h"
#include "ram.h"

namespace nes
{
    ram_t::ram_t()
        : data{}
    {
    }

    void ram_t::reset()
    {
        memset(data, 0, sizeof(data));
    }

    bool ram_t::read(uint16_t addr, uint8_t& value, bool readonly)
    {
        if (addr < 0x2000)
        {
            value = data[addr & 0x7FF];
            return true;
        }
        return false;
    }

    bool ram_t::write(uint16_t addr, uint8_t value)
    {
        if (addr < 0x2000)
        {
            data[addr & 0x7FF] = value;
            return true;
        }
        return false;
    }
}
