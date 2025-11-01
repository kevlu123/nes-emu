#pragma once
#include "pch.h"
#include "bus.h"

namespace nes
{
    struct controller_t
    {
        controller_t();
        void reset();

        bool read(uint16_t addr, uint8_t& value, bool allow_side_effects);
        bool write(uint16_t addr, uint8_t value);

        union button_state_t
        {
            struct
            {
                uint8_t a : 1;
                uint8_t b : 1;
                uint8_t select : 1;
                uint8_t start : 1;
                uint8_t up : 1;
                uint8_t down : 1;
                uint8_t left : 1;
                uint8_t right : 1;
            };
            uint8_t reg;
        };

        uint8_t status_shift_reg[2];
        button_state_t status[2];

        bool latch;
    };
}
