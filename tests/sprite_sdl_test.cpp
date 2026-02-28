// SDL3 visual test: loads pikachu sprite with palette colors
// Usage: ./sprite_sdl_test [--lua]
//   Without --lua: draws pikachu using C++ SpriteSheet API directly
//   With --lua:    pushes pikachu_data to Lua and runs scripts/pikachu_demo.lua

#include <SDL3/SDL.h>
#include <SDL3/SDL_main.h>
#include <cstring>
#include <cstdio>

#include <enjin2/graphics/canvas.hpp>
#include <enjin2/graphics/sprite.hpp>
#include <enjin2/graphics/palette.hpp>
#include <enjin2/input/input_state.hpp>

#ifdef ENJIN2_BUILD_LUA
#include <enjin2/scripting/bindings.hpp>
#include <enjin2/core/scene.hpp>
#include <iostream>
#endif

#include "pikachu.h"

// input_platform_poll is normally in sdl_main.cpp — provide our own for this test
namespace enjin2 {
void input_platform_poll(InputState* state) {
    const bool* keys = SDL_GetKeyboardState(nullptr);
    auto set_btn = [&](int btn, bool on) {
        if (on) state->buttons |= static_cast<uint16_t>(1u << btn);
    };
    set_btn(0, keys[SDL_SCANCODE_UP]    || keys[SDL_SCANCODE_W]);
    set_btn(1, keys[SDL_SCANCODE_DOWN]  || keys[SDL_SCANCODE_S]);
    set_btn(2, keys[SDL_SCANCODE_LEFT]  || keys[SDL_SCANCODE_A]);
    set_btn(3, keys[SDL_SCANCODE_RIGHT] || keys[SDL_SCANCODE_D]);
    set_btn(4, keys[SDL_SCANCODE_Z]);
    set_btn(5, keys[SDL_SCANCODE_X]);
    set_btn(6, keys[SDL_SCANCODE_RETURN]);
}
} // namespace enjin2

static constexpr int CANVAS_W      = 128;
static constexpr int CANVAS_H      = 128;
static constexpr int DEFAULT_SCALE = 4;
static constexpr int WIN_W         = CANVAS_W * DEFAULT_SCALE;
static constexpr int WIN_H         = CANVAS_H * DEFAULT_SCALE;

static enjin2::Canvas4<CANVAS_W, CANVAS_H> g_canvas;
static enjin2::InputState                   g_input{};
static uint8_t g_rgb_staging[CANVAS_W * CANVAS_H * 3];

#ifdef ENJIN2_BUILD_LUA
static enjin2::LuaScriptSystem g_lua;
static enjin2::LuaCanvas       g_lua_canvas(&g_canvas);

// Minimal scene so engine.scene.spawn/destroy/find work from Lua scripts
struct TestScene : enjin2::Scene {
    explicit TestScene() : Scene(0u) {}
};
static TestScene g_scene;
#endif

// Expand 4-bit canvas to RGB24 staging buffer via palette
static void expand_canvas_to_rgb() {
    for (int y = 0; y < CANVAS_H; y++) {
        for (int x = 0; x < CANVAS_W; x++) {
            enjin2::Pixel4 px = g_canvas.getPixel(x, y);
            int i = (y * CANVAS_W + x) * 3;
            if (!enjin2::g_palette.isTransparent(px.value)) {
                enjin2::RGB rgb = enjin2::g_palette.resolve(px.value);
                g_rgb_staging[i + 0] = rgb.r;
                g_rgb_staging[i + 1] = rgb.g;
                g_rgb_staging[i + 2] = rgb.b;
            } else {
                g_rgb_staging[i + 0] = 0;
                g_rgb_staging[i + 1] = 0;
                g_rgb_staging[i + 2] = 0;
            }
        }
    }
}

// C++ path: draw pikachu directly using SpriteSheet API
static void draw_cpp_path() {
    g_canvas.clear(enjin2::Pixel4(0));

    enjin2::SpriteSheet pikachu(pikachu_data, 38, 38, 1, 1);

    // Draw pikachu centered on the 128x128 canvas
    int16_t cx = static_cast<int16_t>((CANVAS_W - 38) / 2);
    int16_t cy = static_cast<int16_t>((CANVAS_H - 38) / 2);
    pikachu.draw(g_canvas, 0, cx, cy);

    // Draw a small palette strip at the bottom for visual verification
    for (int i = 0; i < 15; i++) {
        int16_t sx = static_cast<int16_t>(i * 8);
        int16_t sy = static_cast<int16_t>(CANVAS_H - 8);
        for (int16_t py = sy; py < sy + 8; py++) {
            for (int16_t px = sx; px < sx + 8; px++) {
                g_canvas.setPixel(px, py, enjin2::Pixel4(static_cast<uint8_t>(i)));
            }
        }
    }
}

