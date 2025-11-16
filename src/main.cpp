#include "pch.h"
#include "imgui_ini.h"

#include <spdlog/pattern_formatter.h>
#include <spdlog/sinks/stdout_color_sinks.h>
#include <spdlog/sinks/callback_sink.h>

#include <SDL3/SDL_main.h>
#include <SDL3/SDL.h>

#include "imgui.h"
#include "imgui_impl_sdl3.h"
#include "imgui_impl_sdlrenderer3.h"

#include "nes.h"

#include <fstream>
#include <deque>
#include <algorithm>
#include <filesystem>

constexpr size_t AUDIO_SAMPLE_RATE = 48000;
constexpr size_t APU_DEBUG_WIDTH = 1024;
constexpr size_t MIXER_HISTORY_LENGTH = AUDIO_SAMPLE_RATE * 2;
constexpr size_t MAX_DEBUG_LOG_LINES = 512;

static constexpr SDL_AudioSpec audio_spec = {
    .format = SDL_AUDIO_F32,
    .channels = 1,
    .freq = AUDIO_SAMPLE_RATE,
};

struct colour_t
{
    uint8_t r, g, b, a;
};

static constexpr colour_t NTSC_PALETTE[64] = {
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

enum class action_t
{
    reset,
    unload_cart,
    clock_frame,
    clock_scanline,
    clock_instruction,
    clock_dot,
};

template <size_t Width, size_t Height>
struct debug_image_t
{
    constexpr static size_t width = Width;
    constexpr static size_t height = Height;

    SDL_Texture* texture = nullptr;
    uint8_t data[Width * Height] = { 0 };
};

struct log_msg_t
{
    std::string text;
    ImVec4 colour;
};

static struct
{
    SDL_Window* window = nullptr;
    SDL_Renderer* renderer = nullptr;
    SDL_AudioDeviceID audio_device = 0;
    SDL_AudioStream *audio_stream = nullptr;
    std::vector<float> new_samples;
    std::unique_ptr<nes::nes_t> nes;
    SDL_Texture* screen_texture = nullptr;
    float audio_tick_counter = 0;

    struct
    {
        bool fullscreen = false;
        bool show_grid = false;
        bool show_sprite_zero_hit = false;
        int show_pixel_trace = 0;
        bool pause = false;
        int emulation_speed = 1;
        std::optional<nes::pixel_trace_t> pixel_trace;
        std::string cart_name;
    } debug_control;

    struct
    {
        std::deque<log_msg_t> logs;
        bool has_new_log = false;
    } debug_log;

    struct
    {
        debug_image_t<4 * 8, 1 * 8> images[8];
    } debug_palette;

    struct
    {
        debug_image_t<128, 256> image;
    } debug_pattern_table;

    struct
    {
        debug_image_t<512, 512> image;
    } debug_nametable;

    struct
    {
        debug_image_t<8, 16> images[64];
    } debug_sprites;

    struct
    {
        debug_image_t<APU_DEBUG_WIDTH, 16> pulse1_image;
        debug_image_t<APU_DEBUG_WIDTH, 16> pulse2_image;
        debug_image_t<APU_DEBUG_WIDTH, 16> triangle_image;
        debug_image_t<APU_DEBUG_WIDTH, 16> noise_image;
        debug_image_t<APU_DEBUG_WIDTH, 128> dmc_image;
        debug_image_t<APU_DEBUG_WIDTH, 128> mixer_image;
        std::deque<uint8_t> pulse1_history;
        std::deque<uint8_t> pulse2_history;
        std::deque<uint8_t> triangle_history;
        std::deque<uint8_t> noise_history;
        std::deque<uint8_t> dmc_history;
        std::deque<float> mixer_history;
        float viewport_min = -2.0f;
        float viewport_max = 0.0f;
    } debug_apu;
} ctx;

static ImVec2 operator+(const ImVec2& a, const ImVec2& b)
{
    return ImVec2{ a.x + b.x, a.y + b.y };
}

static void clear_apu_history()
{
    std::fill(ctx.debug_apu.pulse1_history.begin(), ctx.debug_apu.pulse1_history.end(), 0);
    std::fill(ctx.debug_apu.pulse2_history.begin(), ctx.debug_apu.pulse2_history.end(), 0);
    std::fill(ctx.debug_apu.triangle_history.begin(), ctx.debug_apu.triangle_history.end(), 0);
    std::fill(ctx.debug_apu.noise_history.begin(), ctx.debug_apu.noise_history.end(), 0);
    std::fill(ctx.debug_apu.dmc_history.begin(), ctx.debug_apu.dmc_history.end(), 0);
    std::fill(ctx.debug_apu.mixer_history.begin(), ctx.debug_apu.mixer_history.end(), 0.0f);
}

static void do_action(action_t action)
{
    switch (action)
    {
    case action_t::reset:
        ctx.nes->reset();
        clear_apu_history();
        break;
    case action_t::unload_cart:
        ctx.nes->unload_cart();
        ctx.debug_control.cart_name.clear();
        clear_apu_history();
        break;
    case action_t::clock_frame:
        ctx.nes->clock_frame();
        break;
    case action_t::clock_scanline:
        ctx.nes->clock_scanline();
        break;
    case action_t::clock_instruction:
        ctx.nes->clock_instruction();
        break;
    case action_t::clock_dot:
        ctx.nes->clock();
        break;
    default:
        SPDLOG_ERROR("Unknown action");
        break;
    }
}

static SDL_Texture* create_texture(int width, int height)
{
    SDL_Texture* texture = SDL_CreateTexture(
        ctx.renderer,
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
static bool create_texture(debug_image_t<Width, Height>& debug_image)
{
    debug_image.texture = create_texture(
        (int)Width,
        (int)Height);
    return debug_image.texture != nullptr;
}

static void write_texture(
    SDL_Texture* texture,
    const uint8_t* colour_indices,
    std::function<void(uint8_t*, int, int, int)> post_process = nullptr)
{
    float fwidth;
    float fheight;
    int width;
    int height;
    SDL_GetTextureSize(texture, &fwidth, &fheight);
    width = (int)fwidth;
    height = (int)fheight;

    uint8_t* pixels;
    int pitch;
    SDL_LockTexture(texture, nullptr, (void**)&pixels, &pitch);

    for (int y = 0; y < height; y++)
    {
        colour_t* row = (colour_t*)(pixels + y * pitch);
        for (int x = 0; x < width; x++)
        {
            uint8_t colour_index = colour_indices[y * width + x];
            row[x] = NTSC_PALETTE[colour_index % 64];
        }
    }

    if (post_process)
    {
        post_process(pixels, width, height, pitch);
    }

    SDL_UnlockTexture(texture);
}

template <size_t Width, size_t Height>
static void write_texture(
    debug_image_t<Width, Height>& debug_image,
    std::function<void(uint8_t*, int, int, int)> post_process = nullptr)
{
    write_texture(
        debug_image.texture,
        debug_image.data,
        post_process);
}

static void draw_centred_image(SDL_Texture* texture)
{
    float tex_width, tex_height;
    SDL_GetTextureSize(texture, &tex_width, &tex_height);
    float tex_aspect = tex_width / tex_height;

    auto window_size = ImGui::GetContentRegionAvail();
    float window_width = window_size.x;
    float window_height = window_size.y;
    float window_aspect = window_width / window_height;

    SDL_FRect dst_rect;
    if (window_aspect < tex_aspect)
    {
        float new_height = window_width / tex_aspect;
        dst_rect = {
            0.0f,
            (window_height - new_height) / 2.0f,
            window_width,
            new_height,
        };
    }
    else
    {
        float new_width = window_height * tex_aspect;
        dst_rect = {
            (window_width - new_width) / 2.0f,
            0.0f,
            new_width,
            window_height,
        };
    }

    ImGui::SetCursorPos(ImGui::GetWindowContentRegionMin() + ImVec2{ dst_rect.x, dst_rect.y });
    ImGui::Image(texture, ImVec2{ dst_rect.w, dst_rect.h });
}

static void draw_rectangle(uint8_t* pixels, int width, int height, int pitch,
                           size_t x, size_t y, size_t rect_width, size_t rect_height,
                           colour_t colour = colour_t{ 255, 0, 0, 255 })
{
    for (int dy = 0; dy < rect_height; dy++)
    {
        ((colour_t*)(pixels + (y + dy) * pitch))[x] = colour;
        ((colour_t*)(pixels + (y + dy) * pitch))[x + rect_width - 1] = colour;
    }
    for (int dx = 1; dx < rect_width - 1; dx++)
    {
        ((colour_t*)(pixels + y * pitch))[x + dx] = colour;
        ((colour_t*)(pixels + (y + rect_height - 1) * pitch))[x + dx] = colour;
    }
}

static void draw_separator()
{
    ImGui::Spacing();
    ImGui::Spacing();
    ImGui::Spacing();
}

static void show_log()
{
    if (ImGui::Begin("Log"))
    {
        for (const auto& log : ctx.debug_log.logs)
        {
            ImGui::PushStyleColor(ImGuiCol_Text, log.colour);
            ImGui::TextWrapped(log.text.c_str());
            ImGui::PopStyleColor();
        }

        if (ctx.debug_log.has_new_log && ImGui::GetScrollY() >= ImGui::GetScrollMaxY())
        {
            ImGui::SetScrollHereY(1.0f);
            ctx.debug_log.has_new_log = false;
        }
    }
    ImGui::End();
}

static void show_cpu_dism()
{
    struct disassembly_t
    {
        uint16_t addr;
        std::string opcode;
        std::string args;
    };

    if (ImGui::Begin("CPU"))
    {
        ImGui::Text("SP:   %02X   A: %02X   X: %02X   Y: %02X",
            ctx.nes->cpu.sp,
            ctx.nes->cpu.ra,
            ctx.nes->cpu.rx,
            ctx.nes->cpu.ry);
        ImGui::Text("PC: %04X   STATUS: %s%s%s%s%s%s%s",
            ctx.nes->cpu.pc,
            ctx.nes->cpu.status.n ? "N" : ".",
            ctx.nes->cpu.status.v ? "V" : ".",
            ctx.nes->cpu.status.u ? "U" : ".",
            ctx.nes->cpu.status.b ? "B" : ".",
            ctx.nes->cpu.status.d ? "D" : ".",
            ctx.nes->cpu.status.i ? "I" : ".",
            ctx.nes->cpu.status.z ? "Z" : ".",
            ctx.nes->cpu.status.c ? "C" : ".");
        ImGui::Text("");

        const int nearby = 16;
        for (int offset = 0; offset < 3; offset++)
        {
            std::vector<disassembly_t> disassembly;
            int cur_entry = -1;
            for (int i = -nearby * 3; i < nearby * 3;)
            {
                uint16_t addr = ctx.nes->cpu.pc + i;
                if (addr == ctx.nes->cpu.pc)
                {
                    cur_entry = (int)disassembly.size();
                }

                if (addr > ctx.nes->cpu.pc && cur_entry == -1)
                {
                    break;
                }

                uint8_t opcode = ctx.nes->cpu_bus.read(addr, false);
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
                    uint8_t value = ctx.nes->cpu_bus.read(addr + 1, false);
                    args = fmt::format(" #{:02X}", value);
                }
                else if (instruction.addr_mode == &nes::cpu_t::ACC)
                {
                    args = fmt::format("       =  #{:02X}", ctx.nes->cpu.ra);
                }
                else if (instruction.addr_mode == &nes::cpu_t::ABS)
                {
                    uint16_t abs_addr = (ctx.nes->cpu_bus.read(addr + 2, false) << 8)
                        | ctx.nes->cpu_bus.read(addr + 1, false);
                    args = fmt::format("{:04X}", abs_addr);
                    if (instruction.opcode != &nes::cpu_t::JMP
                        && instruction.opcode != &nes::cpu_t::JSR)
                    {
                        uint8_t data = ctx.nes->cpu_bus.read(abs_addr, false);
                        args += fmt::format("   =  #{:02X}", data);
                    }
                }
                else if (instruction.addr_mode == &nes::cpu_t::ABX)
                {
                    uint16_t abs_addr = (ctx.nes->cpu_bus.read(addr + 2, false) << 8)
                        | ctx.nes->cpu_bus.read(addr + 1, false);
                    uint8_t data = ctx.nes->cpu_bus.read(abs_addr + ctx.nes->cpu.rx, false);
                    args = fmt::format("{:04X},X =  #{:02X}", abs_addr, data);
                }
                else if (instruction.addr_mode == &nes::cpu_t::ABY)
                {
                    uint16_t abs_addr = (ctx.nes->cpu_bus.read(addr + 2, false) << 8)
                        | ctx.nes->cpu_bus.read(addr + 1, false);
                    uint8_t data = ctx.nes->cpu_bus.read(abs_addr + ctx.nes->cpu.ry, false);
                    args = fmt::format("{:04X},Y =  #{:02X}", abs_addr, data);
                }
                else if (instruction.addr_mode == &nes::cpu_t::ZRP)
                {
                    uint8_t zrp_addr = ctx.nes->cpu_bus.read(addr + 1, false);
                    uint8_t data = ctx.nes->cpu_bus.read(zrp_addr, false);
                    args = fmt::format("  {:02X}   =  #{:02X}", zrp_addr, data);
                }
                else if (instruction.addr_mode == &nes::cpu_t::ZPX)
                {
                    uint8_t zrp_addr = ctx.nes->cpu_bus.read(addr + 1, false);
                    uint8_t data = ctx.nes->cpu_bus.read((zrp_addr + ctx.nes->cpu.rx) & 0xFF, false);
                    args = fmt::format("  {:02X},X =  #{:02X}", zrp_addr, data);
                }
                else if (instruction.addr_mode == &nes::cpu_t::ZPY)
                {
                    uint8_t zrp_addr = ctx.nes->cpu_bus.read(addr + 1, false);
                    uint8_t data = ctx.nes->cpu_bus.read((zrp_addr + ctx.nes->cpu.ry) & 0xFF, false);
                    args = fmt::format("  {:02X},Y =  #{:02X}", zrp_addr, data);
                }
                else if (instruction.addr_mode == &nes::cpu_t::IDX)
                {
                    uint8_t zrp_addr = ctx.nes->cpu_bus.read(addr + 1, false)
                        + ctx.nes->cpu.rx;
                    uint16_t abs_addr = (ctx.nes->cpu_bus.read((zrp_addr + 1) & 0xFF, false) << 8)
                        | ctx.nes->cpu_bus.read(zrp_addr, false);
                    uint8_t data = ctx.nes->cpu_bus.read(abs_addr, false);
                    args = fmt::format("({:02X},X) =  #{:02X}", zrp_addr, data);
                }
                else if (instruction.addr_mode == &nes::cpu_t::IDY)
                {
                    uint8_t zrp_addr = ctx.nes->cpu_bus.read(addr + 1, false);
                    uint16_t abs_addr = (ctx.nes->cpu_bus.read(zrp_addr, false) << 8)
                        | ctx.nes->cpu_bus.read((zrp_addr + 1) & 0xFF, false);
                    uint8_t data = ctx.nes->cpu_bus.read(abs_addr + ctx.nes->cpu.ry, false);
                    args = fmt::format("({:02X}),Y =  #{:02X}", zrp_addr, data);
                }
                else if (instruction.addr_mode == &nes::cpu_t::IND)
                {
                    uint16_t abs_addr = (ctx.nes->cpu_bus.read(addr + 2, false) << 8)
                        | ctx.nes->cpu_bus.read(addr + 1, false);
                    uint16_t target_addr = ctx.nes->cpu_bus.read(abs_addr, false) |
                        (ctx.nes->cpu_bus.read(((abs_addr + 1) & 0xFF) | (abs_addr & 0xFF00), false) << 8);
                    args = fmt::format("({:04X}) = {:04X}", abs_addr, target_addr);
                }
                else if (instruction.addr_mode == &nes::cpu_t::REL)
                {
                    int8_t offset = (int8_t)ctx.nes->cpu_bus.read(addr + 1, false);
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
    }
    ImGui::End();
}

static void show_memory(nes::bus_t& bus, size_t start_addr, size_t end_addr)
{
    for (size_t i = start_addr; i < end_addr; i += 0x100)
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
                bus.read((uint16_t)(i + j +  0), false),
                bus.read((uint16_t)(i + j +  1), false),
                bus.read((uint16_t)(i + j +  2), false),
                bus.read((uint16_t)(i + j +  3), false),
                bus.read((uint16_t)(i + j +  4), false),
                bus.read((uint16_t)(i + j +  5), false),
                bus.read((uint16_t)(i + j +  6), false),
                bus.read((uint16_t)(i + j +  7), false),
                bus.read((uint16_t)(i + j +  8), false),
                bus.read((uint16_t)(i + j +  9), false),
                bus.read((uint16_t)(i + j + 10), false),
                bus.read((uint16_t)(i + j + 11), false),
                bus.read((uint16_t)(i + j + 12), false),
                bus.read((uint16_t)(i + j + 13), false),
                bus.read((uint16_t)(i + j + 14), false),
                bus.read((uint16_t)(i + j + 15), false));
        }
    }
}

static void show_ram()
{
    if (ImGui::Begin("RAM"))
    {
        show_memory(ctx.nes->cpu_bus, 0x0000, 0x0800);
    }
    ImGui::End();
}

static void show_cpu_memory_map()
{
    if (ImGui::Begin("CPU MAP"))
    {
        show_memory(ctx.nes->cpu_bus, 0x0000, 0x10000);
    }
    ImGui::End();
}

static void show_ppu_memory_map()
{
    if (ImGui::Begin("PPU MAP"))
    {
        show_memory(ctx.nes->ppu_bus, 0x0000, 0x4000);
    }
    ImGui::End();
}

static void show_palette()
{
    if (ImGui::Begin("PALETTE"))
    {
        for (uint8_t is_fg = 0; is_fg < 2; is_fg++)
        for (uint8_t attribute = 0; attribute < 4; attribute++)
        for (uint8_t pattern = 0; pattern < 4; pattern++)
        for (uint8_t y = 0; y < 8; y++)
        for (uint8_t x = 0; x < 8; x++)
        {
            auto& image = ctx.debug_palette.images[is_fg * 4 + attribute];
            image.data[(y * image.width) + (pattern * 8) + x] = ctx.nes->ppu_bus.read(
                nes::ppu_t::get_palette_addr(
                    is_fg,
                    attribute,
                    pattern),
                false);
        }

        for (int i = 0; i < 4; i++)
        {
            for (int is_fg = 0; is_fg < 2; is_fg++)
            {
                auto& image = ctx.debug_palette.images[is_fg * 4 + i];
                write_texture(image, [&](uint8_t *pixels, int width, int height, int pitch)
                {
                    if (ctx.debug_control.pixel_trace)
                    {
                        if (!is_fg && ctx.debug_control.pixel_trace->bg.attribute == i)
                        {
                            size_t pattern = ctx.debug_control.pixel_trace->bg.pattern;
                            draw_rectangle(pixels, width, height, pitch, pattern * 8, 0, 8, 8);
                        }
                        if (is_fg && ctx.debug_control.pixel_trace->fg.exists
                            && ctx.debug_control.pixel_trace->fg.attribute == i)
                        {
                            size_t pattern = ctx.debug_control.pixel_trace->fg.pattern;
                            draw_rectangle(pixels, width, height, pitch, pattern * 8, 0, 8, 8);
                        }
                    }
                });

                ImGui::Text("%s %d:", is_fg ? "FG" : "BG", i);
                ImGui::SameLine();
                ImGui::Image(image.texture, ImVec2{ 16 * 4, 16 * 1 });
                if (!is_fg)
                {
                    ImGui::SameLine();
                }
            }
        }
    }
    ImGui::End();
}

static void show_pattern_table()
{
    if (ImGui::Begin("PATTERN TABLE"))
    {
        constexpr uint8_t colours[4] = { 0x0F, 0x00, 0x10, 0x20 };
        size_t i = 0;
        for (uint8_t lr_table = 0; lr_table < 2; lr_table++)
        for (uint8_t coarse_y = 0; coarse_y < 16; coarse_y++)
        for (uint8_t fine_y = 0; fine_y < 8; fine_y++)
        for (uint8_t coarse_x = 0; coarse_x < 16; coarse_x++)
        {
            uint8_t pattern_lo = ctx.nes->ppu_bus.read(
                nes::ppu_t::get_pattern_lo_addr(
                    lr_table,
                    coarse_y * 16 + coarse_x,
                    fine_y),
                false);
            uint8_t pattern_hi = ctx.nes->ppu_bus.read(
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
                ctx.debug_pattern_table.image.data[i++] = colours[pattern];
            }
        }

        write_texture(ctx.debug_pattern_table.image, [](uint8_t *pixels, int width, int height, int pitch)
        {
            if (ctx.debug_control.pixel_trace)
            {
                size_t tile_index = ctx.debug_control.pixel_trace->bg.tile_index;
                size_t pattern_table = ctx.nes->ppu.ppuctrl.bg_pattern_table_addr;
                size_t y = (pattern_table * 16 * 8) + (tile_index / 16 * 8);
                size_t x = (tile_index % 16 * 8);
                draw_rectangle(pixels, width, height, pitch, x, y, 8, 8);

                if (ctx.debug_control.pixel_trace->fg.exists)
                {
                    tile_index = ctx.debug_control.pixel_trace->fg.tile_index;
                    pattern_table = ctx.debug_control.pixel_trace->fg.pattern_table;
                    y = (pattern_table * 16 * 8) + (tile_index / 16 * 8);
                    x = (tile_index % 16 * 8);
                    draw_rectangle(pixels, width, height, pitch, x, y, 8, 8);
                }
            }
        });
        draw_centred_image(ctx.debug_pattern_table.image.texture);
    }
    ImGui::End();
}

static void show_nametable()
{
    if (ImGui::Begin("NAMETABLE"))
    {
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
                    uint8_t attribute = ctx.nes->ppu_bus.read(
                        nes::ppu_t::get_attribute_addr(
                            nametable,
                            coarse_x,
                            coarse_y),
                        false);
                    attribute = nes::ppu_t::get_attribute(
                        attribute,
                        coarse_x,
                        coarse_y);

                    uint8_t tile = ctx.nes->ppu_bus.read(
                        nes::ppu_t::get_tile_addr(
                            nametable,
                            coarse_x,
                            coarse_y),
                        false);

                    uint8_t pattern_lo = ctx.nes->ppu_bus.read(
                        nes::ppu_t::get_pattern_lo_addr(
                            ctx.nes->ppu.ppuctrl.bg_pattern_table_addr,
                            tile,
                            fine_y),
                        false);
                    uint8_t pattern_hi = ctx.nes->ppu_bus.read(
                        nes::ppu_t::get_pattern_hi_addr(
                            ctx.nes->ppu.ppuctrl.bg_pattern_table_addr,
                            tile,
                            fine_y),
                        false);
                    pattern_lo = nes::ppu_t::reverse_bits(pattern_lo);
                    pattern_hi = nes::ppu_t::reverse_bits(pattern_hi);

                    for (uint8_t fine_x = 0; fine_x < 8; fine_x++)
                    {
                        uint8_t pattern = ((pattern_lo >> fine_x) & 1)
                            | (((pattern_hi >> fine_x) & 1) << 1);

                        ctx.debug_nametable.image.data[i++] = ctx.nes->ppu_bus.read(
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
                    uint8_t attribute = ctx.nes->ppu_bus.read(
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
                        ctx.debug_nametable.image.data[i++] = ctx.nes->ppu_bus.read(
                            nes::ppu_t::get_palette_addr(
                                0,
                                attribute,
                                pattern),
                            false);
                    }
                }
            }
        }

        write_texture(ctx.debug_nametable.image, [](uint8_t *pixels, int width, int height, int pitch)
        {
            size_t left = ctx.nes->ppu.debug.centre_scroll_x;
            size_t top = (ctx.nes->ppu.debug.centre_scroll_y & 0xFF)
                + ((ctx.nes->ppu.debug.centre_scroll_y & 0x100) ? 240 : 0);

            auto draw_red = [&](size_t x, size_t y)
            {
                size_t xx = x % 512;
                size_t yy = y % 480;
                if (yy >= 240)
                {
                    yy += 16;
                }
                colour_t* row = (colour_t*)(pixels + yy * pitch);
                row[xx] = { 255, 0, 0, 255 };
            };
            for (size_t y = 0; y < nes::ppu_t::SCREEN_HEIGHT; y++)
            {
                draw_red(left, top + y);
                draw_red(left + nes::ppu_t::SCREEN_WIDTH - 1, top + y);
            }
            for (size_t x = 1; x < nes::ppu_t::SCREEN_WIDTH - 1; x++)
            {
                draw_red(left + x, top);
                draw_red(left + x, top + nes::ppu_t::SCREEN_HEIGHT - 1);
            }
        });
        draw_centred_image(ctx.debug_nametable.image.texture);
    }
    ImGui::End();
}

static void show_sprites()
{
    if (ImGui::Begin("SPRITES"))
    {
        uint8_t sprite_size = ctx.nes->ppu.ppuctrl.sprite_size == 0 ? 8 : 16;
        uint8_t tile_mask = sprite_size == 16 ? 0xFE : 0xFF;

        ImGui::Text("    RAW             I  X  Y  A P H V");
        for (size_t i = 0; i < 64; i += 8)
        {
            ImGui::Text("");
            for (size_t j = 0; j < 8; j++)
            {
                auto& oam = ctx.nes->ppu.oam[i + j];
                auto& image = ctx.debug_sprites.images[i + j];
                uint8_t pattern_table = sprite_size == 16
                    ? oam.tile_index & 1
                    : ctx.nes->ppu.ppuctrl.sprite8x8_table_addr;

                for (uint8_t y = 0; y < sprite_size; y++)
                {
                    uint8_t y_pos = oam.flip_vertically ? (sprite_size - y - 1) : y;
                    uint8_t pattern_lo;
                    uint8_t pattern_hi;
                    if (y_pos < 8)
                    {
                        pattern_lo = ctx.nes->ppu_bus.read(
                            nes::ppu_t::get_pattern_lo_addr(
                                pattern_table,
                                oam.tile_index & tile_mask,
                                y_pos),
                            false);
                        pattern_hi = ctx.nes->ppu_bus.read(
                            nes::ppu_t::get_pattern_hi_addr(
                                pattern_table,
                                oam.tile_index & tile_mask,
                                y_pos),
                            false);
                    }
                    else
                    {
                        pattern_lo = ctx.nes->ppu_bus.read(
                            nes::ppu_t::get_pattern_lo_addr(
                                pattern_table,
                                (oam.tile_index & 0xFE) + 1,
                                y_pos - 8),
                            false);
                        pattern_hi = ctx.nes->ppu_bus.read(
                            nes::ppu_t::get_pattern_hi_addr(
                                pattern_table,
                                (oam.tile_index & 0xFE) + 1,
                                y_pos - 8),
                            false);
                    }
                    if (!oam.flip_horizontally)
                    {
                        pattern_lo = nes::ppu_t::reverse_bits(pattern_lo);
                        pattern_hi = nes::ppu_t::reverse_bits(pattern_hi);
                    }
                    for (size_t x = 0; x < 8; x++, pattern_lo >>= 1, pattern_hi >>= 1)
                    {
                        uint8_t pattern = (pattern_lo & 1) | ((pattern_hi & 1) << 1);
                        image.data[y * 8 + x] = ctx.nes->ppu_bus.read(
                            nes::ppu_t::get_palette_addr(
                                1,
                                oam.attribute,
                                pattern),
                            false);
                    }
                }
                write_texture(image, [&](uint8_t *pixels, int width, int height, int pitch)
                {
                    if (ctx.debug_control.pixel_trace
                        && ctx.debug_control.pixel_trace->fg.exists
                        && ctx.debug_control.pixel_trace->fg.sprite_index == i + j)
                    {
                        draw_rectangle(pixels, width, height, pitch, 0, 0, 8, sprite_size);
                    }
                });

                ImVec4 colour = oam.y < 0xEF
                    ? ImVec4(1.0f, 1.0f, 1.0f, 1.0f)
                    : ImVec4(0.7f, 0.7f, 0.7f, 1.0f);
                ImGui::TextColored(
                    colour,
                    "%02X: %02X %02X %02X %02X     %02X %02X %02X %d %c %c %c",
                    i + j,
                    ctx.nes->ppu.oam_bytes[(i + j) * 4],
                    ctx.nes->ppu.oam_bytes[(i + j) * 4 + 1],
                    ctx.nes->ppu.oam_bytes[(i + j) * 4 + 2],
                    ctx.nes->ppu.oam_bytes[(i + j) * 4 + 3],
                    oam.tile_index,
                    oam.x,
                    oam.y,
                    oam.attribute,
                    oam.priority ? 'P' : '.',
                    oam.flip_horizontally ? 'H' : '.',
                    oam.flip_vertically ? 'V' : '.');
                ImGui::SameLine();
                ImGui::Image(
                    image.texture,
                    { 16.0f, sprite_size * 2.0f },
                    { 0.0f, 0.0f },
                    { 1.0f, sprite_size == 8 ? 0.5f : 1.0f });
            }
        }
    }
    ImGui::End();
}

static void show_control()
{
    if (ImGui::Begin("Control"))
    {
        ImGui::Text("FPS: %.1f", ImGui::GetIO().Framerate);
        draw_separator();

        if (ctx.nes->cart)
        {
            ImGui::Text("Cartridge: %s", ctx.debug_control.cart_name.c_str());
            ImGui::Text("  PRG:    %d KB", ctx.nes->cart->header.prg_chunks * 16);
            if (ctx.nes->cart->header.chr_chunks)
            {
                ImGui::Text("  CHR:    %d KB", ctx.nes->cart->header.chr_chunks * 8);
            }
            else
            {
                ImGui::Text("  CHR:    8 KB (CHR-RAM)");
            }
            ImGui::Text("  Mapper: %d (%s)",
                ctx.nes->cart->header.mapper_number_lower
                    | (ctx.nes->cart->header.mapper_number_upper << 4),
                ctx.nes->cart->mapper->get_name());
        }
        else
        {
            ImGui::Text("Cartridge (not loaded)");
            ImGui::Text("  PRG:    -");
            ImGui::Text("  CHR:    -");
            ImGui::Text("  Mapper: -");
        }

        draw_separator();
        ImGui::Text("Dot:      %d", ctx.nes->ppu.dot);
        ImGui::Text("Scanline: %d", ctx.nes->ppu.scanline);
        ImGui::Text("Scroll X: %d (+%d)",
            ctx.nes->ppu.t_vram_addr.coarse_x_scroll * 8
                + ctx.nes->ppu.x_scroll.value,
            (ctx.nes->ppu.t_vram_addr.nametable_select & 1) ? 256 : 0);
        ImGui::Text("Scroll Y: %d (+%d)",
            ctx.nes->ppu.t_vram_addr.coarse_y_scroll * 8
                + ctx.nes->ppu.t_vram_addr.fine_y_scroll,
            (ctx.nes->ppu.t_vram_addr.nametable_select & 2) ? 240 : 0);
        const char* mirroring;
        if (ctx.nes->cart)
        {
            switch (ctx.nes->cart->mapper->mirroring)
            {
                case nes::mirroring_t::horizontal:      mirroring = "Horizontal";      break;
                case nes::mirroring_t::vertical:        mirroring = "Vertical";        break;
                case nes::mirroring_t::one_screen_low:  mirroring = "One-screen low";  break;
                case nes::mirroring_t::one_screen_high: mirroring = "One-screen high"; break;
                default:                                mirroring = "Unknown";         break;
            }
        }
        else
        {
            mirroring = "-";
        }
        ImGui::Text("Mirroring: %s", mirroring);

        draw_separator();
        if (ImGui::Button("Step frame"))
        {
            do_action(action_t::clock_frame);
        }
        ImGui::SameLine();
        if (ImGui::Button("Step scanline"))
        {
            do_action(action_t::clock_scanline);
        }
        ImGui::SameLine();
        if (ImGui::Button("Step CPU"))
        {
            do_action(action_t::clock_instruction);
        }
        ImGui::SameLine();
        if (ImGui::Button("Step PPU"))
        {
            do_action(action_t::clock_dot);
        }

        draw_separator();
        if (ImGui::Button("Reset"))
        {
            ctx.nes->reset();
        }

        draw_separator();
        ImGui::SliderInt("Speed", &ctx.debug_control.emulation_speed, 1, 4, "%dx", ImGuiSliderFlags_NoInput);
        ImGui::Checkbox("Pause", &ctx.debug_control.pause);

        draw_separator();
        ImGui::Checkbox("Fullscreen", &ctx.debug_control.fullscreen);

        draw_separator();
        ImGui::Checkbox("Show background", &ctx.nes->ppu.debug.enable_bg);
        ImGui::Checkbox("Show sprites", &ctx.nes->ppu.debug.enable_fg);
        ImGui::Checkbox("Show grid", &ctx.debug_control.show_grid);
        ImGui::Checkbox("Show sprite 0 hit", &ctx.debug_control.show_sprite_zero_hit);
        ImGui::Checkbox("Force greyscale", &ctx.nes->ppu.debug.enable_greyscale);
    }
    ImGui::End();
}

static void show_apu()
{
    if (ImGui::Begin("APU"))
    {
        float window_width = ImGui::GetContentRegionAvail().x;

        if (ImGui::IsWindowHovered())
        {
            ImGuiIO& io = ImGui::GetIO();
            if (io.MouseWheel != 0.0f)
            {
                float mouse_x = io.MousePos.x - ImGui::GetWindowPos().x - ImGui::GetStyle().WindowPadding.x;
                float mouse_ratio = mouse_x / window_width;
                float viewport_size = ctx.debug_apu.viewport_max - ctx.debug_apu.viewport_min;
                float zoom_amount = viewport_size * 0.1f * (io.MouseWheel > 0.0f ? 1.0f : -1.0f);
                ctx.debug_apu.viewport_min += zoom_amount * mouse_ratio;
                ctx.debug_apu.viewport_max -= zoom_amount * (1.0f - mouse_ratio);
                ctx.debug_apu.viewport_min = std::clamp(ctx.debug_apu.viewport_min, -2.0f, -0.001f);    
                ctx.debug_apu.viewport_max = std::clamp(ctx.debug_apu.viewport_max, -1.999f, 0.0f);
            }
        }

        constexpr uint8_t BG = 0x2D;
        constexpr uint8_t WHITE = 0x30;

        memset(ctx.debug_apu.pulse1_image.data, BG, sizeof(ctx.debug_apu.pulse1_image.data));
        memset(ctx.debug_apu.pulse2_image.data, BG, sizeof(ctx.debug_apu.pulse2_image.data));
        memset(ctx.debug_apu.triangle_image.data, BG, sizeof(ctx.debug_apu.triangle_image.data));
        memset(ctx.debug_apu.noise_image.data, BG, sizeof(ctx.debug_apu.noise_image.data));
        memset(ctx.debug_apu.dmc_image.data, BG, sizeof(ctx.debug_apu.dmc_image.data));
        memset(ctx.debug_apu.mixer_image.data, BG, sizeof(ctx.debug_apu.mixer_image.data));

        for (size_t i = 0; i < APU_DEBUG_WIDTH; i++)
        {
            float viewport_pos = (((float)i / (float)APU_DEBUG_WIDTH)
                * (ctx.debug_apu.viewport_max - ctx.debug_apu.viewport_min))
                + ctx.debug_apu.viewport_min;
            size_t index = MIXER_HISTORY_LENGTH + (size_t)std::floor(AUDIO_SAMPLE_RATE * viewport_pos);
            if (index >= MIXER_HISTORY_LENGTH)
            {
                continue;
            }

            uint8_t sample = ctx.debug_apu.pulse1_history[index];
            ctx.debug_apu.pulse1_image.data[APU_DEBUG_WIDTH * (ctx.debug_apu.pulse1_image.height - sample - 1) + i] = WHITE;

            sample = ctx.debug_apu.pulse2_history[index];
            ctx.debug_apu.pulse2_image.data[APU_DEBUG_WIDTH * (ctx.debug_apu.pulse2_image.height - sample - 1) + i] = WHITE;

            sample = ctx.debug_apu.triangle_history[index];
            ctx.debug_apu.triangle_image.data[APU_DEBUG_WIDTH * (ctx.debug_apu.triangle_image.height - sample - 1) + i] = WHITE;

            sample = ctx.debug_apu.noise_history[index];
            ctx.debug_apu.noise_image.data[APU_DEBUG_WIDTH * (ctx.debug_apu.noise_image.height - sample - 1) + i] = WHITE;

            sample = ctx.debug_apu.dmc_history[index];
            ctx.debug_apu.dmc_image.data[APU_DEBUG_WIDTH * (ctx.debug_apu.dmc_image.height - sample - 1) + i] = WHITE;

            sample = (uint8_t)(ctx.debug_apu.mixer_history[index] * 127.0f);
            ctx.debug_apu.mixer_image.data[APU_DEBUG_WIDTH * (ctx.debug_apu.mixer_image.height - sample - 1) + i] = WHITE;
        }

        write_texture(ctx.debug_apu.pulse1_image);
        write_texture(ctx.debug_apu.pulse2_image);
        write_texture(ctx.debug_apu.triangle_image);
        write_texture(ctx.debug_apu.noise_image);
        write_texture(ctx.debug_apu.dmc_image);
        write_texture(ctx.debug_apu.mixer_image);

        ImGui::Checkbox("Pulse 1", &ctx.nes->apu.pulse1.debug.enabled);
        ImGui::Image(ctx.debug_apu.pulse1_image.texture, { window_width, ctx.debug_apu.pulse1_image.height });

        ImGui::Checkbox("Pulse 2", &ctx.nes->apu.pulse2.debug.enabled);
        ImGui::Image(ctx.debug_apu.pulse2_image.texture, { window_width, ctx.debug_apu.pulse2_image.height });

        ImGui::Checkbox("Triangle", &ctx.nes->apu.triangle.debug.enabled);
        ImGui::SameLine();
        ImGui::Checkbox("Zero when stopped", &ctx.nes->apu.triangle.debug.output_zero_when_stopped);
        ImGui::Image(ctx.debug_apu.triangle_image.texture, { window_width, ctx.debug_apu.triangle_image.height });

        ImGui::Checkbox("Noise", &ctx.nes->apu.noise.debug.enabled);
        ImGui::Image(ctx.debug_apu.noise_image.texture, { window_width, ctx.debug_apu.noise_image.height });

        ImGui::Checkbox("DMC", &ctx.nes->apu.dmc.debug.enabled);
        ImGui::Image(ctx.debug_apu.dmc_image.texture, { window_width, ctx.debug_apu.dmc_image.height });

        ImGui::Checkbox("Mix", &ctx.nes->apu.debug.enabled);
        ImGui::Image(ctx.debug_apu.mixer_image.texture, { window_width, ctx.debug_apu.mixer_image.height });
    }
    ImGui::End();
}

static void show_screen()
{
    ctx.debug_control.pixel_trace = std::nullopt;
    if (ImGui::Begin("DISPLAY"))
    {
        write_texture(ctx.screen_texture, ctx.nes->screen_buffer, [](uint8_t* pixels, int width, int height, int pitch)
        {
            if (ctx.debug_control.show_grid)
            {
                for (int y = 0; y < height; y++)
                {
                    colour_t* row = (colour_t*)(pixels + y * pitch);
                    for (int x = 0; x < width; x++)
                    {
                        if ((x + y) % 2 == 0)
                        {
                            row[x].r += 16;
                            row[x].g += 16;
                            row[x].b += 16;
                        }
                        if (x % 8 == 0 || y % 8 == 0)
                        {
                            row[x].r += 16;
                            row[x].g += 16;
                            row[x].b += 16;
                        }
                    }
                }
            }

            if (ctx.debug_control.show_sprite_zero_hit)
            {
                auto& x = ctx.nes->ppu.debug.sprite_zero_hit_dot;
                auto& y = ctx.nes->ppu.debug.sprite_zero_hit_scanline;
                if (x && y && (SDL_GetTicks() / 100) % 2 == 0)
                {
                    ((colour_t*)(pixels + *y * pitch))[*x] = { 255, 0, 0, 255 };
                    x = std::nullopt;
                    y = std::nullopt;
                }
            }
        });
        draw_centred_image(ctx.screen_texture);
        if (ImGui::IsItemHovered() && ctx.debug_control.show_pixel_trace)
        {
            int x = (int)((ImGui::GetIO().MousePos.x - ImGui::GetItemRectMin().x)
                * nes::ppu_t::SCREEN_WIDTH
                / (ImGui::GetItemRectMax().x - ImGui::GetItemRectMin().x));
            int y = (int)((ImGui::GetIO().MousePos.y - ImGui::GetItemRectMin().y)
                * nes::ppu_t::SCREEN_HEIGHT
                / (ImGui::GetItemRectMax().y - ImGui::GetItemRectMin().y));
            x = std::clamp(x, 0, nes::ppu_t::SCREEN_WIDTH - 1);
            y = std::clamp(y, 0, nes::ppu_t::SCREEN_HEIGHT - 1);

            ImGui::PopStyleVar();
            if (ImGui::BeginTooltip())
            {
                auto& trace = ctx.nes->ppu.debug.pixel_trace[y * nes::ppu_t::SCREEN_WIDTH + x];
                if (trace.slot == 0)
                {
                    ImGui::Text("No pixel data");
                }
                else
                {
                    auto& slot = trace.slots[trace.slot - 1];
                    ctx.debug_control.pixel_trace = slot;
                    ImGui::Text("Position: (%d, %d)", x, y);

                    draw_separator();
                    ImGui::Text("Background");
                    ImGui::Text("  Tile index:    0x%02X", slot.bg.tile_index);
                    ImGui::Text("  Pattern table: %s", slot.bg.pattern_table ? "Right" : "Left");
                    ImGui::Text("  Attribute:     %d", slot.bg.attribute);
                    ImGui::Text("  Hidden:        %s", slot.bg.hidden ? "Yes" : "No");
                    
                    if (slot.fg.exists)
                    {
                        draw_separator();
                        ImGui::Text("Sprite");
                        ImGui::Text("  Sprite index:  0x%02X", slot.fg.sprite_index);
                        ImGui::Text("  Tile index:    0x%02X", slot.fg.tile_index);
                        ImGui::Text("  Pattern table: %s", slot.fg.pattern_table ? "Right" : "Left");
                        ImGui::Text("  Attribute:     %d", slot.fg.attribute);
                        ImGui::Text("  Priority:      %s", slot.fg.priority ? "Behind BG" : "Over BG");
                        ImGui::Text("  Size:          %s", slot.fg.is_8x16 ? "8x16" : "8x8");
                        ImGui::Text("  Flip H:        %s", slot.fg.flip_horizontally ? "Yes" : "No");
                        ImGui::Text("  Flip V:        %s", slot.fg.flip_vertically ? "Yes" : "No");
                    }
                }
                ImGui::EndTooltip();
            }
            ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2{0, 1});
        }
    }
    ImGui::End();
}

