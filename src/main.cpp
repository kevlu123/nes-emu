#include "pch.h"

#include <SDL3/SDL_main.h>
#include <SDL3/SDL.h>

#include "imgui.h"
#include "imgui_impl_sdl3.h"
#include "imgui_impl_sdlrenderer3.h"

#include "nes.h"

#include <fstream>
#include <deque>
#include <algorithm>

constexpr size_t AUDIO_SAMPLE_RATE = 48000;
constexpr size_t APU_DEBUG_WIDTH = 1024;
constexpr size_t MIXER_HISTORY_LENGTH = AUDIO_SAMPLE_RATE * 2;

static constexpr SDL_AudioSpec audio_spec = {
    .format = SDL_AUDIO_F32,
    .channels = 1,
    .freq = AUDIO_SAMPLE_RATE,
};

struct colour_t
{
    uint8_t r, g, b, a;
};

static const colour_t NTSC_PALETTE[64] = {
	colour_t{ 84,  84,  84 , 255 },
	colour_t{ 0,   30,  116, 255 },
	colour_t{ 8,   16,  144, 255 },
	colour_t{ 48,  0,   136, 255 },
	colour_t{ 68,  0,   100, 255 },
	colour_t{ 92,  0,   48 , 255 },
	colour_t{ 84,  4,   0  , 255 },
	colour_t{ 60,  24,  0  , 255 },
	colour_t{ 32,  42,  0  , 255 },
	colour_t{ 8,   58,  0  , 255 },
	colour_t{ 0,   64,  0  , 255 },
	colour_t{ 0,   60,  0  , 255 },
	colour_t{ 0,   50,  60 , 255 },
	colour_t{ 0,   0,   0  , 255 },
	colour_t{ 0,   0,   0  , 255 },
	colour_t{ 0,   0,   0  , 255 },
	colour_t{ 152, 150, 152, 255 },
	colour_t{ 8,   76,  196, 255 },
	colour_t{ 48,  50,  236, 255 },
	colour_t{ 92,  30,  228, 255 },
	colour_t{ 136, 20,  176, 255 },
	colour_t{ 160, 20,  100, 255 },
	colour_t{ 152, 34,  32 , 255 },
	colour_t{ 120, 60,  0  , 255 },
	colour_t{ 84,  90,  0  , 255 },
	colour_t{ 40,  114, 0  , 255 },
	colour_t{ 8,   124, 0  , 255 },
	colour_t{ 0,   118, 40 , 255 },
	colour_t{ 0,   102, 120, 255 },
	colour_t{ 0,   0,   0  , 255 },
	colour_t{ 0,   0,   0  , 255 },
	colour_t{ 0,   0,   0  , 255 },
	colour_t{ 236, 238, 236, 255 },
	colour_t{ 76,  154, 236, 255 },
	colour_t{ 120, 124, 236, 255 },
	colour_t{ 176, 98,  236, 255 },
	colour_t{ 228, 84,  236, 255 },
	colour_t{ 236, 88,  180, 255 },
	colour_t{ 236, 106, 100, 255 },
	colour_t{ 212, 136, 32 , 255 },
	colour_t{ 160, 170, 0  , 255 },
	colour_t{ 116, 196, 0  , 255 },
	colour_t{ 76,  208, 32 , 255 },
	colour_t{ 56,  204, 108, 255 },
	colour_t{ 56,  180, 204, 255 },
	colour_t{ 60,  60,  60 , 255 },
	colour_t{ 0,   0,   0  , 255 },
	colour_t{ 0,   0,   0  , 255 },
	colour_t{ 236, 238, 236, 255 },
	colour_t{ 168, 204, 236, 255 },
	colour_t{ 188, 188, 236, 255 },
	colour_t{ 212, 178, 236, 255 },
	colour_t{ 236, 174, 236, 255 },
	colour_t{ 236, 174, 212, 255 },
	colour_t{ 236, 180, 176, 255 },
	colour_t{ 228, 196, 144, 255 },
	colour_t{ 204, 210, 120, 255 },
	colour_t{ 180, 222, 120, 255 },
	colour_t{ 168, 226, 144, 255 },
	colour_t{ 152, 226, 180, 255 },
	colour_t{ 160, 214, 228, 255 },
	colour_t{ 160, 162, 160, 255 },
	colour_t{ 0,   0,   0  , 255 },
	colour_t{ 0,   0,   0  , 255 },
};

template <size_t Width, size_t Height>
struct debug_image_t
{
    constexpr static size_t width = Width;
    constexpr static size_t height = Height;

    SDL_Texture* texture = nullptr;
    uint8_t data[Width * Height] = { 0 };
};

static struct
{
    SDL_Window* window = nullptr;
    SDL_Renderer* renderer = nullptr;
    SDL_AudioDeviceID audio_device = 0;
    SDL_AudioStream *audio_stream = nullptr;
    std::vector<float> new_samples;
    std::unique_ptr<nes::nes_t> nes;
    float audio_tick_counter = 0;
    bool emulation_running = true;
    int emulation_speed = 1;

    debug_image_t<16, 2> debug_palette;
    debug_image_t<256, 128> debug_pattern_table;
    debug_image_t<512, 512> debug_nametable;
    debug_image_t<APU_DEBUG_WIDTH, 16> debug_pulse1;
    debug_image_t<APU_DEBUG_WIDTH, 16> debug_pulse2;
    debug_image_t<APU_DEBUG_WIDTH, 16> debug_triangle;
    debug_image_t<APU_DEBUG_WIDTH, 16> debug_noise;
    debug_image_t<APU_DEBUG_WIDTH, 128> debug_dmc;
    debug_image_t<APU_DEBUG_WIDTH, 128> debug_mixer;
    std::deque<uint8_t> pulse1_history;
    std::deque<uint8_t> pulse2_history;
    std::deque<uint8_t> triangle_history;
    std::deque<uint8_t> noise_history;
    std::deque<uint8_t> dmc_history;
    std::deque<float> mixer_history;
    float audio_viewport_min = -2.0f;
    float audio_viewport_max = 0.0f;
    bool debug_grid_enable = false;
} context;

