#pragma once
#include "pch.h"
#include "cpu.h"

namespace nes
{
    struct length_counter_t
    {
        static constexpr uint8_t LENGTH_LUT[32] = {
            10, 254, 20,  2, 40,  4, 80,  6, 160,  8, 60, 10, 14, 12, 26, 14,
            12,  16, 24, 18, 48, 20, 96, 22, 192, 24, 72, 26, 16, 28, 32, 30,
        };

        length_counter_t();
        void clock();
        void load(uint8_t index);

        bool halt;
        uint8_t value;
    };

    struct envelope_t
    {
        envelope_t();
        void clock();

        bool start_flag;
        bool loop;
        uint8_t divider_period;
        uint8_t divider;
        uint8_t decay_level_counter;
    };

    struct pulse_channel_t
    {
        static constexpr bool SEQUENCE_LUT[4][8] = {
			{ 0, 1, 0, 0, 0, 0, 0, 0 }, // 12.5%
			{ 0, 1, 1, 0, 0, 0, 0, 0 }, // 25%
			{ 0, 1, 1, 1, 1, 0, 0, 0 }, // 50%
			{ 1, 0, 0, 1, 1, 1, 1, 1 }, // 25% negated
        };

        pulse_channel_t(bool is_pulse2);
        void clock();
        void clock_sweep();
        void write(uint16_t addr, uint8_t value);
        uint8_t get_sample() const;
        bool is_sweeper_muting(uint16_t *target_period = nullptr) const;

        struct
        {
            bool enabled;
            uint8_t period;
            bool negate;
            uint8_t shift;
            uint8_t divider;
            bool reload;
            bool is_pulse2;
        } sweep;

        bool constant_volume;
        uint8_t volume;
        uint8_t duty;
        uint8_t sequencer;
        uint16_t timer_period;
        uint16_t timer;
        length_counter_t length_counter;
        envelope_t envelope;

        struct
        {
            bool enabled = true;
        } debug;
    };

    struct triangle_channel_t
    {
        static constexpr uint8_t SEQUENCE_LUT[32] = {
            15, 14, 13, 12, 11, 10,  9,  8,
             7,  6,  5,  4,  3,  2,  1,  0,
             0,  1,  2,  3,  4,  5,  6,  7,
             8,  9, 10, 11, 12, 13, 14, 15,
        };

        triangle_channel_t();
        void clock();
        void write(uint16_t addr, uint8_t value);
        uint8_t get_sample() const;
        bool is_running() const;

        bool reload_flag;
        bool control_flag;
        uint8_t linear_counter;
        uint8_t linear_counter_reload;
        uint16_t timer_period;
        uint16_t timer;
        uint8_t sequencer;
        length_counter_t length_counter;

        struct
        {
            bool enabled = true;
            bool output_zero_when_stopped = false;
        } debug;
    };

    struct noise_channel_t
    {
        static constexpr uint16_t TIMER_LUT[16] = {
              2,   4,   8,  16,  32,  48,   64,   80,
            101, 127, 190, 254, 381, 508, 1017, 2034,
        };

        noise_channel_t();
        void clock();
        void write(uint16_t addr, uint8_t value);
        uint8_t get_sample() const;

        uint16_t shift_register;
        bool mode;
        bool constant_volume;
        uint8_t volume;
        uint16_t timer_period;
        uint16_t timer;
        length_counter_t length_counter;
        envelope_t envelope;

        struct
        {
            bool enabled = true;
        } debug;
    };

    struct dmc_channel_t
    {
        static constexpr uint16_t TIMER_LUT[16] = {
            214, 190, 170, 160, 143, 127, 113, 107,
             95,  80,  71,  64,  53,  42,  36,  27,
        };

        dmc_channel_t(bus_t& cpu_bus);
        void clock();
        void write(uint16_t addr, uint8_t value);
        void start();
        uint8_t get_sample() const;

        bool sample_buffer_full;
        uint8_t sample_buffer;
        uint16_t sample_address;
        uint16_t sample_length;
        bool loop;
        bool interrupt_enabled;
        uint16_t dma_addr;
        uint16_t dma_remaining;
        uint8_t shift_register;
        uint8_t shift_register_bit_count;
        bool silence_flag;
        uint8_t output_level;
        uint16_t timer_period;
        uint16_t timer;
        bool irq;

        struct
        {
            bool enabled = true;
        } debug;

    private:
        bus_t* cpu_bus;
    };

    struct apu_t
    {
        apu_t(bus_t& cpu_bus);
        void reset();
        bool read(uint16_t addr, uint8_t& value, bool allow_side_effects);
        bool write(uint16_t addr, uint8_t value);
        void clock();
        void clock_quarter_frame();
        void clock_half_frame();
        float get_mixed_sample() const;

        pulse_channel_t pulse1;
        pulse_channel_t pulse2;
        triangle_channel_t triangle;
        noise_channel_t noise;
        dmc_channel_t dmc;
        bool pulse1_enabled;
        bool pulse2_enabled;
        bool triangle_enabled;
        bool noise_enabled;

        bool even_clock;
        uint16_t clock_sequencer;
        bool frame_counter_interrupt_inhibit;
        bool frame_counter_sequence_mode;
        bool frame_irq;

        struct
        {
            bool enabled = true;
        } debug;

    private:
        bus_t* cpu_bus;
    };
}