int main(int argc, char* argv[]) {
    bool use_lua = false;
    const char* script_path = "scripts/pikachu_demo.lua";
    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "--lua") == 0) use_lua = true;
        if (strcmp(argv[i], "--script") == 0 && i + 1 < argc) {
            script_path = argv[++i];
            use_lua = true;  // --script implies --lua
        }
    }

    const char* title = use_lua
        ? "Enjin2 Sprite Test (Lua)"
        : "Enjin2 Sprite Test (C++)";

    printf("Sprite SDL3 test — mode: %s\n", use_lua ? "Lua" : "C++");
    printf("Pikachu: 38x38 sprite, 1 frame, palette indices 0-14\n");

    // SDL init
    if (!SDL_Init(SDL_INIT_VIDEO)) {
        SDL_Log("SDL_Init failed: %s", SDL_GetError());
        return 1;
    }

    SDL_Window*   window   = nullptr;
    SDL_Renderer* renderer = nullptr;
    if (!SDL_CreateWindowAndRenderer(title, WIN_W, WIN_H, 0, &window, &renderer)) {
        SDL_Log("SDL_CreateWindowAndRenderer failed: %s", SDL_GetError());
        SDL_Quit();
        return 1;
    }

    SDL_Texture* texture = SDL_CreateTexture(
        renderer, SDL_PIXELFORMAT_RGB24, SDL_TEXTUREACCESS_STREAMING,
        CANVAS_W, CANVAS_H);
    if (!texture) {
        SDL_Log("SDL_CreateTexture failed: %s", SDL_GetError());
        SDL_DestroyRenderer(renderer);
        SDL_DestroyWindow(window);
        SDL_Quit();
        return 1;
    }
    SDL_SetTextureScaleMode(texture, SDL_SCALEMODE_NEAREST);
    SDL_SetRenderScale(renderer,
        static_cast<float>(DEFAULT_SCALE),
        static_cast<float>(DEFAULT_SCALE));

    // Lua initialization (when --lua)
#ifdef ENJIN2_BUILD_LUA
    if (use_lua) {
        if (!g_lua.initialize()) {
            SDL_Log("Lua init failed");
            SDL_DestroyTexture(texture);
            SDL_DestroyRenderer(renderer);
            SDL_DestroyWindow(window);
            SDL_Quit();
            return 1;
        }
        g_lua.setCanvas(&g_lua_canvas);
        g_lua.getBindings().setInput(&g_input);

        // Wire scene so engine.scene.spawn/destroy/find work
        g_scene.initialize();
        g_scene.activate();
        g_lua.getBindings().setActiveScene(&g_scene);
        g_lua.getBindings().setAssetPath("tests");

        // Push pikachu_data as a lightuserdata global accessible to Lua
        lua_State* L = g_lua.getEngine().getState();
        lua_pushlightuserdata(L, const_cast<uint8_t*>(pikachu_data));
        lua_setglobal(L, "PIKACHU_DATA");

        // Also set dimensions as globals for convenience
        g_lua.getEngine().setGlobal("PIKACHU_W", 38.0);
        g_lua.getEngine().setGlobal("PIKACHU_H", 38.0);

        enjin2::LuaResult r = g_lua.loadScript(script_path);
        if (!r.success) {
            std::cerr << "Lua load error: " << r.error << "\n";
            g_canvas.clear(enjin2::Pixel4(14));
        }
    }
#else
    if (use_lua) {
        printf("Warning: --lua requested but ENJIN2_BUILD_LUA is OFF. Falling back to C++ path.\n");
        use_lua = false;
    }
#endif

    // Render loop
    bool running = true;
    Uint64 prev_ticks = SDL_GetTicks();

    while (running) {
        Uint64 frame_start = SDL_GetTicks();

        SDL_Event event;
        while (SDL_PollEvent(&event)) {
            if (event.type == SDL_EVENT_QUIT) running = false;
            else if (event.type == SDL_EVENT_KEY_DOWN && event.key.key == SDLK_ESCAPE)
                running = false;
        }
        if (!running) break;

        float dt = static_cast<float>(frame_start - prev_ticks) / 1000.0f;
        if (dt > 0.133f) dt = 0.133f;
        prev_ticks = frame_start;

        if (!use_lua) {
            // C++ path: draw directly
            draw_cpp_path();
        }
#ifdef ENJIN2_BUILD_LUA
        else {
            // Lua path: call update + draw
            enjin2::input_advance_frame(&g_input);
            enjin2::input_platform_poll(&g_input);
            g_lua.getBindings().setInput(&g_input);

            enjin2::LuaResult r1 = g_lua.callFunction("update", dt);
            if (!r1.success) std::cerr << "Lua update: " << r1.error << "\n";

            enjin2::LuaResult r2 = g_lua.callFunction("draw");
            if (!r2.success) std::cerr << "Lua draw: " << r2.error << "\n";
        }
#endif

        expand_canvas_to_rgb();
        SDL_UpdateTexture(texture, nullptr, g_rgb_staging, CANVAS_W * 3);
        SDL_RenderClear(renderer);
        SDL_RenderTexture(renderer, texture, nullptr, nullptr);
        SDL_RenderPresent(renderer);

        Uint64 elapsed = SDL_GetTicks() - frame_start;
        if (elapsed < 33) SDL_Delay(static_cast<Uint32>(33 - elapsed));
    }

#ifdef ENJIN2_BUILD_LUA
    if (use_lua) g_lua.shutdown();
#endif
    SDL_DestroyTexture(texture);
    SDL_DestroyRenderer(renderer);
    SDL_DestroyWindow(window);
    SDL_Quit();

    return 0;
}
