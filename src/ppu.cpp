#include "pch.h"
#include "ppu.h"

namespace nes
{
    ppu_t::ppu_t(bus_t& ppu_bus, cpu_t& cpu, uint8_t* screen_buffer)
        : ppu_bus(&ppu_bus),
          cpu(&cpu),
          screen_buffer(screen_buffer),
          ppuctrl{},
          ppumask{},
          ppustatus{},
          v_vram_addr{},
          t_vram_addr{},
          x_scroll{},
          write_toggle(false),
          ppudata_read_buffer(0),
          dot(0),
          scanline(0),
          is_first_frame(true),
          nametable_read(0),
          attribute_read(0),
          pattern_lo_read(0),
          pattern_hi_read(0),
          pattern_lo_read_shift_reg(0),
          pattern_hi_read_shift_reg(0),
          nametable{},
          palette{}
    {
        ppu_bus.connect_read<&ppu_t::ppu_read>(this);
        ppu_bus.connect_write<&ppu_t::ppu_write>(this);
    }

    ppu_t::~ppu_t()
    {
        ppu_bus->disconnect_read(this);
        ppu_bus->disconnect_write(this);
    }

    void ppu_t::reset()
    {
        *this = ppu_t(*ppu_bus, *cpu, screen_buffer);
    }
    
    bool ppu_t::cpu_read(uint16_t addr, uint8_t& value, bool readonly)
    {
        if (addr >= 0x2000 && addr <= 0x3FFF)
        {
            addr &= 0x2007;
        }

        if (addr == 0x2002)
        {
            // PPUSTATUS
            value = ppustatus.reg;
            if (!readonly)
            {
                ppustatus.vblank = 0;
                write_toggle = false;
            }
            return true;
        }
        else if (addr == 0x2004)
        {
            // OAMDATA
            SPDLOG_WARN("OAMDATA read stubbed");
            value = 0;
            return true;
        }
        else if (addr == 0x2007)
        {
            // PPUDATA
            value = ppudata_read_buffer;
            if (!readonly)
            {
                ppudata_read_buffer = ppu_bus->read(v_vram_addr.reg);
                v_vram_addr.reg += ppuctrl.vram_addr_increment ? 32 : 1;
            }
            return true;
        }
        return false;
    }

