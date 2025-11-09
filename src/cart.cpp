#include "pch.h"
#include "cart.h"
#include "mappers/mapper000.h"
#include "mappers/mapper001.h"
#include "mappers/mapper002.h"
#include "mappers/mapper003.h"
#include "mappers/mapper004.h"
#include "mappers/mapper007.h"

#define MAPPER(n, T) case n: cart.mapper = std::make_unique<T>(cart); return true;

namespace nes
{
    static bool load_mapper(uint8_t mapper_number, cart_t& cart)
    {
        switch (mapper_number)
        {
            MAPPER(0, mapper000_t);
            MAPPER(1, mapper001_t);
            MAPPER(2, mapper002_t);
            MAPPER(3, mapper003_t);
            MAPPER(4, mapper004_t);
            MAPPER(7, mapper007_t);
        default:
            return false;
        }
    }

    std::unique_ptr<cart_t> cart_t::load(std::vector<uint8_t> rom)
    {
        size_t i = 0;
        auto cart = std::make_unique<cart_t>();

        if (rom.size() < sizeof(ines_header_t))
        {
            SPDLOG_ERROR("Invalid ROM: too small");
            return nullptr;
        }
        memcpy(&cart->header, rom.data(), sizeof(ines_header_t));
        i += sizeof(ines_header_t);

        // Verify signature
        if (memcmp(cart->header.signature, "NES\x1A", 4) != 0)
        {
            SPDLOG_ERROR("Invalid ROM: invalid signature");
            return nullptr;
        }

        // Skip trainer
        if (cart->header.has_trainer)
        {
            if (rom.size() < i + 512)
            {
                SPDLOG_ERROR("Invalid ROM: trainer too short");
                return nullptr;
            }
            i += 512;
        }

        // Load PRG ROM
        size_t prg_size = cart->header.prg_chunks * 0x4000;
        if (prg_size == 0)
        {
            SPDLOG_ERROR("Invalid ROM: PRG ROM size is zero");
            return nullptr;
        } else if (rom.size() < i + prg_size)
        {
            SPDLOG_ERROR("Invalid ROM: PRG ROM too short");
            return nullptr;
        }
        cart->prg_rom = std::span<uint8_t>(rom.data() + i, prg_size);

        // Load CHR ROM
        i += prg_size;
        size_t chr_size = cart->header.chr_chunks * 0x2000;
        if (rom.size() < i + chr_size)
        {
            SPDLOG_ERROR("Invalid ROM: CHR ROM too short");
            return nullptr;
        }
        if (cart->header.chr_chunks == 0)
        {
            cart->chr_ram.resize(0x2000);
            cart->chr = std::span<uint8_t>(cart->chr_ram);
        }
        else
        {
            cart->chr = std::span<uint8_t>(rom.data() + i, chr_size);
        }
        i += chr_size;

        cart->rom = std::move(rom);

        uint8_t mapper_number = (cart->header.mapper_number_upper << 4)
                               | cart->header.mapper_number_lower;
        if (!load_mapper(mapper_number, *cart))
        {
            SPDLOG_ERROR("Unsupported mapper: {}", mapper_number);
            return nullptr;
        }
        return cart;
    }

    void cart_t::reset()
    {
        mapper->reset();
    }

    bool cart_t::cpu_read(uint16_t addr, uint8_t& value, bool allow_side_effects)
    {
        return mapper->cpu_read(addr, value, allow_side_effects);
    }

    bool cart_t::cpu_write(uint16_t addr, uint8_t value)
    {
        return mapper->cpu_write(addr, value);
    }

    bool cart_t::ppu_read(uint16_t addr, uint8_t& value, bool allow_side_effects)
    {
        return mapper->ppu_read(addr, value, allow_side_effects);
    }

    bool cart_t::ppu_write(uint16_t addr, uint8_t value)
    {
        return mapper->ppu_write(addr, value);
    }
}
