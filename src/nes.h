#pragma once
#include "pch.h"

#include "ram.h"
#include "cpu.h"
#include "ppu.h"
#include "apu.h"
#include "controller.h"
#include "cart.h"

#include <optional>

namespace nes
{
    struct nes_t
    {
        static constexpr int SCREEN_WIDTH = 256;
        static constexpr int SCREEN_HEIGHT = 240;
        static constexpr float SCREEN_ASPECT =
            (float)SCREEN_WIDTH / SCREEN_HEIGHT;

        nes_t();
        void reset();
        void clock();
        void clock_frame();
        void load_cart(cart_t cart);

        bus_t cpu_bus;
        bus_t ppu_bus;

        ram_t ram;
        cpu_t cpu;
        ppu_t ppu;
        apu_t apu;
        controller_t controller;
        std::optional<cart_t> cart;

        uint8_t screen_buffer[SCREEN_WIDTH * SCREEN_HEIGHT];
    };
}
