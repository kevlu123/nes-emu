#include "pch.h"
#include "mapper.h"
#include "cart.h"

namespace nes
{
    mapper_t::mapper_t(cart_t& cart)
        : cart(&cart),
          mirroring(cart.header.mirroring),
          irq(false),
          prg_ram{}
    {
    }

    bool mapper_t::cpu_read(uint16_t addr, uint8_t& value, bool allow_side_effects)
    {
        if (addr >= 0x6000 && addr < 0x8000)
        {
            value = prg_ram[addr - 0x6000];
            return true;
        }
        else if (addr >= 0x8000)
        {
            value = cart->prg_rom[(addr - 0x8000) % cart->prg_rom.size()];
            return true;
        }
        return false;
    }

    bool mapper_t::cpu_write(uint16_t addr, uint8_t value)
    {
        if (addr >= 0x6000 && addr < 0x8000)
        {
            prg_ram[addr - 0x6000] = value;
            return true;
        }
        return false;
    }

    bool mapper_t::ppu_read(uint16_t addr, uint8_t& value, bool allow_side_effects)
    {
        if (addr < 0x2000)
        {
            value = cart->chr[addr % cart->chr.size()];
            return true;
        }
        return false;
    }

    bool mapper_t::ppu_write(uint16_t addr, uint8_t value)
    {
        if (addr < 0x2000)
        {
            cart->chr_ram[addr % cart->chr_ram.size()] = value;
            return true;
        }
        return false;
    }

    void mapper_t::on_scanline()
    {
    }

    size_t mapper_t::get_prg_bank_count(uint32_t bank_size) const
    {
        return cart->prg_rom.size() / bank_size;
    }

    size_t mapper_t::get_chr_bank_count(uint32_t bank_size) const
    {
        return cart->chr.size() / bank_size;
    }
}
