#pragma once
#include "pch.h"
#include "bus.h"

namespace nes
{
    struct apu_pulse_t
    {
        uint8_t get_sample() const;
    };

    struct apu_triangle_t
    {
        uint8_t get_sample() const;
    };

    struct apu_noise_t
    {
        uint8_t get_sample() const;
    };

    struct apu_dmc_t
    {
        uint8_t get_sample() const;
    };

    struct apu_t
    {
        apu_t();
        void reset();
        bool read(uint16_t addr, uint8_t& value, bool readonly);
        bool write(uint16_t addr, uint8_t value);
        void clock();
        float get_mixed_sample() const;

        apu_pulse_t pulse1;
        apu_pulse_t pulse2;
        apu_triangle_t triangle;
        apu_noise_t noise;
        apu_dmc_t dmc;
    };
}