static SDL_Texture* CreateTexture(int width, int height)
{
    SDL_Texture* texture = SDL_CreateTexture(
        context.renderer,
        SDL_PIXELFORMAT_RGBA32,
        SDL_TEXTUREACCESS_STREAMING,
        width,
        height);
    if (texture == nullptr)
    {
        SPDLOG_ERROR("Error: SDL_CreateTexture(): {}", SDL_GetError());
        return nullptr;
    }
    SDL_SetTextureScaleMode(texture, SDL_SCALEMODE_NEAREST);
    return texture;
}

template <size_t Width, size_t Height>
static bool CreateTexture(debug_image_t<Width, Height>& debug_image)
{
    debug_image.texture = CreateTexture(
        (int)Width,
        (int)Height);
    return debug_image.texture != nullptr;
}

static void WriteTexture(
    SDL_Texture* texture,
    const uint8_t* colour_indices,
    bool debug_grid = false)
{
    float fwidth;
    float fheight;
    int width;
    int height;
    SDL_GetTextureSize(texture, &fwidth, &fheight);
    width = (int)fwidth;
    height = (int)fheight;

    void* pixels;
    int pitch;
    SDL_LockTexture(texture, nullptr, &pixels, &pitch);

    for (int y = 0; y < height; y++)
    {
        uint8_t* row = (uint8_t*)pixels + y * pitch;
        for (int x = 0; x < width; x++)
        {
            uint8_t colour_index = colour_indices[y * width + x];
            const colour_t& colour = NTSC_PALETTE[colour_index % 64];
            memcpy(row + (x * 4), &colour, sizeof(colour));

            if (debug_grid)
            {
                if ((x + y) % 2 == 0)
                {
                    row[(x * 4)] += 16;
                    row[(x * 4) + 1] += 16;
                    row[(x * 4) + 2] += 16;
                }
                if (x % 8 == 0 || y % 8 == 0)
                {
                    row[(x * 4)] += 16;
                    row[(x * 4) + 1] += 16;
                    row[(x * 4) + 2] += 16;
                }
            }
        }
    }

    SDL_UnlockTexture(texture);
}

template <size_t Width, size_t Height>
static void WriteTexture(
    debug_image_t<Width, Height>& debug_image,
    bool debug_grid = false)
{
    WriteTexture(
        debug_image.texture,
        debug_image.data,
        debug_grid);
}

struct disassembly_t
{
    uint16_t addr;
    std::string opcode;
    std::string args;
};

static bool check_window_collapsed()
{
    if (ImGui::IsWindowCollapsed())
    {
        ImGui::SetWindowSize({ 256, 1 });
        ImGui::End();
        return true;
    }
    return false;
}

