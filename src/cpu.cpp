#include "pch.h"
#include "cpu.h"

namespace nes
{
    cpu_t::cpu_t(bus_t &cpu_bus, bus_t &ppu_bus) :
        cpu_bus(cpu_bus),
        ppu_bus(ppu_bus)
    {
    }
}
