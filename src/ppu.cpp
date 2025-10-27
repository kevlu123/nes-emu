#include "pch.h"
#include "ppu.h"
#include "cart.h"

namespace nes
{
    ppu_t::ppu_t(bus_t& ppu_bus,
                 cpu_t& cpu,
                 oam_dma_t& oam_dma,
                 uint8_t* screen_buffer)
        : ppu_bus(&ppu_bus),
          cpu(&cpu),
          oam_dma(&oam_dma),
          screen_buffer(screen_buffer),
          ppuctrl{},
          ppumask{},
          ppustatus{},
          v_vram_addr{},
          t_vram_addr{},
          x_scroll{},
          write_toggle(false),
          ppudata_read_buffer(0),
          oam_addr(0),
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
          palette{},
          oam_bytes{},
          secondary_oam_bytes{},
          secondary_oam_count(0),
          sprite_output{}
    {
        ppu_bus.connect_read<&ppu_t::ppu_read>(this);
        ppu_bus.connect_write<&ppu_t::ppu_write>(this);
        memset(palette, 0x0F, sizeof(palette));
    }

    ppu_t::~ppu_t()
    {
        ppu_bus->disconnect_read(this);
        ppu_bus->disconnect_write(this);
    }

    void ppu_t::reset()
    {
        cart_t *cart = this->cart;
        *this = ppu_t(*ppu_bus, *cpu, *oam_dma, screen_buffer);
        set_cart(cart);
    }

    void ppu_t::set_cart(cart_t* cart)
    {
        this->cart = cart;
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
            if (dot >= 1 && dot <= 64 && scanline < SCREEN_HEIGHT)
            {
                value = 0xFF;
            }
            else
            {
                value = oam_bytes[oam_addr];
            }
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
                bool vblank_nmi_enable = ppuctrl.vblank_nmi_enable;
                ppuctrl.reg = value;
                t_vram_addr.nametable_select = value & 0b11;
                if (ppustatus.vblank &&
                    !vblank_nmi_enable && ppuctrl.vblank_nmi_enable)
                {
                    cpu->nmi();
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
            oam_addr = value;
            return true;
        }
        else if (addr == 0x2004)
        {
            // OAMDATA
            oam_bytes[oam_addr] = value;
            oam_addr++;
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
            oam_dma->copy(value);
            return true;
        }
        return false;
    }

    bool ppu_t::ppu_read(uint16_t addr, uint8_t& value, bool readonly)
    {
        if (addr >= 0x2000 && addr <= 0x3EFF)
        {
            mirroring_t mirroring;
            if (cart)
            {
                mirroring = cart->mapper->mirroring;
            }
            else
            {
                SPDLOG_WARN("PPU read nametable without cart");
                mirroring = mirroring_t::horizontal;
            }
            switch (mirroring)
            {
            case mirroring_t::horizontal:
                value = nametable[(addr & 0x3FF) | ((addr & 0x800) ? 0x400 : 0)];
                break;
            case mirroring_t::vertical:
                value = nametable[addr & 0x7FF];
                break;
            default:
                SPDLOG_WARN("Mirroring {} not implemented", (int)mirroring);
                value = nametable[addr & 0x7FF];
                break;
            }
            return true;
        }
        else if (addr >= 0x3F00)
        {
            addr &= 0x1F;
            if (addr % 4 == 0)
            {
                addr = 0;
            }
            value = palette[addr & 0x1F];
            return true;
        }
        return false;
    }

    bool ppu_t::ppu_write(uint16_t addr, uint8_t value)
    {
        if (addr >= 0x2000 && addr <= 0x3EFF)
        {
            mirroring_t mirroring;
            if (cart)
            {
                mirroring = cart->mapper->mirroring;
            }
            else
            {
                SPDLOG_WARN("PPU write nametable without cart");
                mirroring = mirroring_t::horizontal;
            }
            switch (mirroring)
            {
            case mirroring_t::horizontal:
                nametable[(addr & 0x3FF) | ((addr & 0x800) ? 0x400 : 0)] = value;
                break;
            case mirroring_t::vertical:
                nametable[addr & 0x7FF] = value;
                break;
            default:
                SPDLOG_WARN("Mirroring {} not implemented", (int)mirroring);
                nametable[addr & 0x7FF] = value;
                break;
            }
            return true;
        }
        else if (addr >= 0x3F00)
        {
            addr &= 0x1F;
            if (addr % 4 == 0 && addr >= 0x10)
            {
                addr &= 0xF;
            }
            palette[addr] = value;
            return true;
        }
        return false;
    }

