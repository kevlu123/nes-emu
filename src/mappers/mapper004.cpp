#include "pch.h"
#include "mapper004.h"
#include "cart.h"

namespace nes
{
    mapper004_t::mapper004_t(cart_t& cart)
        : mapper_t(cart),
          bank_select{},
          map_regs{},
          irq_latch(0),
          irq_counter(0),
          irq_enabled(0),
          irq_reload(false)
    {
    }

    MAPPER_DEFINE_RESET(mapper004_t);
    MAPPER_DEFINE_NAME(mapper004_t, "MMC3");

    bool mapper004_t::cpu_read(uint16_t addr, uint8_t& value, bool readonly)
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

    bool mapper004_t::cpu_write(uint16_t addr, uint8_t value)
    {
        if (addr >= 0x6000 && addr < 0x8000)
        {
            prg_ram[addr - 0x6000] = value;
        }
        else if (addr >= 0x8000 && addr <= 0x9FFF)
        {
            if (addr % 2 == 0)
            {
                // Bank select
                bank_select.reg = value;
            }
            else
            {
                // Bank data
                map_regs[bank_select.select] = value;
            }
        }
        else if (addr >= 0xA000 && addr <= 0xBFFF)
        {
            if (addr % 2 == 0)
            {
                // Mirroring
                mirroring = (value & 1) ? mirroring_t::vertical : mirroring_t::horizontal;
            }
            else
            {
                // PRG RAM protect (not implemented)
            }
        }
        else if (addr >= 0xC000 && addr <= 0xDFFF)
        {
            if (addr % 2 == 0)
            {
                irq_latch = value;
            }
            else
            {
                irq_reload = true;
            }
        }
        else if (addr >= 0xE000 && addr <= 0xFFFF)
        {
            if (addr % 2 == 0)
            {
                irq_enabled = false;
                irq = false;
            }
            else
            {
                irq_enabled = true;
            }
        }
        else
        {
            return false;
        }
        return true;
    }

    bool mapper004_t::ppu_read(uint16_t addr, uint8_t& value, bool readonly)
    {
        if (addr < 0x2000)
        {
            size_t mapped = map_chr(addr);
            value = cart->chr[mapped % cart->chr.size()];
            return true;
        }
        return false;
    }

    bool mapper004_t::ppu_write(uint16_t addr, uint8_t value)
    {
        if (addr < 0x2000)
        {
            size_t mapped = map_chr(addr);
            cart->chr[mapped % cart->chr.size()] = value;
            return true;
        }
        return false;
    }

    void mapper004_t::on_scanline()
    {
        if (irq_counter == 0 || irq_reload)
        {
            irq_counter = irq_latch;
            irq_reload = false;
        }
        else
        {
            irq_counter--;
        }

        if (irq_counter == 0 && irq_enabled)
        {
            irq = true;
        }
    }

    size_t mapper004_t::map_prg(uint16_t addr)
    {
        size_t slot = (addr - 0x8000) / 0x2000;
        size_t bank = 0;
        if (bank_select.prg_mode == 0)
        {
            switch (slot)
            {
                case 0: bank = map_regs[6] & 0x3F; break;
                case 1: bank = map_regs[7] & 0x3F; break;
                case 2: bank = get_prg_bank_count(0x2000) - 2; break;
                case 3: bank = get_prg_bank_count(0x2000) - 1; break;
            }
        }
        else
        {
            switch (slot)
            {
                case 0: bank = get_prg_bank_count(0x2000) - 2; break;
                case 1: bank = map_regs[7] & 0x3F; break;
                case 2: bank = map_regs[6] & 0x3F; break;
                case 3: bank = get_prg_bank_count(0x2000) - 1; break;
            }
        }
        return (bank * 0x2000) + (addr % 0x2000);
    }

    size_t mapper004_t::map_chr(uint16_t addr)
    {
        size_t slot = addr / 0x400;
        size_t bank = 0;
        if (bank_select.chr_mode == 0)
        {
            switch (slot)
            {
                case 0: bank = (map_regs[0] & 0xFE);     break;
                case 1: bank = (map_regs[0] & 0xFE) + 1; break;
                case 2: bank = (map_regs[1] & 0xFE);     break;
                case 3: bank = (map_regs[1] & 0xFE) + 1; break;
                case 4: bank = map_regs[2]; break;
                case 5: bank = map_regs[3]; break;
                case 6: bank = map_regs[4]; break;
                case 7: bank = map_regs[5]; break;
            }
        }
        else
        {
            switch (slot)
            {
                case 0: bank = map_regs[2]; break;
                case 1: bank = map_regs[3]; break;
                case 2: bank = map_regs[4]; break;
                case 3: bank = map_regs[5]; break;
                case 4: bank = (map_regs[0] & 0xFE);     break;
                case 5: bank = (map_regs[0] & 0xFE) + 1; break;
                case 6: bank = (map_regs[1] & 0xFE);     break;
                case 7: bank = (map_regs[1] & 0xFE) + 1; break;
            }
        }
        return (bank * 0x400) + (addr % 0x400);
    }
}
