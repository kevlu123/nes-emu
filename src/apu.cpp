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
        value = LENGTH_LUT[index];
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

    pulse_channel_t::pulse_channel_t(bool is_pulse2)
        : sweep{},
          constant_volume(0),
          volume(0),
          duty(0),
          sequencer(0),
          timer_period(0),
          timer(0)
    {
        sweep.is_pulse2 = is_pulse2;
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

    void pulse_channel_t::clock_sweep()
    {
        uint16_t target_period;
        if (sweep.divider == 0 &&
            sweep.enabled &&
            sweep.shift > 0 &&
            !is_sweeper_muting(&target_period))
        {
            timer_period = target_period;
        }

        if (sweep.divider == 0 || sweep.reload)
        {
            sweep.divider = sweep.period;
            sweep.reload = false;
        }
        else
        {
            sweep.divider--;
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
            sweep.shift = value & 0x07;
            sweep.negate = value & 0x08;
            sweep.period = (value >> 4) & 0x07;
            sweep.enabled = value & 0x80;
            sweep.reload = true;
            break;
        case 2:
            timer_period &= 0xFF00;
            timer_period |= value;
            break;
        case 3:
            length_counter.load(value >> 3);
            timer_period &= 0x00FF;
            timer_period |= (value & 7) << 8;
            timer = timer_period;
            sequencer = 0;
            envelope.start_flag = true;
            break;
        }
    }

    uint8_t pulse_channel_t::get_sample() const
    {
        if (!debug.enabled ||
            length_counter.value == 0 ||
            is_sweeper_muting() ||
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

    bool pulse_channel_t::is_sweeper_muting(uint16_t *target_period) const
    {
        int tp;
        uint16_t change_amount = timer_period >> sweep.shift;
        if (sweep.negate)
        {
            if (sweep.is_pulse2)
            {
                tp = timer_period + (-change_amount - 1);
            }
            else
            {
                tp = timer_period - change_amount;
            }
            if (tp < 0)
            {
                tp = 0;
            }
        }
        else
        {
            tp = timer_period + change_amount;
        }
        if (target_period)
        {
            *target_period = tp;
        }
        return timer_period < 8 || tp > 0x7FF;
    }

    triangle_channel_t::triangle_channel_t()
        : reload_flag(false),
          control_flag(false),
          linear_counter(0),
          linear_counter_reload(0),
          timer_period(0),
          timer(0),
          sequencer(0)
    {
    }

    void triangle_channel_t::clock()
    {
        timer--;
        if (timer == 0xFFFF)
        {
            timer = timer_period;
            if (is_running())
            {
                sequencer = (sequencer + 1) % 32;
            }
        }
    }

    void triangle_channel_t::write(uint16_t addr, uint8_t value)
    {
        switch (addr)
        {
        case 0x4008:
            length_counter.halt = control_flag = value & 0x80;
            linear_counter_reload = value & 0x7F;
            break;
        case 0x400A:
            timer_period &= 0xFF00;
            timer_period |= value;
            break;
        case 0x400B:
            length_counter.load(value >> 3);
            timer_period &= 0x00FF;
            timer_period |= (value & 7) << 8;
            reload_flag = true;
            break;
        }
    }

    uint8_t triangle_channel_t::get_sample() const
    {
        if (!is_running() && debug.output_zero_when_stopped)
        {
            return 0;
        }
        else
        {
            return SEQUENCE_LUT[sequencer];
        }
    }

    bool triangle_channel_t::is_running() const
    {
        return debug.enabled && timer_period >= 2 && linear_counter > 0 && length_counter.value > 0;
    }

    noise_channel_t::noise_channel_t()
        : shift_register(1),
          constant_volume(false),
          volume(0),
          mode(false),
          timer_period(2),
          timer(1)
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
            timer_period = TIMER_LUT[value & 0x0F];
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
        if (!debug.enabled ||
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

    dmc_channel_t::dmc_channel_t(bus_t& cpu_bus)
        : cpu_bus(&cpu_bus),
          sample_buffer_full(false),
          sample_buffer(0),
          sample_address(0),
          sample_length(0),
          loop(false),
          interrupt_enabled(false),
          dma_addr(0x8000),
          dma_remaining(0),
          shift_register(0),
          shift_register_bit_count(0),
          silence_flag(false),
          output_level(0),
          timer_period(214),
          timer(1),
          irq(false)
    {
    }

    void dmc_channel_t::clock()
    {
        timer--;
        if (timer == 0)
        {
            timer = timer_period;

            if (!silence_flag)
            {
                if ((shift_register & 1) == 0 && output_level >= 2)
                {
                    output_level -= 2;
                }
                else if ((shift_register & 1) == 1 && output_level <= 125)
                {
                    output_level += 2;
                }
            }

            shift_register >>= 1;
            shift_register_bit_count--;

            // Output cycle end
            if (shift_register_bit_count == 0)
            {
                shift_register_bit_count = 8;
                silence_flag = !sample_buffer_full;
                if (sample_buffer_full)
                {
                    shift_register = sample_buffer;
                    sample_buffer_full = false;
                }
            }
        }

        // DMA
        if (!sample_buffer_full && dma_remaining > 0)
        {
            sample_buffer = cpu_bus->read(dma_addr, sample_buffer);
            sample_buffer_full = true;
            dma_addr = (dma_addr + 1) | 0x8000;
            dma_remaining--;
            if (dma_remaining == 0)
            {
                if (loop)
                {
                    start();
                }
                else if (interrupt_enabled)
                {
                    irq = true;
                }
            }
        }
    }

    void dmc_channel_t::write(uint16_t addr, uint8_t value)
    {
        switch (addr)
        {
        case 0x4010:
            timer_period = TIMER_LUT[value & 0x0F];
            loop = value & 0x40;
            interrupt_enabled = value & 0x80;
            if (!interrupt_enabled)
            {
                irq = false;
            }
            break;
        case 0x4011:
            output_level = value & 0x7F;
            break;
        case 0x4012:
            sample_address = 0xC000 + (value * 64);
            break;
        case 0x4013:
            sample_length = (value * 16) + 1;
            break;
        }
    }

    void dmc_channel_t::start()
    {
        dma_addr = sample_address;
        dma_remaining = sample_length;
    }

    uint8_t dmc_channel_t::get_sample() const
    {
        return debug.enabled ? output_level : 0;
    }

    apu_t::apu_t(bus_t& cpu_bus)
        : pulse1(false),
          pulse2(true),
          dmc(cpu_bus),
          pulse1_enabled(false),
          pulse2_enabled(false),
          triangle_enabled(false),
          noise_enabled(false),
          even_clock(false),
          clock_sequencer(0),
          frame_counter_interrupt_inhibit(false),
          frame_counter_sequence_mode(false),
          frame_irq(false),
          cpu_bus(&cpu_bus)
    {
    }

    void apu_t::reset()
    {
        auto pulse1_debug = pulse1.debug.enabled;
        auto pulse2_debug = pulse2.debug.enabled;
        auto triangle_debug = triangle.debug.enabled;
        auto noise_debug = noise.debug.enabled;
        auto dmc_debug = dmc.debug.enabled;
        auto apu_debug = debug;
        *this = apu_t(*cpu_bus);
        pulse1.debug.enabled = pulse1_debug;
        pulse2.debug.enabled = pulse2_debug;
        triangle.debug.enabled = triangle_debug;
        noise.debug.enabled = noise_debug;
        dmc.debug.enabled = dmc_debug;
        debug = apu_debug;
    }

    bool apu_t::read(uint16_t addr, uint8_t& value, bool allow_side_effects)
    {
        if (addr == 0x4015)
        {
            value = (0x01 * (pulse1.length_counter.value > 0 && pulse1_enabled))
                  | (0x02 * (pulse2.length_counter.value > 0 && pulse2_enabled))
                  | (0x04 * (triangle.length_counter.value > 0 && triangle_enabled))
                  | (0x08 * (noise.length_counter.value > 0 && noise_enabled))
                  | (0x10 * (dmc.dma_remaining > 0))
                  | (0x40 * frame_irq)
                  | (0x80 * dmc.irq);
            if (allow_side_effects)
            {
                frame_irq = false;
            }
            return true;
        }
        return false;
    }

    bool apu_t::write(uint16_t addr, uint8_t value)
    {
        switch (addr)
        {
        case 0x4009:
        case 0x400D:
            // Unused registers
            return true;
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
            triangle.write(addr, value);
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
            dmc.write(addr, value);
            return true;
        case 0x4015:
            pulse1_enabled = value & 0x01;
            pulse2_enabled = value & 0x02;
            triangle_enabled = value & 0x04;
            noise_enabled = value & 0x08;
            if (value & 0x10)
            {
                dmc.start();
            }
            else
            {
                dmc.dma_remaining = 0;
            }
            return true;
        case 0x4017:
            frame_counter_interrupt_inhibit = value & 0x40;
            frame_counter_sequence_mode = value & 0x80;
            if (frame_counter_interrupt_inhibit)
            {
                frame_irq = false;
            }
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
            else if (frame_counter_sequence_mode == 0 && clock_sequencer >= 14914)
            {
                clock_quarter_frame();
                clock_half_frame();
                clock_sequencer = 0xFFFF;
                if (!frame_counter_interrupt_inhibit)
                {
                    frame_irq = true;
                }
            }
            else if (frame_counter_sequence_mode == 1 && clock_sequencer >= 18640)
            {
                clock_quarter_frame();
                clock_half_frame();
                clock_sequencer = 0xFFFF;
            }
        }
    }

    void apu_t::clock_quarter_frame()
    {
        pulse1.envelope.clock();
        pulse2.envelope.clock();
        noise.envelope.clock();

        if (triangle.reload_flag)
        {
            triangle.linear_counter = triangle.linear_counter_reload;
        }
        else if (triangle.linear_counter > 0)
        {
            triangle.linear_counter--;
        }
        if (!triangle.control_flag)
        {
            triangle.reload_flag = false;
        }
    }

    void apu_t::clock_half_frame()
    {
        pulse1.length_counter.clock();
        pulse1.clock_sweep();
        pulse2.length_counter.clock();
        pulse2.clock_sweep();
        triangle.length_counter.clock();
        noise.length_counter.clock();
    }

    float apu_t::get_mixed_sample() const
    {
        if (!debug.enabled)
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

        float mixed = pulse_out + tnd_out;
        return mixed > 1.0f ? 1.0f : mixed;
    }
}
