#include "pch.h"
#include "mapper.h"

namespace nes
{
    struct mapper000_t : mapper_t
    {
        mapper000_t(cart_t& cart);
        void reset() override;
    };
}
