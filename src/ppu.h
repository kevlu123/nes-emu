#pragma once
#include "pch.h"
#include "bus.h"
#include "cpu.h"
#include "mirroring.h"

namespace nes
{
    struct ppu_t
    {
        static constexpr int SCREEN_WIDTH = 256;
        static constexpr int SCREEN_HEIGHT = 240;
        static constexpr float SCREEN_ASPECT =
            (float)SCREEN_WIDTH / SCREEN_HEIGHT;
        static constexpr int SCANLINES = 262;
        static constexpr int DOTS_PER_SCANLINE = 341;

        ppu_t(bus_t& ppu_bus, cpu_t& cpu, uint8_t* screen_buffer);
        ~ppu_t();
        void reset();
        bool cpu_read(uint16_t addr, uint8_t& value, bool readonly);
        bool cpu_write(uint16_t addr, uint8_t value);
        bool ppu_read(uint16_t addr, uint8_t& value, bool readonly);
        bool ppu_write(uint16_t addr, uint8_t value);
        void clock();

        static constexpr uint16_t get_nametable_addr(uint8_t nametable)
        {
            return 0x2000 + (nametable & 0b11) * 0x400;
        }

        static constexpr uint16_t get_pattern_table_addr(uint8_t pattern_table)
        {
            return (pattern_table & 0b1) * 0x1000;
        }

        static constexpr uint16_t get_attribute_table_addr(uint8_t nametable)
        {
            return get_nametable_addr(nametable) + 0x3C0;
        }

        static constexpr uint16_t get_attribute_addr(uint8_t nametable,
            uint8_t coarse_x, uint8_t coarse_y)
        {
            return get_attribute_table_addr(nametable)
                + ((coarse_y >> 2) & 0b111) * 8
                + ((coarse_x >> 2) & 0b111);
        }

        static constexpr uint16_t get_tile_addr(
            uint8_t nametable,
            uint8_t coarse_x,
            uint8_t coarse_y)
        {
            return get_nametable_addr(nametable)
                + ((coarse_y & 0b11111) << 5)
                + (coarse_x & 0b11111);
        }

        static constexpr uint16_t get_pattern_lo_addr(
            uint8_t pattern_table,
            uint8_t tile_index,
            uint8_t fine_y)
        {
            return get_pattern_table_addr(pattern_table)
                + (tile_index << 4)
                + (fine_y & 0b111);
        }

        static constexpr uint16_t get_pattern_hi_addr(
            uint8_t pattern_table,
            uint8_t tile_index,
            uint8_t fine_y)
        {
            return get_pattern_lo_addr(pattern_table, tile_index, fine_y) + 8;
        }

        static constexpr uint16_t get_palette_addr(
            uint8_t is_fg,
            uint8_t attribute,
            uint8_t pattern)
        {
            return ((is_fg & 0b1) << 4)
                + ((attribute & 0b11) << 2)
                + (pattern & 0b11)
                + 0x3F00;
        }

        static constexpr uint8_t reverse_bits(uint8_t value)
        {
            value = (value & 0xF0) >> 4 | (value & 0x0F) << 4;
            value = (value & 0xCC) >> 2 | (value & 0x33) << 2;
            return  (value & 0xAA) >> 1 | (value & 0x55) << 1;
        }

        union
        {
            struct
            {
                uint8_t base_nametable_addr : 2;
                uint8_t vram_addr_increment : 1;
                uint8_t sprite8x8_table_addr : 1;
                uint8_t bg_pattern_table_addr : 1;
                uint8_t sprite_size : 1;
                uint8_t ppu_master_slave_select : 1;
                uint8_t vblank_nmi_enable : 1;
            };
            uint8_t reg;
        } ppuctrl;

        union
        {
            struct
            {
                uint8_t greyscale : 1;
                uint8_t show_bg_left8 : 1;
                uint8_t show_sprite_left8 : 1;
                uint8_t enable_bg : 1;
                uint8_t enable_sprite : 1;
                uint8_t emphasise_red : 1;
                uint8_t emphasise_green : 1;
                uint8_t emphasise_blue : 1;
            };
            uint8_t reg;
        } ppumask;

        union
        {
            struct
            {
                uint8_t unused : 5;
                uint8_t sprite_overflow : 1;
                uint8_t sprite_zero_hit : 1;
                uint8_t vblank : 1;
            };
            uint8_t reg;
        } ppustatus;

        union
        {
            struct
            {
                uint16_t coarse_x_scroll : 5;
                uint16_t coarse_y_scroll : 5;
                uint16_t nametable_select : 2;
                uint16_t fine_y_scroll : 3;
                uint16_t unused : 1;
            };
            uint16_t reg;
        } v_vram_addr, t_vram_addr;

        union
        {
            struct
            {
                uint8_t value : 3;
                uint8_t unused : 5;
            };
        } x_scroll;

        bool write_toggle;
        uint8_t ppudata_read_buffer;

        uint8_t nametable_read;
        uint8_t attribute_read;
        uint8_t pattern_lo_read;
        uint8_t pattern_hi_read;
        uint16_t pattern_lo_read_shift_reg;
        uint16_t pattern_hi_read_shift_reg;

        int dot;
        int scanline;
        bool is_first_frame;

        mirroring_t mirroring;
        uint8_t nametable[0x800];
        uint8_t palette[0x20];

    private:
        bus_t* ppu_bus;
        cpu_t* cpu;
        uint8_t* screen_buffer;
    };
}