    bool ppu_t::cpu_write(uint16_t addr, uint8_t value)
    {
        if (addr >= 0x2000 && addr <= 0x3FFF)
        {
            addr &= 0x2007;
        }

        if (addr == 0x2000)
        {
            // PPUCTRL
            if (!is_first_frame)
            {
                if (ppustatus.vblank &&
                    !ppuctrl.vblank_nmi_enable && (value & 0x80) != 0)
                {
                    ppuctrl.reg = value;
                    cpu->nmi();
                }
                else
                {
                    ppuctrl.reg = value;
                }
            }
            return true;
        }
        else if (addr == 0x2001)
        {
            // PPUMASK
            if (!is_first_frame)
            {
                ppumask.reg = value;
            }
            return true;
        }
        else if (addr == 0x2003)
        {
            // OAMADDR
            SPDLOG_WARN("OAMADDR write stubbed");
        }
        else if (addr == 0x2004)
        {
            // OAMDATA
            SPDLOG_WARN("OAMDATA write stubbed");
            return true;
        }
        else if (addr == 0x2005)
        {
            // PPUSCROLL
            if (write_toggle == 0)
            {
                t_vram_addr.coarse_x_scroll = value >> 3;
                x_scroll.value = value & 0b111;
            }
            else
            {
                t_vram_addr.coarse_y_scroll = value >> 3;
                t_vram_addr.fine_y_scroll = value & 0b111;
            }
            write_toggle = !write_toggle;
            return true;
        }
        else if (addr == 0x2006)
        {
            // PPUADDR
            if (write_toggle == 0)
            {
                t_vram_addr.reg &= 0x00FF;
                t_vram_addr.reg |= (value & 0b0011'1111) << 8;
            }
            else
            {
                t_vram_addr.reg &= 0xFF00;
                t_vram_addr.reg |= value;
                v_vram_addr.reg = t_vram_addr.reg;
            }
            write_toggle = !write_toggle;
            return true;
        }
        else if (addr == 0x2007)
        {
            // PPUDATA
            ppu_bus->write(v_vram_addr.reg, value);
            v_vram_addr.reg += ppuctrl.vram_addr_increment ? 32 : 1;
            return true;
        }
        else if (addr == 0x4014)
        {
            // OAMDMA
            SPDLOG_WARN("OAMDMA write stubbed");
            return true;
        }
        return false;
    }

    bool ppu_t::ppu_read(uint16_t addr, uint8_t& value, bool readonly)
    {
        if (addr >= 0x2000 && addr <= 0x2FFF)
        {
            value = nametable[addr & 0x7FF];
            return true;
        }
        else if (addr >= 0x3F00)
        {
            value = palette[addr & 0x1F];
            return true;
        }
        return false;
    }

    bool ppu_t::ppu_write(uint16_t addr, uint8_t value)
    {
        if (addr >= 0x2000 && addr <= 0x2FFF)
        {
            nametable[addr & 0x7FF] = value;
            return true;
        }
        else if (addr >= 0x3F00)
        {
            palette[addr & 0x1F] = value;
            return true;
        }
        return false;
    }

    void ppu_t::clock()
    {
        bool rendering_enabled = ppumask.enable_bg || ppumask.enable_sprite;

        if (dot == 256 && rendering_enabled)
        {
            v_vram_addr.fine_y_scroll++;
            if (v_vram_addr.fine_y_scroll == 0)
            {
                v_vram_addr.coarse_y_scroll++;
                if (v_vram_addr.coarse_y_scroll == 30)
                {
                    v_vram_addr.nametable_select ^= 0b10;
                    v_vram_addr.coarse_y_scroll = 0;
                }
            }
        }

        if (dot == 257 && rendering_enabled)
        {
            v_vram_addr.coarse_x_scroll = t_vram_addr.coarse_x_scroll;
            v_vram_addr.nametable_select &= 0b10;
            v_vram_addr.nametable_select |= t_vram_addr.nametable_select & 0b01;
        }

        if (dot >= 280 && dot <= 304 && rendering_enabled
            && scanline == SCANLINES - 1)
        {
            v_vram_addr.coarse_y_scroll = t_vram_addr.coarse_y_scroll;
            v_vram_addr.nametable_select &= 0b01;
            v_vram_addr.nametable_select |= t_vram_addr.nametable_select & 0b10;
            v_vram_addr.fine_y_scroll = t_vram_addr.fine_y_scroll;
        }

        uint8_t pattern = 0;
        if (((dot >= 1 && dot <= 256) || (dot >= 321 && dot <= 336))
            && rendering_enabled)
        {
            uint8_t fine_x = x_scroll.value;
            pattern = ((pattern_lo_read_shift_reg >> fine_x) & 1)
                    | (((pattern_hi_read_shift_reg >> fine_x) & 1) << 1);

            pattern_lo_read_shift_reg >>= 1;
            pattern_lo_read_shift_reg |= 0x8000;
            pattern_hi_read_shift_reg >>= 1;
            pattern_hi_read_shift_reg |= 0x8000;

            if (dot >= 1 && dot <= 256 && scanline < SCREEN_HEIGHT)
            {
                screen_buffer[scanline * SCREEN_WIDTH + dot - 1] = pattern;
            }

            switch (dot % 8)
            {
            case 2:
                nametable_read = ppu_bus->read(
                    get_tile_addr(v_vram_addr.nametable_select,
                        v_vram_addr.coarse_x_scroll,
                        v_vram_addr.coarse_y_scroll));
                break;
            case 4:
                // attribute_read = ppu_bus->read(
                //     get_attribute_addr());
                break;
            case 6:
                pattern_lo_read = ppu_bus->read(
                    get_pattern_lo_addr(ppuctrl.bg_pattern_table_addr,
                        nametable_read,
                        v_vram_addr.fine_y_scroll));
                pattern_lo_read = reverse_bits(pattern_lo_read);
                break;
            case 0:
                pattern_hi_read = ppu_bus->read(
                    get_pattern_hi_addr(ppuctrl.bg_pattern_table_addr,
                        nametable_read,
                        v_vram_addr.fine_y_scroll));
                pattern_hi_read = reverse_bits(pattern_hi_read);
                pattern_lo_read_shift_reg &= 0xFF | (pattern_lo_read << 8);
                pattern_hi_read_shift_reg &= 0xFF | (pattern_hi_read << 8);
                break;
            }
        }

        if (((dot >= 8 && dot <= 256) || dot >= 328)
            && dot % 8 == 0 && rendering_enabled)
        {
            v_vram_addr.coarse_x_scroll++;
            if (v_vram_addr.coarse_x_scroll == 0)
            {
                v_vram_addr.nametable_select ^= 0b01;
            }
        }

        if (is_first_frame && dot == 0 && scanline == SCANLINES - 1)
        {
            is_first_frame = false;
        }

        if (dot == 1 && scanline == 241)
        {
            ppustatus.vblank = 1;
            if (ppuctrl.vblank_nmi_enable)
            {
                cpu->nmi();
            }
        }

        dot++;
        if (dot >= DOTS_PER_SCANLINE)
        {
            dot = 0;
            scanline++;
            if (scanline >= SCANLINES)
            {
                scanline = 0;
            }
        }
    }
}
