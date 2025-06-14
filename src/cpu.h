#pragma once
#include "pch.h"
#include "bus.h"

namespace nes
{
    struct cpu_t
    {
        cpu_t(bus_t& cpu_bus, bus_t& ppu_bus);
        void reset();

    private:
        bus_t& cpu_bus;
        bus_t& ppu_bus;
    };
}
