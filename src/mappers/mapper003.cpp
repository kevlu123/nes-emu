#include "pch.h"
#include "mapper003.h"
#include "cart.h"

namespace nes
{
    mapper003_t::mapper003_t(cart_t& cart)
        : mapper_t(cart),
          bank_select(0)
    {
    }

    MAPPER_DEFINE_RESET(mapper003_t);
    MAPPER_DEFINE_NAME(mapper003_t, "CNROM");

    bool mapper003_t::cpu_write(uint16_t addr, uint8_t value)
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

    bool mapper003_t::ppu_read(uint16_t addr, uint8_t& value, bool readonly)
    {
        if (addr < 0x2000)
        {
            size_t mapped = map_chr(addr);
            value = cart->chr[mapped % cart->chr.size()];
            return true;
        }
        return false;
    }

    bool mapper003_t::ppu_write(uint16_t addr, uint8_t value)
    {
        if (addr < 0x2000)
        {
            size_t mapped = map_chr(addr);
            cart->chr[mapped % cart->chr.size()] = value;
            return true;
        }
        return false;
    }

    size_t mapper003_t::map_chr(uint16_t addr)
    {
        return (bank_select * 0x2000) + (addr % 0x2000);
    }
}
