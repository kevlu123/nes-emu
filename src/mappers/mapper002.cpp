#include "pch.h"
#include "mapper002.h"
#include "cart.h"

namespace nes
{
    mapper002_t::mapper002_t(cart_t& cart)
        : mapper_t(cart),
          bank_select(0)
    {
    }

    MAPPER_DEFINE_RESET(mapper002_t);

    bool mapper002_t::cpu_read(uint16_t addr, uint8_t& value, bool readonly)
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

    bool mapper002_t::cpu_write(uint16_t addr, uint8_t value)
    {
        if (addr >= 0x6000 && addr < 0x8000)
        {
            prg_ram[addr - 0x6000] = value;
            return true;
        }
        else if (addr >= 0x8000)
        {
            bank_select = value;
            return true;
        }
        return false;
    }

    size_t mapper002_t::map_prg(uint16_t addr)
    {
        if (addr < 0xC000)
        {
            return (bank_select * 0x4000) + (addr % 0x4000);
        }
        else
        {
            return cart->prg_rom.size() - 0x4000 + (addr % 0x4000);
        }
    }
}