static void show_cpu_dism()
{
    ImGui::Begin("CPU");
    if (check_window_collapsed())
    {
        return;
    }

    ImGui::SetWindowSize({});
    ImGui::Text("SP:   %02X   A: %02X   X: %02X   Y: %02X",
        context.nes->cpu.sp,
        context.nes->cpu.ra,
        context.nes->cpu.rx,
        context.nes->cpu.ry);
    ImGui::Text("PC: %04X   STATUS: %s%s%s%s%s%s%s",
        context.nes->cpu.pc,
        context.nes->cpu.status.n ? "N" : ".",
        context.nes->cpu.status.v ? "V" : ".",
        context.nes->cpu.status.u ? "U" : ".",
        context.nes->cpu.status.b ? "B" : ".",
        context.nes->cpu.status.d ? "D" : ".",
        context.nes->cpu.status.i ? "I" : ".",
        context.nes->cpu.status.z ? "Z" : ".",
        context.nes->cpu.status.c ? "C" : ".");
    ImGui::Text("");

    const int nearby = 16;
    for (int offset = 0; offset < 3; offset++)
    {
        std::vector<disassembly_t> disassembly;
        int cur_entry = -1;
        for (int i = -nearby * 3; i < nearby * 3;)
        {
            uint16_t addr = context.nes->cpu.pc + i;
            if (addr == context.nes->cpu.pc)
            {
                cur_entry = (int)disassembly.size();
            }

            if (addr > context.nes->cpu.pc && cur_entry == -1)
            {
                break;
            }

            uint8_t opcode = context.nes->cpu_bus.read(addr, false);
            auto& instruction = nes::cpu_t::instructions[opcode];

            bool is_write_only = instruction.opcode == &nes::cpu_t::STA
                              || instruction.opcode == &nes::cpu_t::STX
                              || instruction.opcode == &nes::cpu_t::STY
                              || instruction.opcode == &nes::cpu_t::SAX
                              || instruction.opcode == &nes::cpu_t::AXA
                              || instruction.opcode == &nes::cpu_t::SXA
                              || instruction.opcode == &nes::cpu_t::SYA
                              || instruction.opcode == &nes::cpu_t::XAS;

            std::string args;
            if (instruction.addr_mode == &nes::cpu_t::IMM)
            {
                uint8_t value = context.nes->cpu_bus.read(addr + 1, false);
                args = fmt::format(" #{:02X}", value);
            }
            else if (instruction.addr_mode == &nes::cpu_t::ACC)
            {
                args = fmt::format("       =  #{:02X}", context.nes->cpu.ra);
            }
            else if (instruction.addr_mode == &nes::cpu_t::ABS)
            {
                uint16_t abs_addr = (context.nes->cpu_bus.read(addr + 2, false) << 8)
                    | context.nes->cpu_bus.read(addr + 1, false);
                args = fmt::format("{:04X}", abs_addr);
                if (instruction.opcode != &nes::cpu_t::JMP
                    && instruction.opcode != &nes::cpu_t::JSR)
                {
                    uint8_t data = context.nes->cpu_bus.read(abs_addr, false);
                    args += fmt::format("   =  #{:02X}", data);
                }
            }
            else if (instruction.addr_mode == &nes::cpu_t::ABX)
            {
                uint16_t abs_addr = (context.nes->cpu_bus.read(addr + 2, false) << 8)
                    | context.nes->cpu_bus.read(addr + 1, false);
                uint8_t data = context.nes->cpu_bus.read(abs_addr + context.nes->cpu.rx, false);
                args = fmt::format("{:04X},X =  #{:02X}", abs_addr, data);
            }
            else if (instruction.addr_mode == &nes::cpu_t::ABY)
            {
                uint16_t abs_addr = (context.nes->cpu_bus.read(addr + 2, false) << 8)
                    | context.nes->cpu_bus.read(addr + 1, false);
                uint8_t data = context.nes->cpu_bus.read(abs_addr + context.nes->cpu.ry, false);
                args = fmt::format("{:04X},Y =  #{:02X}", abs_addr, data);
            }
            else if (instruction.addr_mode == &nes::cpu_t::ZRP)
            {
                uint8_t zrp_addr = context.nes->cpu_bus.read(addr + 1, false);
                uint8_t data = context.nes->cpu_bus.read(zrp_addr, false);
                args = fmt::format("  {:02X}   =  #{:02X}", zrp_addr, data);
            }
            else if (instruction.addr_mode == &nes::cpu_t::ZPX)
            {
                uint8_t zrp_addr = context.nes->cpu_bus.read(addr + 1, false);
                uint8_t data = context.nes->cpu_bus.read((zrp_addr + context.nes->cpu.rx) & 0xFF, false);
                args = fmt::format("  {:02X},X =  #{:02X}", zrp_addr, data);
            }
            else if (instruction.addr_mode == &nes::cpu_t::ZPY)
            {
                uint8_t zrp_addr = context.nes->cpu_bus.read(addr + 1, false);
                uint8_t data = context.nes->cpu_bus.read((zrp_addr + context.nes->cpu.ry) & 0xFF, false);
                args = fmt::format("  {:02X},Y =  #{:02X}", zrp_addr, data);
            }
            else if (instruction.addr_mode == &nes::cpu_t::IDX)
            {
                uint8_t zrp_addr = context.nes->cpu_bus.read(addr + 1, false)
                    + context.nes->cpu.rx;
                uint16_t abs_addr = (context.nes->cpu_bus.read((zrp_addr + 1) & 0xFF, false) << 8)
                    | context.nes->cpu_bus.read(zrp_addr, false);
                uint8_t data = context.nes->cpu_bus.read(abs_addr, false);
                args = fmt::format("({:02X},X) =  #{:02X}", zrp_addr, data);
            }
            else if (instruction.addr_mode == &nes::cpu_t::IDY)
            {
                uint8_t zrp_addr = context.nes->cpu_bus.read(addr + 1, false);
                uint16_t abs_addr = (context.nes->cpu_bus.read(zrp_addr, false) << 8)
                    | context.nes->cpu_bus.read((zrp_addr + 1) & 0xFF, false);
                uint8_t data = context.nes->cpu_bus.read(abs_addr + context.nes->cpu.ry, false);
                args = fmt::format("({:02X}),Y =  #{:02X}", zrp_addr, data);
            }
            else if (instruction.addr_mode == &nes::cpu_t::IND)
            {
                uint16_t abs_addr = (context.nes->cpu_bus.read(addr + 2, false) << 8)
                    | context.nes->cpu_bus.read(addr + 1, false);
                uint16_t target_addr = context.nes->cpu_bus.read(abs_addr, false) |
                    (context.nes->cpu_bus.read(((abs_addr + 1) & 0xFF) | (abs_addr & 0xFF00), false) << 8);
                args = fmt::format("({:04X})   = {:04X}", abs_addr, target_addr);
            }
            else if (instruction.addr_mode == &nes::cpu_t::REL)
            {
                int8_t offset = (int8_t)context.nes->cpu_bus.read(addr + 1, false);
                uint16_t target_addr = addr + offset + 2;
                args = fmt::format(" {}{:02X}   = {:04X}",
                    offset >= 0 ? '+' : '-', abs(offset), target_addr);
            }

            if (is_write_only && args.size() > 6)
            {
                args = args.substr(0, 6);
            }

            disassembly.push_back(disassembly_t{
                .addr = addr,
                .opcode = instruction.opcode_name,
                .args = std::move(args),
            });

            i += instruction.byte_count();
        }

        if (cur_entry != -1)
        {
            while (cur_entry > nearby)
            {
                cur_entry--;
                disassembly.erase(disassembly.begin());
            }
            while ((int)disassembly.size() > cur_entry + nearby)
            {
                disassembly.pop_back();
            }
            for (size_t i = 0; i < disassembly.size(); i++)
            {
                auto& entry = disassembly[i];
                ImVec4 colour = i == cur_entry
                    ? ImVec4(1.0f, 1.0f, 0.0f, 1.0f)
                    : ImVec4(0.7f, 0.7f, 0.7f, 1.0f);
                ImGui::TextColored(colour, "%04X: %s %s",
                    entry.addr, entry.opcode.c_str(), entry.args.c_str());
            }
            break;
        }
    }

    ImGui::End();
}

