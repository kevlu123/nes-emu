#include "pch.h"
#include "cart.h"

namespace nes
{
    cart_t::cart_t()
    {
    }

    bool cart_t::read(uint16_t addr, uint8_t& value)
    {
        return false;
    }

    bool cart_t::write(uint16_t addr, uint8_t value)
    {
        return false;
    }
}
