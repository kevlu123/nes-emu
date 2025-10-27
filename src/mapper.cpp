#include "pch.h"
#include "mapper.h"
#include "cart.h"

namespace nes
{
    mapper_t::mapper_t(cart_t& cart)
        : cart(&cart),
          mirroring(cart.header.mirroring)
    {
    }

    bool mapper_t::cpu_read(uint16_t addr, uint8_t& value, bool readonly)
    {
        if (addr >= 0x8000)
        {
            value = cart->prg_rom[(addr - 0x8000) % cart->prg_rom.size()];
            return true;
        }
        return false;
    }

    bool mapper_t::cpu_write(uint16_t addr, uint8_t value)
    {
        return false;
    }

    bool mapper_t::ppu_read(uint16_t addr, uint8_t& value, bool readonly)
    {
        if (addr < 0x2000)
        {
            value = cart->chr[addr];
            return true;
        }
        return false;
    }

    bool mapper_t::ppu_write(uint16_t addr, uint8_t value)
    {
        if (addr < 0x2000 && addr < cart->chr_ram.size())
        {
            cart->chr_ram[addr] = value;
            return true;
        }
        return false;
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