    void ppu_t::clock()
    {
        bool rendering_enabled = (ppumask.enable_bg || ppumask.enable_sprite)
            && (scanline < SCREEN_HEIGHT || scanline == SCANLINES - 1);

        if (dot % 2 == 0 && dot >= 1 && dot <= 64 && scanline < SCREEN_HEIGHT)
        {
            secondary_oam_bytes[dot / 2 - 1] = 0xFF;
        }

        if (dot == 257 && scanline < SCREEN_HEIGHT)
        {
            secondary_oam_count = 0;
            int sprite_size = ppuctrl.sprite_size ? 16 : 8;
            for (int n = 0; n < 64; n++)
            {
                int oam_y = oam[n].y;
                secondary_oam[secondary_oam_count].y = oam_y;
                if (oam_y < 239 && scanline >= oam_y && scanline < oam_y + sprite_size)
                {
                    secondary_oam[secondary_oam_count] = oam[n];
                    secondary_oam_count++;
                    if (secondary_oam_count >= 8)
                    {
                        for (int m = 0; n < 64; n++)
                        {
                            oam_y = oam_bytes[(n * 4) + m];
                            if (scanline >= oam_y && scanline < oam_y + sprite_size)
                            {
                                ppustatus.sprite_overflow = 1;
                                n++;
                            }
                            else
                            {
                                m = (m + 1) % 4;
                            }
                        }
                        break;
                    }
                }
            }

            uint8_t tile_mask = sprite_size == 16 ? 0xFE : 0xFF;
            std::memset(sprite_output, 0xFF, sizeof(sprite_output));
            for (int i = 0; i < secondary_oam_count; i++)
            {
                const oam_t& oam = secondary_oam[i];
                auto& output = sprite_output[i];
                output.attribute = oam.palette;
                output.priority = oam.priority;
                output.x = oam.x;
                int fine_y = scanline - oam.y;
                if (oam.flip_vertically)
                {
                    fine_y = sprite_size - fine_y - 1;
                }
                uint8_t pattern_table = sprite_size == 16 ? oam.tile_index & 1 : ppuctrl.sprite8x8_table_addr;
                if (fine_y < 8)
                {
                    output.pattern_lo = ppu_bus->read(
                        get_pattern_lo_addr(pattern_table,
                            oam.tile_index & tile_mask,
                            fine_y));
                    output.pattern_hi = ppu_bus->read(
                        get_pattern_hi_addr(pattern_table,
                            oam.tile_index & tile_mask,
                            fine_y));
                }
                else
                {
                    output.pattern_lo = ppu_bus->read(
                        get_pattern_lo_addr(pattern_table,
                            oam.tile_index + 1,
                            fine_y - 8));
                    output.pattern_hi = ppu_bus->read(
                        get_pattern_hi_addr(pattern_table,
                            oam.tile_index + 1,
                            fine_y - 8));
                }
                if (oam.flip_horizontally)
                {
                    output.pattern_lo = reverse_bits(output.pattern_lo);
                    output.pattern_hi = reverse_bits(output.pattern_hi);
                }
            }
        }

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

        if (((dot >= 1 && dot <= 256) || (dot >= 321 && dot <= 336))
            && rendering_enabled)
        {
            uint8_t bg_pattern = 0;
            uint8_t bg_attribute = 0;
            if (ppumask.enable_bg && !(ppumask.show_bg_left8 == 0 && dot <= 8))
            {
                bg_pattern = ((pattern_lo_read_shift_reg >> x_scroll.value) & 1)
                    | (((pattern_hi_read_shift_reg >> x_scroll.value) & 1) << 1);
                bg_attribute = ((attribute_lo_read_shift_reg >> x_scroll.value) & 1)
                    | (((attribute_hi_read_shift_reg >> x_scroll.value) & 1) << 1);
            }

            pattern_lo_read_shift_reg >>= 1;
            pattern_lo_read_shift_reg |= 0x8000;
            pattern_hi_read_shift_reg >>= 1;
            pattern_hi_read_shift_reg |= 0x8000;
            attribute_lo_read_shift_reg >>= 1;
            attribute_lo_read_shift_reg |= attribute_lo_latch * 0x80;
            attribute_hi_read_shift_reg >>= 1;
            attribute_hi_read_shift_reg |= attribute_hi_latch * 0x80;

            if (dot >= 1 && dot <= 256 && scanline < SCREEN_HEIGHT)
            {
                uint8_t fg_pattern = 0;
                uint8_t fg_attribute = 0;
                bool back_priority = false;
                bool found_sprite = false;
                bool sprite_zero_opaque = false;
                for (int i = 0; i < secondary_oam_count; i++)
                {
                    auto& sprite = sprite_output[i];
                    if (dot - 1 >= sprite.x && dot - 1 < sprite.x + 8)
                    {
                        int pattern = ((sprite.pattern_hi >> 7) << 1) | (sprite.pattern_lo >> 7);
                        if (!found_sprite && ppumask.enable_sprite && !(ppumask.show_sprite_left8 == 0 && dot <= 8))
                        {
                            if (i == 0)
                            {
                                sprite_zero_opaque = pattern != 0;
                            }
                            if (pattern != 0)
                            {
                                found_sprite = true;
                                back_priority = sprite.priority;
                                fg_attribute = sprite.attribute;
                                fg_pattern = pattern;
                            }
                        }
                        sprite.pattern_hi <<= 1;
                        sprite.pattern_lo <<= 1;
                    }
                }

                if (ppustatus.sprite_zero_hit == 0 && sprite_zero_opaque && bg_pattern != 0 && dot - 1 < 255)
                {
                    ppustatus.sprite_zero_hit = 1;
                }

                bool is_fg_palette = found_sprite && ((fg_pattern != 0 && !back_priority) || bg_pattern == 0);
                uint8_t final_pattern = is_fg_palette ? fg_pattern : bg_pattern;
                uint8_t final_attribute = is_fg_palette ? fg_attribute : bg_attribute;
                uint8_t colour_index = ppu_bus->read(
                    get_palette_addr(is_fg_palette,
                        final_attribute,
                        final_pattern));
                screen_buffer[scanline * SCREEN_WIDTH + dot - 1] = colour_index;
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
                attribute_read = ppu_bus->read(
                    get_attribute_addr(v_vram_addr.nametable_select,
                        v_vram_addr.coarse_x_scroll,
                        v_vram_addr.coarse_y_scroll));
                break;
            case 6:
                pattern_lo_read = ppu_bus->read(
                    get_pattern_lo_addr(ppuctrl.bg_pattern_table_addr,
                        nametable_read,
                        v_vram_addr.fine_y_scroll));
                pattern_lo_read = reverse_bits(pattern_lo_read);
                break;
            case 0:
            {
                pattern_hi_read = ppu_bus->read(
                    get_pattern_hi_addr(ppuctrl.bg_pattern_table_addr,
                        nametable_read,
                        v_vram_addr.fine_y_scroll));
                pattern_hi_read = reverse_bits(pattern_hi_read);
                pattern_lo_read_shift_reg &= 0xFF | (pattern_lo_read << 8);
                pattern_hi_read_shift_reg &= 0xFF | (pattern_hi_read << 8);

                uint8_t attribute = get_attribute(
                    attribute_read,
                    v_vram_addr.coarse_x_scroll,
                    v_vram_addr.coarse_y_scroll);
                attribute_lo_latch = attribute & 1;
                attribute_hi_latch = (attribute >> 1) & 1;
                break;
            }
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

        if (dot == 1 && scanline == SCANLINES - 1)
        {
            ppustatus.vblank = 0;
            ppustatus.sprite_zero_hit = 0;
            ppustatus.sprite_overflow = 0;
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