static void show_ram()
{
    ImGui::Begin("RAM");
    if (check_window_collapsed())
    {
        return;
    }

    ImGui::SetWindowSize({ 0, 640 });
    for (size_t i = 0; i < 0x800; i += 0x100)
    {
        if (i)
        {
            ImGui::Text("");
        }
        for (size_t j = 0; j < 0x100; j += 16)
        {
            ImGui::Text("%04X: %02X %02X %02X %02X  %02X %02X %02X %02X  "
                "%02X %02X %02X %02X  %02X %02X %02X %02X",
                i + j,
                context.nes->cpu_bus.read((uint16_t)(i + j +  0), false),
                context.nes->cpu_bus.read((uint16_t)(i + j +  1), false),
                context.nes->cpu_bus.read((uint16_t)(i + j +  2), false),
                context.nes->cpu_bus.read((uint16_t)(i + j +  3), false),
                context.nes->cpu_bus.read((uint16_t)(i + j +  4), false),
                context.nes->cpu_bus.read((uint16_t)(i + j +  5), false),
                context.nes->cpu_bus.read((uint16_t)(i + j +  6), false),
                context.nes->cpu_bus.read((uint16_t)(i + j +  7), false),
                context.nes->cpu_bus.read((uint16_t)(i + j +  8), false),
                context.nes->cpu_bus.read((uint16_t)(i + j +  9), false),
                context.nes->cpu_bus.read((uint16_t)(i + j + 10), false),
                context.nes->cpu_bus.read((uint16_t)(i + j + 11), false),
                context.nes->cpu_bus.read((uint16_t)(i + j + 12), false),
                context.nes->cpu_bus.read((uint16_t)(i + j + 13), false),
                context.nes->cpu_bus.read((uint16_t)(i + j + 14), false),
                context.nes->cpu_bus.read((uint16_t)(i + j + 15), false));
        }
    }
    ImGui::End();
}

static void show_palette()
{
    ImGui::Begin("PALETTE RAM");
    if (check_window_collapsed())
    {
        return;
    }

    size_t i = 0;
    for (uint8_t is_fg = 0; is_fg < 2; is_fg++)
    for (uint8_t attribute = 0; attribute < 4; attribute++)
    for (uint8_t pattern = 0; pattern < 4; pattern++)
    {
        context.debug_palette.data[i++] = context.nes->ppu_bus.read(
            nes::ppu_t::get_palette_addr(
                is_fg,
                attribute,
                pattern),
            false);
    }

    WriteTexture(context.debug_palette);
    ImGui::SetWindowSize({});
    ImGui::Image(context.debug_palette.texture, { 16 * 16, 16 * 2 });
    ImGui::End();
}

static void show_pattern_table()
{
    ImGui::Begin("PATTERN TABLE");
    if (check_window_collapsed())
    {
        return;
    }

    size_t i = 0;
    for (uint8_t coarse_y = 0; coarse_y < 16; coarse_y++)
    for (uint8_t fine_y = 0; fine_y < 8; fine_y++)
    for (uint8_t lr_table = 0; lr_table < 2; lr_table++)
    for (uint8_t coarse_x = 0; coarse_x < 16; coarse_x++)
    {
        uint8_t pattern_lo = context.nes->ppu_bus.read(
            nes::ppu_t::get_pattern_lo_addr(
                lr_table,
                coarse_y * 16 + coarse_x,
                fine_y),
            false);
        uint8_t pattern_hi = context.nes->ppu_bus.read(
            nes::ppu_t::get_pattern_hi_addr(
                lr_table,
                coarse_y * 16 + coarse_x,
                fine_y),
            false);
        pattern_lo = nes::ppu_t::reverse_bits(pattern_lo);
        pattern_hi = nes::ppu_t::reverse_bits(pattern_hi);
        for (uint8_t fine_x = 0; fine_x < 8; fine_x++)
        {
            uint8_t pattern = ((pattern_lo >> fine_x) & 1)
                | (((pattern_hi >> fine_x) & 1) << 1);
            static const uint8_t colours[4] = { 0x0F, 0x00, 0x10, 0x20 };
            context.debug_pattern_table.data[i++] = colours[pattern];
        }
    }

    WriteTexture(context.debug_pattern_table);
    ImGui::SetWindowSize({});
    ImGui::Image(context.debug_pattern_table.texture, { 512, 256 });
    ImGui::End();
}

static void show_nametable()
{
    ImGui::Begin("NAMETABLE");
    if (check_window_collapsed())
    {
        return;
    }

    size_t i = 0;
    for (uint8_t nametable_y = 0; nametable_y < 2; nametable_y++)
    {
        for (uint8_t coarse_y = 0; coarse_y < 30; coarse_y++)
        for (uint8_t fine_y = 0; fine_y < 8; fine_y++)
        for (uint8_t nametable_x = 0; nametable_x < 2; nametable_x++)
        {
            uint8_t nametable = nametable_y * 2 + nametable_x;
            for (uint8_t coarse_x = 0; coarse_x < 32; coarse_x++)
            {
                uint8_t attribute = context.nes->ppu_bus.read(
                    nes::ppu_t::get_attribute_addr(
                        nametable,
                        coarse_x,
                        coarse_y),
                    false);
                attribute = nes::ppu_t::get_attribute(
                    attribute,
                    coarse_x,
                    coarse_y);

                uint8_t tile = context.nes->ppu_bus.read(
                    nes::ppu_t::get_tile_addr(
                        nametable,
                        coarse_x,
                        coarse_y),
                    false);

                uint8_t pattern_lo = context.nes->ppu_bus.read(
                    nes::ppu_t::get_pattern_lo_addr(
                        context.nes->ppu.ppuctrl.bg_pattern_table_addr,
                        tile,
                        fine_y),
                    false);
                uint8_t pattern_hi = context.nes->ppu_bus.read(
                    nes::ppu_t::get_pattern_hi_addr(
                        context.nes->ppu.ppuctrl.bg_pattern_table_addr,
                        tile,
                        fine_y),
                    false);
                pattern_lo = nes::ppu_t::reverse_bits(pattern_lo);
                pattern_hi = nes::ppu_t::reverse_bits(pattern_hi);

                for (uint8_t fine_x = 0; fine_x < 8; fine_x++)
                {
                    uint8_t pattern = ((pattern_lo >> fine_x) & 1)
                        | (((pattern_hi >> fine_x) & 1) << 1);

                    context.debug_nametable.data[i++] = context.nes->ppu_bus.read(
                        nes::ppu_t::get_palette_addr(
                            0,
                            attribute,
                            pattern),
                        false);
                }
            }
        }

        for (uint8_t block_y = 0; block_y < 16; block_y++)
        for (uint8_t nametable_x = 0; nametable_x < 2; nametable_x++)
        {
            uint8_t nametable = nametable_y * 2 + nametable_x;
            for (uint8_t block_x = 0; block_x < 16; block_x++)
            {
                uint8_t attribute = context.nes->ppu_bus.read(
                    nes::ppu_t::get_attribute_addr(
                        nametable,
                        block_x * 2,
                        block_y * 2),
                    false);
                attribute = nes::ppu_t::get_attribute(
                    attribute,
                    block_x * 2,
                    block_y * 2);
                for (uint8_t pattern = 0; pattern < 4; pattern++)
                for (uint8_t repeat_x = 0; repeat_x < 4; repeat_x++)
                {
                    context.debug_nametable.data[i++] = context.nes->ppu_bus.read(
                        nes::ppu_t::get_palette_addr(
                            0,
                            attribute,
                            pattern),
                        false);
                }
            }
        }
    }

    WriteTexture(context.debug_nametable);
    ImGui::SetWindowSize({});
    ImGui::Image(context.debug_nametable.texture, { 512, 512 });
    ImGui::End();
}