static void handle_key_up(SDL_KeyboardEvent key)
{
    switch (key.key)
    {
    case SDLK_A:
        ctx.nes->controller.status[0].left = false;
        break;
    case SDLK_D:
        ctx.nes->controller.status[0].right = false;
        break;
    case SDLK_W:
        ctx.nes->controller.status[0].up = false;
        break;
    case SDLK_S:
        ctx.nes->controller.status[0].down = false;
        break;
    case SDLK_SEMICOLON:
        ctx.nes->controller.status[0].b = false;
        break;
    case SDLK_APOSTROPHE:
        ctx.nes->controller.status[0].a = false;
        break;
    case SDLK_RSHIFT:
        ctx.nes->controller.status[0].select = false;
        break;
    case SDLK_RETURN:
        ctx.nes->controller.status[0].start = false;
        break;
    case SDLK_LCTRL:
        ctx.debug_control.show_pixel_trace &= ~1;
        break;
    case SDLK_RCTRL:
        ctx.debug_control.show_pixel_trace &= ~2;
        break;
    }
}

static void handle_key_down(SDL_KeyboardEvent key)
{
    bool ctrl = (key.mod & (SDL_KMOD_LCTRL | SDL_KMOD_RCTRL)) != 0;
    bool alt = (key.mod & (SDL_KMOD_LALT | SDL_KMOD_RALT)) != 0;
    bool shift = (key.mod & (SDL_KMOD_LSHIFT | SDL_KMOD_RSHIFT)) != 0;
    switch (key.key)
    {
    case SDLK_A:
        ctx.nes->controller.status[0].left = true;
        break;
    case SDLK_D:
        ctx.nes->controller.status[0].right = true;
        break;
    case SDLK_W:
        if (ctrl)
        {
            do_action(action_t::unload_cart);
        }
        else
        {
            ctx.nes->controller.status[0].up = true;
        }
        break;
    case SDLK_S:
        ctx.nes->controller.status[0].down = true;
        break;
    case SDLK_SEMICOLON:
        ctx.nes->controller.status[0].b = true;
        break;
    case SDLK_APOSTROPHE:
        ctx.nes->controller.status[0].a = true;
        break;
    case SDLK_RSHIFT:
        ctx.nes->controller.status[0].select = true;
        break;
    case SDLK_RETURN:
        if (alt)
        {
            ctx.debug_control.fullscreen = !ctx.debug_control.fullscreen;
        }
        else
        {
            ctx.nes->controller.status[0].start = true;
        }
        break;
    case SDLK_R:
        if (ctrl)
        {
            do_action(action_t::reset);
        }
        break;
    case SDLK_P:
        if (ctrl)
        {
            ctx.debug_control.pause = !ctx.debug_control.pause;
        }
        break;
    case SDLK_F10:
        do_action(shift ? action_t::clock_scanline : action_t::clock_frame);
        break;
    case SDLK_F11:
        do_action(shift ? action_t::clock_dot : action_t::clock_instruction);
        break;
    case SDLK_LCTRL:
        ctx.debug_control.show_pixel_trace |= 1;
        break;
    case SDLK_RCTRL:
        ctx.debug_control.show_pixel_trace |= 2;
        break;
    }
}

