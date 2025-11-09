#include "pch.h"
#include "mapper000.h"
#include "cart.h"

namespace nes
{
    mapper000_t::mapper000_t(cart_t& cart)
        : mapper_t(cart)
    {
    }

    MAPPER_DEFINE_RESET(mapper000_t);
    MAPPER_DEFINE_NAME(mapper000_t, "NROM");
}
