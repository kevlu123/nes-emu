#pragma once
#include "pch.h"
#include "bus.h"
#include "mirroring.h"
#include "mappers/mapper.h"

#include <vector>
#include <span>
#include <memory>

namespace nes
{
    /* TODO: pack */
    struct ines_header_t
    {
        uint8_t signature[4];

        uint8_t prg_chunks; // Size in 16KB units

        uint8_t chr_chunks; // Size in 8KB units

        mirroring_t mirroring : 1;
        bool has_persistent_storage : 1;
        bool has_trainer : 1;
        bool _use_alt_nametable_layout : 1;
        uint8_t mapper_number_lower : 4;

        bool _is_vs_unisystem : 1;
        bool _is_playchoice10 : 1;
        uint8_t is_nes2_0 : 2;
        uint8_t mapper_number_upper : 4;

        uint8_t prg_ram_size; // Size in 8KB units

        uint8_t _tv_system_type1 : 1;
        uint8_t _reserved1 : 7;

        uint8_t _tv_system_type2 : 2;
        uint8_t _reserved2 : 2;
        uint8_t _has_prg_ram : 1;
        uint8_t _has_bus_conflicts : 1;
        uint8_t _reserved3 : 2;

        uint8_t _unused[5];
    };

    static_assert(sizeof(ines_header_t) == 16, "INES header size mismatch");

    struct cart_t
    {
        cart_t() = default;
        cart_t(cart_t&&) = delete;
        cart_t& operator=(cart_t&&) = delete;

        static std::unique_ptr<cart_t> load(std::vector<uint8_t> rom);
        void reset();

        bool cpu_read(uint16_t addr, uint8_t& value, bool allow_side_effects);
        bool cpu_write(uint16_t addr, uint8_t value);
        bool ppu_read(uint16_t addr, uint8_t& value, bool allow_side_effects);
        bool ppu_write(uint16_t addr, uint8_t value);

        ines_header_t header;
        std::vector<uint8_t> rom;
        std::span<uint8_t> prg_rom;
        std::span<uint8_t> chr;
        std::vector<uint8_t> chr_ram;
        std::unique_ptr<mapper_t> mapper;
    };
}
