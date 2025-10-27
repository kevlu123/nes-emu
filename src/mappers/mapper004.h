#pragma once
#include "pch.h"
#include "mapper.h"

namespace nes
{
    struct mapper004_t : mapper_t
    {
        mapper004_t(cart_t& cart);
        void reset() override;

        bool cpu_read(uint16_t addr, uint8_t& value, bool readonly) override;
        bool cpu_write(uint16_t addr, uint8_t value) override;
        bool ppu_read(uint16_t addr, uint8_t& value, bool readonly) override;
        bool ppu_write(uint16_t addr, uint8_t value) override;

        union
        {
            struct
            {
                uint8_t select : 3;
                uint8_t unused : 2;
                uint8_t mmc6 : 1;
                uint8_t prg_mode : 1;
                uint8_t chr_mode : 1;
            };
            uint8_t reg;
        } bank_select;
        uint8_t map_regs[8];
        mirroring_t mirroring;
        std::vector<uint8_t> prg_ram;

    private:
        size_t map_prg(uint16_t addr);
        size_t map_chr(uint16_t addr);
    };
}
