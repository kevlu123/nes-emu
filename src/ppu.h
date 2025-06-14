#pragma once
#include "pch.h"
#include "bus.h"

namespace nes
{
    struct ppu_t
    {
        ppu_t(bus_t& ppu_bus);
        void reset();
        bool cpu_read(uint16_t addr, uint8_t& value);
        bool cpu_write(uint16_t addr, uint8_t value);
        bool ppu_read(uint16_t addr, uint8_t& value);
        bool ppu_write(uint16_t addr, uint8_t value);
    private:
        bus_t& ppu_bus;
    };
}
