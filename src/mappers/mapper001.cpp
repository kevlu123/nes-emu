#include "pch.h"
#include "mapper001.h"
#include "cart.h"

namespace nes
{
    mapper001_t::mapper001_t(cart_t& cart)
        : mapper_t(cart),
          shift_register(0x10),
          control{},
          chr_bank0(0),
          chr_bank1(0),
          prg_bank(0)
    {
        control.reg = 0x0C;
    }

    MAPPER_DEFINE_RESET(mapper001_t);

    bool mapper001_t::cpu_read(uint16_t addr, uint8_t& value, bool readonly)
    {
        if (addr >= 0x6000 && addr < 0x8000)
        {
            value = prg_ram[addr - 0x6000];
            return true;
        }
        else if (addr >= 0x8000)
        {
            size_t mapped = map_prg(addr);
            value = cart->prg_rom[mapped % cart->prg_rom.size()];
            return true;
        }
        return false;
    }

    bool mapper001_t::cpu_write(uint16_t addr, uint8_t value)
    {
        if (addr >= 0x6000 && addr < 0x8000)
        {
            prg_ram[addr - 0x6000] = value;
            return true;
        }
        else if (addr >= 0x8000)
        {
            if (value & 0x80)
            {
                shift_register = 0x10;
                control.prg_bank_mode = 3;
            }
            else if ((shift_register & 1) == 0)
            {
                shift_register = (shift_register >> 1) | ((value & 1) << 4);
            }
            else
            {
                uint8_t shift_value = (shift_register >> 1) | ((value & 1) << 4);
                shift_register = 0x10;

                if (addr <= 0x9FFF)
                {
                    control.reg = shift_value;
                    switch (control.mirroring)
                    {
                    case 0: mirroring = mirroring_t::one_screen_low; break;
                    case 1: mirroring = mirroring_t::one_screen_high; break;
                    case 2: mirroring = mirroring_t::vertical; break;
                    case 3: mirroring = mirroring_t::horizontal; break;
                    }
                }
                else if (addr >= 0xA000 && addr <= 0xBFFF)
                {
                    chr_bank0 = shift_value;
                }
                else if (addr >= 0xC000 && addr <= 0xDFFF)
                {
                    chr_bank1 = shift_value;
                }
                else if (addr >= 0xE000)
                {
                    prg_bank = shift_value & 0x0F;
                }
            }
            return true;
        }
        return false;
    }

    bool mapper001_t::ppu_read(uint16_t addr, uint8_t& value, bool readonly)
    {
        if (addr < 0x2000)
        {
            size_t mapped = map_chr(addr);
            value = cart->chr[mapped % cart->chr.size()];
            return true;
        }
        return false;
    }

    bool mapper001_t::ppu_write(uint16_t addr, uint8_t value)
    {
        if (addr < 0x2000)
        {
            size_t mapped = map_chr(addr);
            cart->chr[mapped % cart->chr.size()] = value;
            return true;
        }
        return false;
    }

    size_t mapper001_t::map_prg(uint16_t addr)
    {
        if (control.prg_bank_mode <= 1)
        {
            size_t bank = prg_bank & 0x0E;
            return (bank * 0x4000) + (addr % 0x8000);
        }

        size_t bank;
        if (control.prg_bank_mode == 2)
        {
            bank = addr < 0xC000 ? 0 : prg_bank;
        }
        else
        {
            bank = addr < 0xC000 ? prg_bank : (get_prg_bank_count(0x4000) - 1);
        }
        return (bank * 0x4000) + (addr % 0x4000);
    }

    size_t mapper001_t::map_chr(uint16_t addr)
    {
        size_t bank;
        if (control.chr_bank_mode == 0)
        {
            bank = addr < 0x1000 ? (chr_bank0 & 0xFE) : ((chr_bank0 & 0xFE) + 1);
        }
        else
        {
            bank = addr < 0x1000 ? chr_bank0 : chr_bank1;
        }
        return (bank * 0x1000) + (addr % 0x1000);
    }
}
