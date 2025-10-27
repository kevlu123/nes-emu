#include "pch.h"
#include "apu.h"

namespace nes
{
    length_counter_t::length_counter_t()
        : halt(false),
          value(0)
    {
    }

    void length_counter_t::clock()
    {
        if (value > 0 && !halt)
        {
            value--;
        }
    }

    void length_counter_t::load(uint8_t index)
    {
        value = LENGTH_COUNTER_LUT[index];
    }

    envelope_t::envelope_t()
        : start_flag(false),
          loop(false),
          divider_period(0),
          divider(0),
          decay_level_counter(0)
    {
    }

    void envelope_t::clock()
    {
        if (start_flag)
        {
            start_flag = false;
            decay_level_counter = 15;
            divider = divider_period;
            return;
        }

        if (divider > 0)
        {
            divider--;
            return;
        }

        divider = divider_period;
        if (decay_level_counter > 0)
        {
            decay_level_counter--;
            return;
        }

        if (loop)
        {
            decay_level_counter = 15;
        }
    }

    pulse_channel_t::pulse_channel_t()
        : constant_volume(0),
          volume(0),
          duty(0),
          sequencer(0),
          timer_period(0),
          timer(0),
          debug_enabled(true)
    {
    }

    void pulse_channel_t::clock()
    {
        timer--;
        if (timer == 0xFFFF)
        {
            timer = timer_period;
            sequencer = (sequencer + 1) % 8;
        }
    }

    void pulse_channel_t::write(uint16_t addr, uint8_t value)
    {
        switch (addr & 3)
        {
        case 0:
            envelope.divider_period = volume = value & 0x0F;
            constant_volume = value & 0x10;
            envelope.loop = length_counter.halt = value & 0x20;
            duty = (value >> 6) & 0x03;
            break;
        case 1:
            SPDLOG_WARN("APU pulse sweep write 0x{:02X} stubbed", value);
            break;
        case 2:
            timer_period &= 0xFF00;
            timer_period |= value;
            break;
        case 3:
            length_counter.load(value >> 3);
            timer_period &= 0x00FF;
            timer_period |= (value & 3) << 8;
            timer = timer_period;
            sequencer = 0;
            envelope.start_flag = true;
            break;
        }
    }

    uint8_t pulse_channel_t::get_sample() const
    {
        if (!debug_enabled ||
            length_counter.value == 0 ||
            timer_period < 8 ||
            !SEQUENCE_LUT[duty][sequencer])
        {
            return 0;
        }
        else if (constant_volume)
        {
            return volume;
        }
        else
        {
            return envelope.decay_level_counter;
        }
    }

    triangle_channel_t::triangle_channel_t()
        : debug_enabled(true)
    {
    }

    void triangle_channel_t::clock()
    {
    }

    uint8_t triangle_channel_t::get_sample() const
    {
        return 0;
    }

    noise_channel_t::noise_channel_t()
        : shift_register(1),
          constant_volume(false),
          volume(0),
          mode(false),
          timer_period(4),
          timer(1),
          debug_enabled(true)
    {
    }

    void noise_channel_t::clock()
    {
        timer--;
        if (timer == 0)
        {
            timer = timer_period;
            bool mode_bit = mode ? (shift_register & 0x40) : (shift_register & 2);
            bool new_bit = (shift_register & 1) ^ mode_bit;
            shift_register = (new_bit << 14) | (shift_register >> 1);
        }
    }

    void noise_channel_t::write(uint16_t addr, uint8_t value)
    {
        switch (addr)
        {
        case 0x400C:
            envelope.divider_period = volume = value & 0x0F;
            constant_volume = value & 0x10;
            length_counter.halt = value & 0x20;
            break;
        case 0x400E:
            timer_period = NOISE_TIMER_LUT[value & 0x0F];
            mode = value & 0x80;
            break;
        case 0x400F:
            length_counter.load(value >> 3);
            envelope.start_flag = true;
            break;
        }
    }

    uint8_t noise_channel_t::get_sample() const
    {
        if (!debug_enabled ||
            length_counter.value == 0 ||
            (shift_register & 1))
        {
            return 0;
        }
        else if (constant_volume)
        {
            return volume;
        }
        else
        {
            return envelope.decay_level_counter;
        }
    }

    dmc_channel_t::dmc_channel_t()
        : debug_enabled(true)
    {
    }

    void dmc_channel_t::clock()
    {
    }

    uint8_t dmc_channel_t::get_sample() const
    {
        return 0;
    }

    apu_t::apu_t(cpu_t& cpu)
        : cpu(&cpu),
          clock_sequencer(0),
          even_clock(false),
          debug_enabled(true)
    {
    }

    void apu_t::reset()
    {
        *this = apu_t(*cpu);
    }

    bool apu_t::read(uint16_t addr, uint8_t& value, bool readonly)
    {
        if (addr == 0x4015)
        {
            SPDLOG_WARN("APU read 0x4015 stubbed");
            value = 0;
            return true;
        }
        return false;
    }

    bool apu_t::write(uint16_t addr, uint8_t value)
    {
        switch (addr)
        {
        case 0x4000:
        case 0x4001:
        case 0x4002:
        case 0x4003:
            pulse1.write(addr, value);
            return true;
        case 0x4004:
        case 0x4005:
        case 0x4006:
        case 0x4007:
            pulse2.write(addr, value);
            return true;
        case 0x4008:
        case 0x400A:
        case 0x400B:
            SPDLOG_WARN("APU triangle write 0x{:02X} stubbed", value);
            return true;
        case 0x400C:
        case 0x400E:
        case 0x400F:
            noise.write(addr, value);
            return true;
        case 0x4010:
        case 0x4011:
        case 0x4012:
        case 0x4013:
            SPDLOG_WARN("APU DMC write 0x{:02X} stubbed", value);
            return true;
        case 0x4015:
            SPDLOG_WARN("APU write 0x4015 stubbed");
            return true;
        case 0x4017:
            frame_counter_ctrl.reg = value;
            return true;
        default:
            return false;
        }
    }

    void apu_t::clock()
    {
        even_clock = !even_clock;
        triangle.clock();
        if (even_clock)
        {
            pulse1.clock();
            pulse2.clock();
            noise.clock();
            dmc.clock();

            clock_sequencer++;
            if (clock_sequencer == 3728)
            {
                clock_quarter_frame();
            }
            else if (clock_sequencer == 7456)
            {
                clock_quarter_frame();
                clock_half_frame();
            }
            else if (clock_sequencer == 11185)
            {
                clock_quarter_frame();
            }
            else if (frame_counter_ctrl.sequence_mode == 0 && clock_sequencer >= 14914)
            {
                clock_quarter_frame();
                clock_half_frame();
                clock_sequencer = 0xFFFF;
                if (!frame_counter_ctrl.interrupt_inhibit)
                {
                    cpu->irq();
                }
            }
            else if (frame_counter_ctrl.sequence_mode == 1 && clock_sequencer >= 18640)
            {
                clock_quarter_frame();
                clock_half_frame    ();
                clock_sequencer = 0xFFFF;
            }
        }
    }

    void apu_t::clock_quarter_frame()
    {
        pulse1.envelope.clock();
        pulse2.envelope.clock();
        noise.envelope.clock();
    }

    void apu_t::clock_half_frame()
    {
        pulse1.length_counter.clock();
        pulse2.length_counter.clock();
        noise.length_counter.clock();
    }

    float apu_t::get_mixed_sample() const
    {
        if (!debug_enabled)
        {
            return 0.0f;
        }

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
