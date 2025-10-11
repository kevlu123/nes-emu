#pragma once
#include "pch.h"
#include "bus.h"

namespace nes
{
    struct oam_dma_t
    {
        oam_dma_t(bus_t& cpu_bus);
        ~oam_dma_t();
        void reset();
        void copy(uint8_t page);
        void clock();
        uint8_t page;
        int cycles_remaining;
        uint8_t last_read;
    private:
        bus_t* cpu_bus;
    };
}