static void show_sprites()
{
    ImGui::Begin("OAM");
    if (check_window_collapsed())
    {
        return;
    }

    ImGui::SetWindowSize({ 0, 640 });
    if (context.nes->ppu.ppuctrl.sprite_size == 0)
    {
        ImGui::Text("    RAW             I  X  Y  PL PR FH FV");
        for (size_t i = 0; i < 64; i += 8)
        {
            ImGui::Text("");
            for (size_t j = 0; j < 8; j++)
            {
                auto& oam = context.nes->ppu.oam[i + j];
                ImVec4 colour = oam.y < 0xEF
                    ? ImVec4(1.0f, 1.0f, 1.0f, 1.0f)
                    : ImVec4(0.7f, 0.7f, 0.7f, 1.0f);
                ImGui::TextColored(
                    colour,
                    "%02X: %02X %02X %02X %02X     %02X %02X %02X %d  %c  %c  %c",
                    i + j,
                    context.nes->ppu.oam_bytes[(i + j) * 4],
                    context.nes->ppu.oam_bytes[(i + j) * 4 + 1],
                    context.nes->ppu.oam_bytes[(i + j) * 4 + 2],
                    context.nes->ppu.oam_bytes[(i + j) * 4 + 3],
                    oam.tile_index,
                    oam.x,
                    oam.y,
                    oam.palette + 4,
                    oam.priority ? 'X' : '.',
                    oam.flip_horizontally ? 'X' : '.',
                    oam.flip_vertically ? 'X' : '.');
            }
        }
    }
    else
    {
        ImGui::Text("8x16 sprites view not supported.");
    }
    ImGui::End();
}

static void show_apu()
{
    ImGui::Begin("APU");
    if (check_window_collapsed())
    {
        return;
    }

    if (ImGui::IsWindowHovered())
    {
        ImGuiIO& io = ImGui::GetIO();
        if (io.MouseWheel != 0.0f)
        {
            float mouse_x = io.MousePos.x - ImGui::GetWindowPos().x - ImGui::GetStyle().WindowPadding.x;
            float mouse_ratio = mouse_x / APU_DEBUG_WIDTH;
            float viewport_size = context.audio_viewport_max - context.audio_viewport_min;
            float zoom_amount = viewport_size * 0.1f * (io.MouseWheel > 0.0f ? 1.0f : -1.0f);
            context.audio_viewport_min += zoom_amount * mouse_ratio;
            context.audio_viewport_max -= zoom_amount * (1.0f - mouse_ratio);
            context.audio_viewport_min = std::clamp(context.audio_viewport_min, -2.0f, -0.001f);
            context.audio_viewport_max = std::clamp(context.audio_viewport_max, -1.999f, 0.0f);
        }
    }

    constexpr uint8_t BG = 0x2D;
    constexpr uint8_t WHITE = 0x30;

    memset(context.debug_pulse1.data, BG, sizeof(context.debug_pulse1.data));
    memset(context.debug_pulse2.data, BG, sizeof(context.debug_pulse2.data));
    memset(context.debug_triangle.data, BG, sizeof(context.debug_triangle.data));
    memset(context.debug_noise.data, BG, sizeof(context.debug_noise.data));
    memset(context.debug_dmc.data, BG, sizeof(context.debug_dmc.data));
    memset(context.debug_mixer.data, BG, sizeof(context.debug_mixer.data));

    for (size_t i = 0; i < APU_DEBUG_WIDTH; i++)
    {
        float viewport_pos = (((float)i / (float)APU_DEBUG_WIDTH)
            * (context.audio_viewport_max - context.audio_viewport_min))
            + context.audio_viewport_min;
        size_t index = MIXER_HISTORY_LENGTH + (size_t)std::floor(AUDIO_SAMPLE_RATE * viewport_pos);
        if (index >= MIXER_HISTORY_LENGTH)
        {
            continue;
        }

        uint8_t sample = context.pulse1_history[index];
        context.debug_pulse1.data[APU_DEBUG_WIDTH * (context.debug_pulse1.height - sample - 1) + i] = WHITE;

        sample = context.pulse2_history[index];
        context.debug_pulse2.data[APU_DEBUG_WIDTH * (context.debug_pulse2.height - sample - 1) + i] = WHITE;

        sample = context.triangle_history[index];
        context.debug_triangle.data[APU_DEBUG_WIDTH * (context.debug_triangle.height - sample - 1) + i] = WHITE;

        sample = context.noise_history[index];
        context.debug_noise.data[APU_DEBUG_WIDTH * (context.debug_noise.height - sample - 1) + i] = WHITE;

        sample = context.dmc_history[index];
        context.debug_dmc.data[APU_DEBUG_WIDTH * (context.debug_dmc.height - sample - 1) + i] = WHITE;

        sample = (uint8_t)(context.mixer_history[index] * 127.0f);
        context.debug_mixer.data[APU_DEBUG_WIDTH * (context.debug_mixer.height - sample - 1) + i] = WHITE;
    }

    WriteTexture(context.debug_pulse1);
    WriteTexture(context.debug_pulse2);
    WriteTexture(context.debug_triangle);
    WriteTexture(context.debug_noise);
    WriteTexture(context.debug_dmc);
    WriteTexture(context.debug_mixer);

    ImGui::SetWindowSize({});

    ImGui::Checkbox("Pulse 1", &context.nes->apu.pulse1.debug_enabled);
    ImGui::Image(context.debug_pulse1.texture, { APU_DEBUG_WIDTH, context.debug_pulse1.height });

    ImGui::Checkbox("Pulse 2", &context.nes->apu.pulse2.debug_enabled);
    ImGui::Image(context.debug_pulse2.texture, { APU_DEBUG_WIDTH, context.debug_pulse2.height });

    ImGui::Checkbox("Triangle", &context.nes->apu.triangle.debug_enabled);
    ImGui::SameLine();
    ImGui::Checkbox("Zero when stopped", &context.nes->apu.triangle.debug_output_zero_when_stopped);
    ImGui::Image(context.debug_triangle.texture, { APU_DEBUG_WIDTH, context.debug_triangle.height });

    ImGui::Checkbox("Noise", &context.nes->apu.noise.debug_enabled);
    ImGui::Image(context.debug_noise.texture, { APU_DEBUG_WIDTH, context.debug_noise.height });

    ImGui::Checkbox("DMC", &context.nes->apu.dmc.debug_enabled);
    ImGui::Image(context.debug_dmc.texture, { APU_DEBUG_WIDTH, context.debug_dmc.height });

    ImGui::Checkbox("Mix", &context.nes->apu.debug_enabled);
    ImGui::Image(context.debug_mixer.texture, { APU_DEBUG_WIDTH, context.debug_mixer.height });

    ImGui::End();
}

