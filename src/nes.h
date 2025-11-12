#pragma once
#include "pch.h"

#include "ram.h"
#include "cpu.h"
#include "ppu.h"
#include "apu.h"
#include "controller.h"
#include "cart.h"
#include "oam_dma.h"

#include <memory>
#include <functional>

namespace nes
{
    struct nes_t
    {
        nes_t(std::function<void()> on_clock = []{});
        void reset();
        void clock();
        void clock_instruction();
        void clock_scanline();
        void clock_frame();
        void unload_cart();
        void load_cart(std::unique_ptr<cart_t> cart);

        bus_t cpu_bus;
        bus_t ppu_bus;
        ram_t ram;
        cpu_t cpu;
        ppu_t ppu;
        apu_t apu;
        oam_dma_t oam_dma;
        controller_t controller;
        std::unique_ptr<cart_t> cart;

        uint64_t ppu_clock_count;
        uint8_t screen_buffer[ppu_t::SCREEN_WIDTH * ppu_t::SCREEN_HEIGHT];
        std::function<void()> on_clock;
    };
}
