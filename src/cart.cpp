#include "pch.h"
#include "cart.h"

namespace nes
{
    bool cart_t::try_load(std::vector<uint8_t> rom, cart_t& cart)
    {
        size_t i = 0;

        if (rom.size() < sizeof(ines_header_t))
        {
            SPDLOG_ERROR("Invalid ROM: too small");
            return false;
        }
        memcpy(&cart.header, rom.data(), sizeof(ines_header_t));
        i += sizeof(ines_header_t);

        // Verify signature
        if (memcmp(cart.header.signature, "NES\x1A", 4) != 0)
        {
            SPDLOG_ERROR("Invalid ROM: invalid signature");
            return false;
        }

        // Skip trainer
        if (cart.header.has_trainer)
        {
            if (rom.size() < i + 512)
            {
                SPDLOG_ERROR("Invalid ROM: trainer too short");
                return false;
            }
            i += 512;
        }

        // Load PRG ROM
        size_t prg_size = cart.header.prg_chunks * 0x4000;
        if (rom.size() < i + prg_size)
        {
            SPDLOG_ERROR("Invalid ROM: PRG ROM too short");
            return false;
        }
        cart.prg_rom = std::span<uint8_t>(rom.data() + i, prg_size);

        // Load CHR ROM
        i += prg_size;
        size_t chr_size = cart.header.chr_chunks * 0x2000;
        if (rom.size() < i + chr_size)
        {
            SPDLOG_ERROR("Invalid ROM: CHR ROM too short");
            return false;
        }
        if (cart.header.chr_chunks == 0)
        {
            cart.chr_ram.resize(0x2000);
            cart.chr = std::span<uint8_t>(cart.chr_ram);
        }
        else
        {
            cart.chr = std::span<uint8_t>(rom.data() + i, chr_size);
        }
        i += chr_size;

        cart.rom = std::move(rom);

        uint8_t mapper_number = (cart.header.mapper_number_upper << 4)
                               | cart.header.mapper_number_lower;
        SPDLOG_INFO("Loaded ROM");
        SPDLOG_INFO("    Mapper:       {}", mapper_number);
        SPDLOG_INFO("    PRG ROM size: {}KB", cart.header.prg_chunks * 16);
        SPDLOG_INFO("    CHR ROM size: {}KB", cart.header.chr_chunks * 8);
        SPDLOG_INFO("    Mirroring:    {}",
                    cart.header.mirroring == mirroring_t::horizontal
                        ? "H" : "V");
        SPDLOG_INFO("    SRAM:         {}",
                    cart.header.has_persistent_storage ? "Yes" : "No");
        SPDLOG_INFO("    PRG RAM size: {}KB",
                    cart.header.prg_ram_size * 8);
        return true;
    }

    void cart_t::reset()
    {
    }

    bool cart_t::read(uint16_t addr, uint8_t& value, bool readonly)
    {
        if (addr < 0x2000)
        {
            value = chr[addr];
            return true;
        }
        else if (addr >= 0x8000)
        {
            value = prg_rom[(addr - 0x8000) % prg_rom.size()];
            return true;
        }
        return false;
    }

    bool cart_t::write(uint16_t addr, uint8_t value)
    {
        if (header.chr_chunks == 0 && addr < 0x2000)
        {
            chr_ram[addr] = value;
            return true;
        }
        return false;
    }
}