static void handle_key_up(SDL_KeyboardEvent key)
{
    switch (key.key)
    {
    case SDLK_A:
        context.nes->controller.status[0].left = false;
        break;
    case SDLK_D:
        context.nes->controller.status[0].right = false;
        break;
    case SDLK_W:
        context.nes->controller.status[0].up = false;
        break;
    case SDLK_S:
        context.nes->controller.status[0].down = false;
        break;
    case SDLK_SEMICOLON:
        context.nes->controller.status[0].b = false;
        break;
    case SDLK_APOSTROPHE:
        context.nes->controller.status[0].a = false;
        break;
    case SDLK_RSHIFT:
        context.nes->controller.status[0].select = false;
        break;
    case SDLK_RETURN:
        context.nes->controller.status[0].start = false;
        break;
    }
}

static void on_clock()
{
    constexpr float TICKS_PER_SAMPLE = 5369319 / AUDIO_SAMPLE_RATE;
    context.audio_tick_counter++;
    if (context.audio_tick_counter < TICKS_PER_SAMPLE)
    {
        return;
    }
    context.audio_tick_counter -= TICKS_PER_SAMPLE;

    float mixed_sample = context.nes->apu.get_mixed_sample();
    context.pulse1_history.pop_front();
    context.pulse1_history.push_back(context.nes->apu.pulse1.get_sample());
    context.pulse2_history.pop_front();
    context.pulse2_history.push_back(context.nes->apu.pulse2.get_sample());
    context.triangle_history.pop_front();
    context.triangle_history.push_back(context.nes->apu.triangle.get_sample());
    context.noise_history.pop_front();
    context.noise_history.push_back(context.nes->apu.noise.get_sample());
    context.dmc_history.pop_front();
    context.dmc_history.push_back(context.nes->apu.dmc.get_sample());
    context.mixer_history.pop_front();
    context.mixer_history.push_back(mixed_sample);
    context.new_samples.push_back(mixed_sample * 2.0f - 1.0f);
}

static void handle_key_down(SDL_KeyboardEvent key)
{
    bool ctrl = (key.mod & (SDL_KMOD_LCTRL | SDL_KMOD_RCTRL)) != 0;
    bool alt = (key.mod & (SDL_KMOD_LALT | SDL_KMOD_RALT)) != 0;
    bool shift = (key.mod & (SDL_KMOD_LSHIFT | SDL_KMOD_RSHIFT)) != 0;
    switch (key.key)
    {
    case SDLK_A:
        context.nes->controller.status[0].left = true;
        break;
    case SDLK_D:
        context.nes->controller.status[0].right = true;
        break;
    case SDLK_W:
        context.nes->controller.status[0].up = true;
        break;
    case SDLK_S:
        context.nes->controller.status[0].down = true;
        break;
    case SDLK_SEMICOLON:
        context.nes->controller.status[0].b = true;
        break;
    case SDLK_APOSTROPHE:
        context.nes->controller.status[0].a = true;
        break;
    case SDLK_RSHIFT:
        context.nes->controller.status[0].select = true;
        break;
    case SDLK_RETURN:
        if (alt)
        {
            bool is_fullscreen = SDL_GetWindowFlags(context.window)
                & SDL_WINDOW_FULLSCREEN;
            SDL_SetWindowFullscreen(context.window, !is_fullscreen);
        }
        else
        {
            context.nes->controller.status[0].start = true;
        }
        break;
    case SDLK_R:
        if (ctrl)
        {
            context.nes->reset();
            std::fill(context.pulse1_history.begin(), context.pulse1_history.end(), 0);
            std::fill(context.pulse2_history.begin(), context.pulse2_history.end(), 0);
            std::fill(context.triangle_history.begin(), context.triangle_history.end(), 0);
            std::fill(context.noise_history.begin(), context.noise_history.end(), 0);
            std::fill(context.dmc_history.begin(), context.dmc_history.end(), 0);
            std::fill(context.mixer_history.begin(), context.mixer_history.end(), 0.0f);
        }
        break;
    case SDLK_P:
        if (ctrl)
        {
            context.emulation_running = !context.emulation_running;
        }
        break;
    case SDLK_G:
        if (ctrl)
        {
            context.debug_grid_enable = !context.debug_grid_enable;
        }
        break;
    case SDLK_F11:
        for (int i = 0; i < (shift ? 10 : 1); i++)
        {
            context.nes->clock_instruction(on_clock);
        }
        break;
    case SDLK_1:
        if (ctrl)
        {
            context.emulation_speed = 1;
        }
        break;
    case SDLK_2:
        if (ctrl)
        {
            context.emulation_speed = 2;
        }
        break;
    }
}

