#pragma once
#include "pch.h"

#include "ram.h"
#include "cpu.h"
#include "ppu.h"
#include "apu.h"
#include "controller.h"
#include "cart.h"
#include "oam_dma.h"

#include <optional>

namespace nes
{
    struct nes_t
    {
        nes_t();
        void reset();
        void clock();
        void clock_cpu();
        void clock_frame();
        void unload_cart();
        void load_cart(cart_t cart);

        bus_t cpu_bus;
        bus_t ppu_bus;

        ram_t ram;
        cpu_t cpu;
        ppu_t ppu;
        apu_t apu;
        oam_dma_t oam_dma;
        controller_t controller;
        std::optional<cart_t> cart;

        uint64_t ppu_clock_count;

        uint8_t screen_buffer[ppu_t::SCREEN_WIDTH * ppu_t::SCREEN_HEIGHT];
    };
}
