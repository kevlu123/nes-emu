#include "pch.h"
#include "ppu.h"

namespace nes
{
    ppu_t::ppu_t(bus_t& ppu_bus)
        : ppu_bus(ppu_bus)
    {
    }

    void ppu_t::reset()
    {
    }
    
    bool ppu_t::cpu_read(uint16_t addr, uint8_t& value)
    {
        return false;
    }

    bool ppu_t::cpu_write(uint16_t addr, uint8_t value)
    {
        return false;
    }

    bool ppu_t::ppu_read(uint16_t addr, uint8_t& value)
    {
        return false;
    }

    bool ppu_t::ppu_write(uint16_t addr, uint8_t value)
    {
        return false;
    }
}
