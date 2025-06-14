#include "pch.h"

#include <SDL3/SDL_main.h>
#include <SDL3/SDL.h>

#include "imgui.h"
#include "imgui_impl_sdl3.h"
#include "imgui_impl_sdlrenderer3.h"

#include "spdlog/spdlog.h"

#include "nes.h"

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

int main(int argc, char* argv[])
{
    // Initialise SDL

    if (!SDL_Init(SDL_INIT_VIDEO | SDL_INIT_GAMEPAD))
    {
        SPDLOG_ERROR("Failed to initialize SDL: {}", SDL_GetError());
        return -1;
    }

    SDL_WindowFlags window_flags = SDL_WINDOW_RESIZABLE
                                 | SDL_WINDOW_HIDDEN
                                 | SDL_WINDOW_HIGH_PIXEL_DENSITY;
    SDL_Window* window = SDL_CreateWindow(
        "NES",
        nes::nes_t::SCREEN_WIDTH,
        nes::nes_t::SCREEN_HEIGHT,
        window_flags);
    if (window == nullptr)
    {
        SPDLOG_ERROR("Error: SDL_CreateWindow(): {}", SDL_GetError());
        return -1;
    }

    SDL_Renderer* renderer = SDL_CreateRenderer(window, nullptr);
    SDL_SetRenderVSync(renderer, 1);
    if (renderer == nullptr)
    {
        SPDLOG_ERROR("Error: SDL_CreateRenderer(): {}", SDL_GetError());
        return -1;
    }
    SDL_SetWindowPosition(window, SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED);
    SDL_ShowWindow(window);
    SDL_SetWindowAspectRatio(
        window,
        nes::nes_t::SCREEN_ASPECT,
        nes::nes_t::SCREEN_ASPECT);
    SDL_SetRenderLogicalPresentation(
        renderer,
        (int)(1080 * nes::nes_t::SCREEN_ASPECT),
        1080,
        SDL_LOGICAL_PRESENTATION_LETTERBOX);

    SDL_Texture* nes_screen_texture = SDL_CreateTexture(
        renderer,
        SDL_PIXELFORMAT_RGBA32,
        SDL_TEXTUREACCESS_STREAMING,
        nes::nes_t::SCREEN_WIDTH,
        nes::nes_t::SCREEN_HEIGHT);
    if (nes_screen_texture == nullptr)
    {
        SPDLOG_ERROR("Error: SDL_CreateTexture(): {}", SDL_GetError());
        return -1;
    }
    SDL_SetTextureScaleMode(nes_screen_texture, SDL_SCALEMODE_NEAREST);

    // Initialise imgui

    ImGui::CreateContext();
    ImGuiIO& io = ImGui::GetIO();
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableGamepad;

    ImGui_ImplSDL3_InitForSDLRenderer(window, renderer);
    ImGui_ImplSDLRenderer3_Init(renderer);

    // Main loop

    auto nes = std::make_unique<nes::nes_t>();

    SPDLOG_INFO("Application initialised");
    bool running = true;
    while (running)
    {
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
                if (event.window.windowID == SDL_GetWindowID(window))
                {
                    running = false;
                }
                break;
            case SDL_EVENT_KEY_DOWN:
                if (event.key.key == SDLK_RETURN
                    && (event.key.mod == SDL_KMOD_LALT
                      || event.key.mod == SDL_KMOD_RALT))
                {
                    bool is_fullscreen = SDL_GetWindowFlags(window)
                        & SDL_WINDOW_FULLSCREEN;
                    SDL_SetWindowFullscreen(window, !is_fullscreen);
                }
            }
        }

        nes->clock_frame();

        void* pixels;
        int pitch;
        SDL_LockTexture(nes_screen_texture, nullptr, &pixels, &pitch);
        for (int y = 0; y < nes::nes_t::SCREEN_HEIGHT; y++)
        {
            uint8_t* row = (uint8_t*)pixels + y * pitch;
            for (int x = 0; x < nes::nes_t::SCREEN_WIDTH; x++)
            {
                uint8_t color_index =
                    nes->screen_buffer[y * nes::nes_t::SCREEN_WIDTH + x];
                const colour_t& colour = NTSC_PALETTE[color_index % 64];
                memcpy(row + x * 4, &colour, sizeof(colour));
            }
        }
        SDL_UnlockTexture(nes_screen_texture);

        ImGui_ImplSDLRenderer3_NewFrame();
        ImGui_ImplSDL3_NewFrame();
        ImGui::NewFrame();

        ImGui::Render();
        SDL_RenderClear(renderer);
        SDL_RenderTexture(renderer, nes_screen_texture, nullptr, nullptr);
        ImGui_ImplSDLRenderer3_RenderDrawData(ImGui::GetDrawData(), renderer);
        SDL_RenderPresent(renderer);
    }

    SPDLOG_INFO("Application exiting");

    ImGui_ImplSDLRenderer3_Shutdown();
    ImGui_ImplSDL3_Shutdown();
    ImGui::DestroyContext();

    SDL_DestroyRenderer(renderer);
    SDL_DestroyWindow(window);
    SDL_Quit();

    return 0;
}