static void on_clock()
{
    constexpr float TICKS_PER_SAMPLE = 5369319 / AUDIO_SAMPLE_RATE;
    ctx.audio_tick_counter++;
    float ticks_per_sample = TICKS_PER_SAMPLE * ctx.debug_control.emulation_speed;
    if (ctx.audio_tick_counter < ticks_per_sample)
    {
        return;
    }
    ctx.audio_tick_counter -= ticks_per_sample;

    float mixed_sample = ctx.nes->apu.get_mixed_sample();
    ctx.debug_apu.pulse1_history.pop_front();
    ctx.debug_apu.pulse1_history.push_back(ctx.nes->apu.pulse1.get_sample());
    ctx.debug_apu.pulse2_history.pop_front();
    ctx.debug_apu.pulse2_history.push_back(ctx.nes->apu.pulse2.get_sample());
    ctx.debug_apu.triangle_history.pop_front();
    ctx.debug_apu.triangle_history.push_back(ctx.nes->apu.triangle.get_sample());
    ctx.debug_apu.noise_history.pop_front();
    ctx.debug_apu.noise_history.push_back(ctx.nes->apu.noise.get_sample());
    ctx.debug_apu.dmc_history.pop_front();
    ctx.debug_apu.dmc_history.push_back(ctx.nes->apu.dmc.get_sample());
    ctx.debug_apu.mixer_history.pop_front();
    ctx.debug_apu.mixer_history.push_back(mixed_sample);
    ctx.new_samples.push_back(mixed_sample * 2.0f - 1.0f);
}

