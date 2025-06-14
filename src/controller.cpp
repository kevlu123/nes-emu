#include "pch.h"
#include "controller.h"

namespace nes
{
    controller_t::controller_t()
    {
    }

    void controller_t::reset()
    {
    }

    bool controller_t::read(uint16_t addr, uint8_t& value)
    {
        return false;
    }

    bool controller_t::write(uint16_t addr, uint8_t value)
    {
        return false;
    }
}