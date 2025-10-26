#include "pch.h"
#include "apu.h"

#include <math.h>
static uint64_t t = 0;

namespace nes
{
    uint8_t apu_pulse_t::get_sample() const
    {
        return t % 16;
    }

    uint8_t apu_triangle_t::get_sample() const
    {
        return 0;
    }

    uint8_t apu_noise_t::get_sample() const
    {
        return 0;
    }

    uint8_t apu_dmc_t::get_sample() const
    {
        return 0;
    }

    apu_t::apu_t()
    {
    }

    void apu_t::reset()
    {
    }

    bool apu_t::read(uint16_t addr, uint8_t& value, bool readonly)
    {
        if (addr >= 0x4000 && addr <= 0x4015)
        {
            // SPDLOG_WARN("APU read stubbed");
            return true;
        }
        return false;
    }

    bool apu_t::write(uint16_t addr, uint8_t value)
    {
        if (addr >= 0x4000 && addr <= 0x4017 && addr != 0x4016)
        {
            // SPDLOG_WARN("APU write stubbed");
            return true;
        }
        return false;
    }

    void apu_t::clock()
    {
        t++;
    }

    float apu_t::get_mixed_sample() const
    {
        float pulse_out = 0.0f;
        uint8_t pulse_sum = pulse1.get_sample() + pulse2.get_sample();
        if (pulse_sum > 0)
        {
            pulse_out = 95.88f / (100.0f + (8128.0f / (float)pulse_sum));
        }

        float tnd_out = 0.0f;
        if (triangle.get_sample() + noise.get_sample() + dmc.get_sample() > 0)
        {
            tnd_out = 159.79f / (100.0f + (1.0f / (
                ((float)triangle.get_sample() / 8227.0f)
                + ((float)noise.get_sample() / 12241.0f)
                + ((float)dmc.get_sample() / 22638.0f))));
        }

        return pulse_out + tnd_out;
    }
}
