#pragma once
#include "pch.h"
#include "mapper.h"

namespace nes
{
    struct mapper001_t : mapper_t
    {
        mapper001_t(cart_t& cart);
        void reset() override;
        const char* get_name() const override;
        bool cpu_read(uint16_t addr, uint8_t& value, bool readonly) override;
        bool cpu_write(uint16_t addr, uint8_t value) override;
        bool ppu_read(uint16_t addr, uint8_t& value, bool readonly) override;
        bool ppu_write(uint16_t addr, uint8_t value) override;

        uint8_t shift_register;

        union
        {
            struct
            {
                uint8_t mirroring : 2;
                uint8_t prg_bank_mode : 2;
                uint8_t chr_bank_mode : 1;
                uint8_t unused : 3;
            };
            uint8_t reg;
        } control;

        uint8_t chr_bank0;
        uint8_t chr_bank1;
        uint8_t prg_bank;

    private:
        size_t map_prg(uint16_t addr);
        size_t map_chr(uint16_t addr);
    };
}
