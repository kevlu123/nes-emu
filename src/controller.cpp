#include "pch.h"
#include "controller.h"

namespace nes
{
    controller_t::controller_t()
        : status{},
          status_shift_reg{ 0xFF, 0xFF }
    {
    }

    void controller_t::reset()
    {
        *this = controller_t();
    }

    bool controller_t::read(uint16_t addr, uint8_t& value, bool allow_side_effects)
    {
        if (addr == 0x4016)
        {
            value = (status_shift_reg[0] & 1) | 0x40;
            if (allow_side_effects)
            {
                status_shift_reg[0] = (status_shift_reg[0] >> 1) | 0x80;
            }
            return true;
        }
        else if (addr == 0x4017)
        {
            value = (status_shift_reg[1] & 1) | 0x40;
            if (allow_side_effects)
            {
                status_shift_reg[1] = (status_shift_reg[1] >> 1) | 0x80;
            }
            return true;
        }
        return false;
    }

    bool controller_t::write(uint16_t addr, uint8_t value)
    {
        if (addr == 0x4016)
        {
            status_shift_reg[0] = status[0].reg;
            status_shift_reg[1] = status[1].reg;
            return true;
        }
        return false;
    }
}
