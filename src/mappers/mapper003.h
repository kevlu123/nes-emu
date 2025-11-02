#pragma once
#include "pch.h"
#include "mapper.h"

namespace nes
{
    struct mapper003_t : mapper_t
    {
        mapper003_t(cart_t& cart);
        void reset() override;
        bool cpu_write(uint16_t addr, uint8_t value) override;
        bool ppu_read(uint16_t addr, uint8_t& value, bool readonly) override;
        bool ppu_write(uint16_t addr, uint8_t value) override;

        uint8_t bank_select;

    private:
        size_t map_chr(uint16_t addr);
    };
}