static void load_rom(const char* rom_file)
{
    std::ifstream rom(rom_file, std::ios::binary | std::ios::ate);
    size_t rom_size = rom.tellg();
    rom.seekg(0, std::ios::beg);
    if (!rom.is_open() || rom_size == 0)
    {
        SPDLOG_ERROR("Failed to open ROM file: {}", rom_file);
        return;
    }
    std::vector<uint8_t> rom_data(rom_size);
    rom.read((char*)rom_data.data(), rom_size);

    auto cart = nes::cart_t::load(std::move(rom_data));
    if (!cart)
    {
        SPDLOG_ERROR("Failed to load ROM file: {}", rom_file);
        return;
    }
    ctx.nes->load_cart(std::move(cart));

    std::filesystem::path path(rom_file);
    ctx.debug_control.cart_name = path.stem().string();
}

int main(int argc, char* argv[])
{
    auto logger = std::make_shared<spdlog::logger>("logger", spdlog::sinks_init_list{
        std::make_shared<spdlog::sinks::callback_sink_st>([](const spdlog::details::log_msg& msg)
        {
            static spdlog::pattern_formatter formatter("%H:%M:%S.%e [%l] %v");
            spdlog::memory_buf_t formatted;
            formatter.format(msg, formatted);

            log_msg_t log;
            log.text = std::string(formatted.data(), formatted.size());

            log.colour = ImVec4(1.0f, 1.0f, 1.0f, 1.0f);
            switch (msg.level)
            {
            case spdlog::level::trace:    log.colour = ImVec4(0.5f, 0.5f, 0.5f, 1.0f); break;
            case spdlog::level::debug:    log.colour = ImVec4(0.0f, 1.0f, 1.0f, 1.0f); break;
            case spdlog::level::info:     log.colour = ImVec4(0.0f, 1.0f, 0.0f, 1.0f); break;
            case spdlog::level::warn:     log.colour = ImVec4(1.0f, 1.0f, 0.0f, 1.0f); break;
            case spdlog::level::err:      log.colour = ImVec4(1.0f, 0.0f, 0.0f, 1.0f); break;
            case spdlog::level::critical: log.colour = ImVec4(1.0f, 0.0f, 1.0f, 1.0f); break;
            }

            if (ctx.debug_log.logs.size() >= MAX_DEBUG_LOG_LINES)
            {
                ctx.debug_log.logs.pop_front();
            }
            ctx.debug_log.logs.push_back(std::move(log));
            ctx.debug_log.has_new_log = true;
        }),
        // std::make_shared<spdlog::sinks::stdout_color_sink_mt>(),
    });
    spdlog::set_default_logger(logger);

    ctx.nes = std::make_unique<nes::nes_t>(on_clock);

    if (argc >= 2)
    {
        const char* rom_file = argv[1];
        load_rom(rom_file);
    }

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
    ctx.window = SDL_CreateWindow(
        "NES",
        1280,
        720,
        window_flags);
    if (ctx.window == nullptr)
    {
        SPDLOG_ERROR("Error: SDL_CreateWindow(): {}", SDL_GetError());
        return -1;
    }

    ctx.renderer = SDL_CreateRenderer(ctx.window, nullptr);
    SDL_SetRenderVSync(ctx.renderer, 1);
    if (ctx.renderer == nullptr)
    {
        SPDLOG_ERROR("Error: SDL_CreateRenderer(): {}", SDL_GetError());
        return -1;
    }
    SDL_SetWindowPosition(
        ctx.window,
        SDL_WINDOWPOS_CENTERED,
        SDL_WINDOWPOS_CENTERED);
    SDL_ShowWindow(ctx.window);

    ctx.screen_texture = create_texture(
        nes::ppu_t::SCREEN_WIDTH,
        nes::ppu_t::SCREEN_HEIGHT);
    if (ctx.screen_texture == nullptr)
    {
        SPDLOG_ERROR("Error: ctx.screen_texture create_texture(): {}", SDL_GetError());
        return -1;
    }

    for (auto& image : ctx.debug_palette.images)
    {
        if (!create_texture(image))
        {
            SPDLOG_ERROR("Error: debug_palette create_texture(): {}", SDL_GetError());
            return -1;
        }
    }
    for (auto& image : ctx.debug_sprites.images)
    {
        if (!create_texture(image))
        {
            SPDLOG_ERROR("Error: debug_sprites create_texture(): {}", SDL_GetError());
            return -1;
        }
    }
    if (!create_texture(ctx.debug_pattern_table.image))
    {
        SPDLOG_ERROR("Error: debug_pattern_table create_texture(): {}", SDL_GetError());
        return -1;
    }
    if (!create_texture(ctx.debug_nametable.image))
    {
        SPDLOG_ERROR("Error: debug_nametable create_texture(): {}", SDL_GetError());
        return -1;
    }
    if (!create_texture(ctx.debug_apu.pulse1_image))
    {
        SPDLOG_ERROR("Error: debug_pulse1 create_texture(): {}", SDL_GetError());
        return -1;
    }
    if (!create_texture(ctx.debug_apu.pulse2_image))
    {
        SPDLOG_ERROR("Error: debug_pulse2 create_texture(): {}", SDL_GetError());
        return -1;
    }
    if (!create_texture(ctx.debug_apu.triangle_image))
    {
        SPDLOG_ERROR("Error: debug_triangle create_texture(): {}", SDL_GetError());
        return -1;
    }
    if (!create_texture(ctx.debug_apu.noise_image))
    {
        SPDLOG_ERROR("Error: debug_noise create_texture(): {}", SDL_GetError());
        return -1;
    }
    if (!create_texture(ctx.debug_apu.dmc_image))
    {
        SPDLOG_ERROR("Error: debug_dmc create_texture(): {}", SDL_GetError());
        return -1;
    }
    if (!create_texture(ctx.debug_apu.mixer_image))
    {
        SPDLOG_ERROR("Error: debug_mixer create_texture(): {}", SDL_GetError());
        return -1;
    }

    ctx.audio_device = SDL_OpenAudioDevice(SDL_AUDIO_DEVICE_DEFAULT_PLAYBACK, &audio_spec);
    if (ctx.audio_device == 0)
    {
        SPDLOG_ERROR("Error: SDL_OpenAudioDevice(): {}", SDL_GetError());
        return -1;
    }
    ctx.audio_stream = SDL_CreateAudioStream(&audio_spec, &audio_spec);
    if (ctx.audio_stream == nullptr)
    {
        SPDLOG_ERROR("Error: SDL_CreateAudioStream(): {}", SDL_GetError());
        return -1;
    }
    if (!SDL_BindAudioStream(ctx.audio_device, ctx.audio_stream))
    {
        SPDLOG_ERROR("Error: SDL_BindAudioStream(): {}", SDL_GetError());
        return -1;
    }
    ctx.debug_apu.pulse1_history.resize(MIXER_HISTORY_LENGTH, 0);
    ctx.debug_apu.pulse2_history.resize(MIXER_HISTORY_LENGTH, 0);
    ctx.debug_apu.triangle_history.resize(MIXER_HISTORY_LENGTH, 0);
    ctx.debug_apu.noise_history.resize(MIXER_HISTORY_LENGTH, 0);
    ctx.debug_apu.dmc_history.resize(MIXER_HISTORY_LENGTH, 0);
    ctx.debug_apu.mixer_history.resize(MIXER_HISTORY_LENGTH, 0);

    // Initialise imgui

    ImGui::CreateContext();

    ImGuiIO& io = ImGui::GetIO();
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableGamepad;
    io.ConfigFlags |= ImGuiConfigFlags_DockingEnable;
    
    ImGuiStyle& style = ImGui::GetStyle();
    style.WindowMenuButtonPosition = ImGuiDir_None;
    style.Colors[ImGuiCol_WindowBg] = ImVec4(0.1f, 0.1f, 0.1f, 1.0f);

    ImGui_ImplSDL3_InitForSDLRenderer(ctx.window, ctx.renderer);
    ImGui_ImplSDLRenderer3_Init(ctx.renderer);
    ImGui::LoadIniSettingsFromMemory(IMGUI_DEFAULT_INI);

    // Main loop

    SPDLOG_INFO("Application initialised");
    bool running = true;
    uint64_t frames_run = SDL_GetTicks() * 60 / 1000;
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
                if (event.window.windowID == SDL_GetWindowID(ctx.window))
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
            case SDL_EVENT_DROP_FILE:
                load_rom(event.drop.data);
                break;
            }
        }

        // Run emulation

        uint64_t current_ticks = SDL_GetTicks();
        uint64_t frames_expected = current_ticks * 60 / 1000;
        if (!ctx.debug_control.pause && ctx.nes->cart)
        {
            for (int i = 0; i < 5 && frames_run < frames_expected; i++)
            {
                for (int j = 0; j < ctx.debug_control.emulation_speed; j++)
                {
                    ctx.nes->clock_frame();
                }
                frames_run++;
            }
        }
        frames_run = frames_expected;

        SDL_PutAudioStreamData(ctx.audio_stream, ctx.new_samples.data(), (int)ctx.new_samples.size() * sizeof(float));
        ctx.new_samples.clear();

        // Begin rendering

        ImGui_ImplSDLRenderer3_NewFrame();
        ImGui_ImplSDL3_NewFrame();
        ImGui::NewFrame();
        ImGui::DockSpaceOverViewport();
        SDL_RenderClear(ctx.renderer);

        // Render ImGui

        show_log();
        show_cpu_dism();
        show_ram();
        show_cpu_memory_map();
        show_ppu_memory_map();
        show_control();
        show_sprites();

        ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2{0, 1});
        show_palette();
        show_pattern_table();
        show_nametable();
        show_apu();
        show_screen();
        ImGui::PopStyleVar();

        // End rendering

        ImGui::Render();
        ImGui_ImplSDLRenderer3_RenderDrawData(ImGui::GetDrawData(), ctx.renderer);
        SDL_RenderPresent(ctx.renderer);

        bool is_fullscreen = SDL_GetWindowFlags(ctx.window) & SDL_WINDOW_FULLSCREEN;
        if (is_fullscreen != ctx.debug_control.fullscreen)
        {
            if (!SDL_SetWindowFullscreen(ctx.window, ctx.debug_control.fullscreen))
            {
                SPDLOG_ERROR("Error: SDL_SetWindowFullscreen(): {}", SDL_GetError());
                break;
            }
        }
    }

    SPDLOG_INFO("Application exiting");

    ImGui_ImplSDLRenderer3_Shutdown();
    ImGui_ImplSDL3_Shutdown();
    ImGui::DestroyContext();

    SDL_UnbindAudioStream(ctx.audio_stream);
    SDL_DestroyAudioStream(ctx.audio_stream);
    SDL_CloseAudioDevice(ctx.audio_device);
    for (auto& image : ctx.debug_palette.images)
    {
        SDL_DestroyTexture(image.texture);
    }
    for (auto& image : ctx.debug_sprites.images)
    {
        SDL_DestroyTexture(image.texture);
    }
    SDL_DestroyTexture(ctx.debug_pattern_table.image.texture);
    SDL_DestroyTexture(ctx.debug_nametable.image.texture);
    SDL_DestroyTexture(ctx.debug_apu.pulse1_image.texture);
    SDL_DestroyTexture(ctx.debug_apu.pulse2_image.texture);
    SDL_DestroyTexture(ctx.debug_apu.triangle_image.texture);
    SDL_DestroyTexture(ctx.debug_apu.noise_image.texture);
    SDL_DestroyTexture(ctx.debug_apu.dmc_image.texture);
    SDL_DestroyTexture(ctx.debug_apu.mixer_image.texture);
    SDL_DestroyTexture(ctx.screen_texture);
    SDL_DestroyRenderer(ctx.renderer);
    SDL_DestroyWindow(ctx.window);
    SDL_Quit();

    return 0;
}
