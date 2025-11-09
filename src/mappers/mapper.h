#pragma once
#include "pch.h"
#include "mirroring.h"

#define MAPPER_DEFINE_RESET(T) void T::reset() { *this = T(*cart); }
#define MAPPER_DEFINE_NAME(T, name) const char* T::get_name() const { return name; }

namespace nes
{
    struct cart_t;

    struct mapper_t
    {
        mapper_t(cart_t& cart);
        virtual ~mapper_t() = default;
        virtual void reset() = 0;
        virtual const char* get_name() const = 0;
        virtual bool cpu_read(uint16_t addr, uint8_t& value, bool allow_side_effects);
        virtual bool cpu_write(uint16_t addr, uint8_t value);
        virtual bool ppu_read(uint16_t addr, uint8_t& value, bool allow_side_effects);
        virtual bool ppu_write(uint16_t addr, uint8_t value);
        virtual void on_scanline();

        size_t get_prg_bank_count(uint32_t bank_size) const;
        size_t get_chr_bank_count(uint32_t bank_size) const;

        cart_t* cart;
        mirroring_t mirroring;
        bool irq;
        uint8_t prg_ram[0x2000];
    };
}
