
#include "pch.h"
#include "nes.h"

namespace nes
{
    nes_t::nes_t() :
        cpu(cpu_bus, ppu_bus),
        ppu(ppu_bus)
    {
        cpu_bus.connect_read<&ram_t::read>(ram);
        cpu_bus.connect_write<&ram_t::write>(ram);
        cpu_bus.connect_read<&ppu_t::cpu_read>(ppu);
        cpu_bus.connect_write<&ppu_t::cpu_write>(ppu);
        cpu_bus.connect_read<&apu_t::read>(apu);
        cpu_bus.connect_write<&apu_t::write>(apu);
        cpu_bus.connect_read<&controller_t::read>(controller);
        cpu_bus.connect_write<&controller_t::write>(controller);

        ppu_bus.connect_read<&ppu_t::ppu_read>(ppu);
        ppu_bus.connect_write<&ppu_t::ppu_write>(ppu);

        for (int y = 0; y < SCREEN_HEIGHT; y += 8)
        {
            for (int x = 0; x < SCREEN_WIDTH; x += 8)
            {
                uint8_t colour = rand() % 64;
                for (int yy = 0; yy < 8; ++yy)
                {
                    for (int xx = 0; xx < 8; ++xx)
                    {
                        int i = (y + yy) * SCREEN_WIDTH + (x + xx);
                        screen_buffer[i] = colour;
                    }
                }
            }
        }
    }

    void nes_t::clock_frame()
    {

    }
}
