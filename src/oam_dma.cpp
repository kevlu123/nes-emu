#include "pch.h"
#include "oam_dma.h"

namespace nes
{
    oam_dma_t::oam_dma_t(bus_t& cpu_bus)
        : cpu_bus(&cpu_bus),
          page(0),
          cycles_remaining(0)
    {
    }

    oam_dma_t::~oam_dma_t()
    {
    }

    void oam_dma_t::reset()
    {
        *this = oam_dma_t(*cpu_bus);
    }

    void oam_dma_t::copy(uint8_t page)
    {
        this->page = page;
        cycles_remaining = 513;
    }

    void oam_dma_t::clock()
    {
        if (cycles_remaining > 0)
        {
            cycles_remaining--;
            if (cycles_remaining == 512)
            {
                return;
            }

            if (cycles_remaining % 2 == 1)
            {
                uint8_t offset = (511 - cycles_remaining) / 2;
                last_read = cpu_bus->read((page << 8) | offset);
            }
            else
            {
                cpu_bus->write(0x2004, last_read);
            }
        }
    }
}