int main(int argc, char* argv[])
{
    if (argc < 2)
    {
        SPDLOG_ERROR("Usage: {} <rom_file>", argv[0]);
        return -1;
    }
    const char* rom_file = argv[1];

    std::ifstream rom(rom_file, std::ios::binary | std::ios::ate);
    size_t rom_size = rom.tellg();
    rom.seekg(0, std::ios::beg);
    if (!rom.is_open() || rom_size == 0)
    {
        SPDLOG_ERROR("Failed to open ROM file: {}", rom_file);
        return -1;
    }
    std::vector<uint8_t> rom_data(rom_size);
    rom.read((char*)rom_data.data(), rom_size);

    nes::cart_t cart;
    if (!nes::cart_t::try_load(std::move(rom_data), cart))
    {
        SPDLOG_ERROR("Failed to load ROM file: {}", rom_file);
        return -1;
    }

    context.nes = std::make_unique<nes::nes_t>();
    context.nes->load_cart(std::move(cart));

    // Initialise SDL

    if (!SDL_Init(SDL_INIT_VIDEO | SDL_INIT_AUDIO | SDL_INIT_GAMEPAD))
    {
        SPDLOG_ERROR("Failed to initialize SDL: {}", SDL_GetError());
        return -1;
    }

    SDL_WindowFlags window_flags = SDL_WINDOW_RESIZABLE
                                 | SDL_WINDOW_HIDDEN
                                 | SDL_WINDOW_HIGH_PIXEL_DENSITY
                                 | SDL_WINDOW_MAXIMIZED;
    context.window = SDL_CreateWindow(
        "NES",
        1280,
        720,
        window_flags);
    if (context.window == nullptr)
    {
        SPDLOG_ERROR("Error: SDL_CreateWindow(): {}", SDL_GetError());
        return -1;
    }

    context.renderer = SDL_CreateRenderer(context.window, nullptr);
    SDL_SetRenderVSync(context.renderer, 1);
    if (context.renderer == nullptr)
    {
        SPDLOG_ERROR("Error: SDL_CreateRenderer(): {}", SDL_GetError());
        return -1;
    }
    SDL_SetWindowPosition(
        context.window,
        SDL_WINDOWPOS_CENTERED,
        SDL_WINDOWPOS_CENTERED);
    SDL_ShowWindow(context.window);

    SDL_Texture* nes_screen_texture = CreateTexture(
        nes::ppu_t::SCREEN_WIDTH,
        nes::ppu_t::SCREEN_HEIGHT);
    if (nes_screen_texture == nullptr)
    {
        SPDLOG_ERROR("Error: nes_screen_texture CreateTexture(): {}", SDL_GetError());
        return -1;
    }

    if (!CreateTexture(context.debug_palette))
    {
        SPDLOG_ERROR("Error: debug_palette CreateTexture(): {}", SDL_GetError());
        return -1;
    }
    if (!CreateTexture(context.debug_pattern_table))
    {
        SPDLOG_ERROR("Error: debug_pattern_table CreateTexture(): {}", SDL_GetError());
        return -1;
    }
    if (!CreateTexture(context.debug_nametable))
    {
        SPDLOG_ERROR("Error: debug_nametable CreateTexture(): {}", SDL_GetError());
        return -1;
    }
    if (!CreateTexture(context.debug_pulse1))
    {
        SPDLOG_ERROR("Error: debug_pulse1 CreateTexture(): {}", SDL_GetError());
        return -1;
    }
    if (!CreateTexture(context.debug_pulse2))
    {
        SPDLOG_ERROR("Error: debug_pulse2 CreateTexture(): {}", SDL_GetError());
        return -1;
    }
    if (!CreateTexture(context.debug_triangle))
    {
        SPDLOG_ERROR("Error: debug_triangle CreateTexture(): {}", SDL_GetError());
        return -1;
    }
    if (!CreateTexture(context.debug_noise))
    {
        SPDLOG_ERROR("Error: debug_noise CreateTexture(): {}", SDL_GetError());
        return -1;
    }
    if (!CreateTexture(context.debug_dmc))
    {
        SPDLOG_ERROR("Error: debug_dmc CreateTexture(): {}", SDL_GetError());
        return -1;
    }
    if (!CreateTexture(context.debug_mixer))
    {
        SPDLOG_ERROR("Error: debug_mixer CreateTexture(): {}", SDL_GetError());
        return -1;
    }

    context.audio_device = SDL_OpenAudioDevice(SDL_AUDIO_DEVICE_DEFAULT_PLAYBACK, &audio_spec);
    if (context.audio_device == 0)
    {
        SPDLOG_ERROR("Error: SDL_OpenAudioDevice(): {}", SDL_GetError());
        return -1;
    }
    context.audio_stream = SDL_CreateAudioStream(&audio_spec, &audio_spec);
    if (context.audio_stream == nullptr)
    {
        SPDLOG_ERROR("Error: SDL_CreateAudioStream(): {}", SDL_GetError());
        return -1;
    }
    if (!SDL_BindAudioStream(context.audio_device, context.audio_stream))
    {
        SPDLOG_ERROR("Error: SDL_BindAudioStream(): {}", SDL_GetError());
        return -1;
    }
    context.pulse1_history.resize(MIXER_HISTORY_LENGTH, 0);
    context.pulse2_history.resize(MIXER_HISTORY_LENGTH, 0);
    context.triangle_history.resize(MIXER_HISTORY_LENGTH, 0);
    context.noise_history.resize(MIXER_HISTORY_LENGTH, 0);
    context.dmc_history.resize(MIXER_HISTORY_LENGTH, 0);
    context.mixer_history.resize(MIXER_HISTORY_LENGTH, 0);

    // Initialise imgui

    ImGui::CreateContext();
    ImGuiIO& io = ImGui::GetIO();
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableGamepad;

    ImGui_ImplSDL3_InitForSDLRenderer(context.window, context.renderer);
    ImGui_ImplSDLRenderer3_Init(context.renderer);

    // Main loop

    SPDLOG_INFO("Application initialised");
    bool running = true;
    uint64_t frames_run = SDL_GetTicks() * 60 / 1000;
    int fps_counter = 0;
    uint64_t tick_counter = SDL_GetTicks();
    while (running)
    {
        // Process events

        SDL_Event event;
        while (SDL_PollEvent(&event))
        {
            ImGui_ImplSDL3_ProcessEvent(&event);
            switch (event.type)
            {
            case SDL_EVENT_QUIT:
                running = false;
                break;
            case SDL_EVENT_WINDOW_CLOSE_REQUESTED:
                if (event.window.windowID == SDL_GetWindowID(context.window))
                {
                    running = false;
                }
                break;
            case SDL_EVENT_KEY_DOWN:
                handle_key_down(event.key);
                break;
            case SDL_EVENT_KEY_UP:
                handle_key_up(event.key);
                break;
            }
        }

        // Run emulation

        uint64_t current_ticks = SDL_GetTicks();
        uint64_t frames_expected = current_ticks * 60 / 1000;
        if (context.emulation_running)
        {
            for (int i = 0; i < 5 && frames_run < frames_expected; i++)
            {
                for (int j = 0; j < context.emulation_speed; j++)
                {
                    context.nes->clock_frame(on_clock);
                }
                frames_run++;
                fps_counter++;
            }
        }
        frames_run = frames_expected;

        SDL_PutAudioStreamData(context.audio_stream, context.new_samples.data(), (int)context.new_samples.size() * sizeof(float));
        context.new_samples.clear();

        if (current_ticks / 1000 != tick_counter / 1000)
        {
            SDL_SetWindowTitle(context.window, fmt::format(
                "NES - FPS: {}",
                fps_counter).c_str());
            fps_counter = 0;
        }
        tick_counter = current_ticks;

        // Begin rendering

        ImGui_ImplSDLRenderer3_NewFrame();
        ImGui_ImplSDL3_NewFrame();
        ImGui::NewFrame();
        SDL_RenderClear(context.renderer);

        // Render ImGui

        show_cpu_dism();
        show_ram();
        show_sprites();

        ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2{0, 1});
        show_palette();
        show_pattern_table();
        show_nametable();
        show_apu();
        ImGui::PopStyleVar();

        // Render NES screen

        WriteTexture(
            nes_screen_texture,
            context.nes->screen_buffer,
            context.debug_grid_enable);

        int window_width, window_height;
        SDL_GetWindowSize(context.window, &window_width, &window_height);
        float aspect = (float)window_width / window_height;
        if (aspect < nes::ppu_t::SCREEN_ASPECT)
        {
            float new_height = window_width / nes::ppu_t::SCREEN_ASPECT;
            SDL_FRect dst_rect = {
                0.0f,
                (window_height - new_height) / 2.0f,
                (float)window_width,
                new_height,
            };
            SDL_RenderTexture(context.renderer, nes_screen_texture, nullptr, &dst_rect);
        }
        else
        {
            float new_width = window_height * nes::ppu_t::SCREEN_ASPECT;
            SDL_FRect dst_rect = {
                (window_width - new_width) / 2.0f,
                0.0f,
                new_width,
                (float)window_height,
            };
            SDL_RenderTexture(context.renderer, nes_screen_texture, nullptr, &dst_rect);
        }

        // End rendering

        ImGui::Render();
        ImGui_ImplSDLRenderer3_RenderDrawData(ImGui::GetDrawData(), context.renderer);
        SDL_RenderPresent(context.renderer);
    }

    SPDLOG_INFO("Application exiting");

    ImGui_ImplSDLRenderer3_Shutdown();
    ImGui_ImplSDL3_Shutdown();
    ImGui::DestroyContext();

    SDL_UnbindAudioStream(context.audio_stream);
    SDL_DestroyAudioStream(context.audio_stream);
    SDL_CloseAudioDevice(context.audio_device);
    SDL_DestroyTexture(context.debug_palette.texture);
    SDL_DestroyTexture(context.debug_pattern_table.texture);
    SDL_DestroyTexture(context.debug_nametable.texture);
    SDL_DestroyTexture(context.debug_pulse1.texture);
    SDL_DestroyTexture(context.debug_pulse2.texture);
    SDL_DestroyTexture(context.debug_triangle.texture);
    SDL_DestroyTexture(context.debug_noise.texture);
    SDL_DestroyTexture(context.debug_dmc.texture);
    SDL_DestroyTexture(context.debug_mixer.texture);
    SDL_DestroyTexture(nes_screen_texture);
    SDL_DestroyRenderer(context.renderer);
    SDL_DestroyWindow(context.window);
    SDL_Quit();

    return 0;
}
