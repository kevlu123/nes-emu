#include "pch.h"
#include "mapper007.h"
#include "cart.h"

namespace nes
{
    mapper007_t::mapper007_t(cart_t& cart)
        : mapper_t(cart),
          bank_select(0)
    {
    }

    MAPPER_DEFINE_RESET(mapper007_t);
    MAPPER_DEFINE_NAME(mapper007_t, "AxROM");

    bool mapper007_t::cpu_read(uint16_t addr, uint8_t& value, bool readonly)
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

    bool mapper007_t::cpu_write(uint16_t addr, uint8_t value)
    {
        if (addr >= 0x6000 && addr < 0x8000)
        {
            prg_ram[addr - 0x6000] = value;
            return true;
        }
        else if (addr >= 0x8000)
        {
            bank_select = value;
            mirroring = (value & 0x10) ? mirroring_t::one_screen_low : mirroring_t::one_screen_high;
            return true;
        }
        return false;
    }

    size_t mapper007_t::map_prg(uint16_t addr)
    {
        return (bank_select * 0x8000) + (addr % 0x8000);
    }
}
