#include <SDL3/SDL.h>
#include <SDL3/SDL_main.h>
#include <cstdlib>
#include <cstring>

#include <enjin2/graphics/canvas.hpp>
#include <enjin2/graphics/palette.hpp>
#include <enjin2/input/input_state.hpp>

#ifdef ENJIN2_BUILD_LUA
#include <enjin2/scripting/bindings.hpp>
#include <iostream>
#endif

// ---------------------------------------------------------------------------
// Constants
// ---------------------------------------------------------------------------

static constexpr int CANVAS_W      = 128;
static constexpr int CANVAS_H      = 128;
static constexpr int DEFAULT_SCALE = 4;
static constexpr int WIN_W         = CANVAS_W * DEFAULT_SCALE; // 512
static constexpr int WIN_H         = CANVAS_H * DEFAULT_SCALE; // 512
static constexpr int DEFAULT_FPS   = 30;

// ---------------------------------------------------------------------------
// Button indices — local enum, not exported (INP-04 contract)
// ---------------------------------------------------------------------------

enum : int {
    BTN_UP    = 0,
    BTN_DOWN  = 1,
    BTN_LEFT  = 2,
    BTN_RIGHT = 3,
    BTN_A     = 4,  // Z key
    BTN_B     = 5,  // X key
    BTN_START = 6   // Enter key
};

// ---------------------------------------------------------------------------
// Static globals — no heap allocation
// ---------------------------------------------------------------------------

static enjin2::Canvas4<CANVAS_W, CANVAS_H> g_canvas;
static enjin2::InputState                   g_input{};
static uint8_t g_rgb_staging[CANVAS_W * CANVAS_H * 3];

#ifdef ENJIN2_BUILD_LUA
static enjin2::LuaScriptSystem g_lua;
static enjin2::LuaCanvas       g_lua_canvas(&g_canvas);
#endif

// ---------------------------------------------------------------------------
// input_platform_poll — SDL3 keyboard -> InputState mapping (INP-04)
// Use SDL_GetKeyboardState (not EVENT_KEY_DOWN) for held-key detection.
// WASD mirrors directional arrows to the same button bits.
// ---------------------------------------------------------------------------

namespace enjin2 {
void input_platform_poll(InputState* state) {
    const bool* keys = SDL_GetKeyboardState(nullptr);
    auto set_btn = [&](int btn, bool on) {
        if (on) state->buttons |= static_cast<uint16_t>(1u << btn);
    };
    set_btn(BTN_UP,    keys[SDL_SCANCODE_UP]    || keys[SDL_SCANCODE_W]);
    set_btn(BTN_DOWN,  keys[SDL_SCANCODE_DOWN]  || keys[SDL_SCANCODE_S]);
    set_btn(BTN_LEFT,  keys[SDL_SCANCODE_LEFT]  || keys[SDL_SCANCODE_A]);
    set_btn(BTN_RIGHT, keys[SDL_SCANCODE_RIGHT] || keys[SDL_SCANCODE_D]);
    set_btn(BTN_A,     keys[SDL_SCANCODE_Z]);
    set_btn(BTN_B,     keys[SDL_SCANCODE_X]);
    set_btn(BTN_START, keys[SDL_SCANCODE_RETURN]);
}
} // namespace enjin2

// ---------------------------------------------------------------------------
// expand_canvas_to_rgb — Canvas4 palette-expanded -> RGB24 staging buffer
// ---------------------------------------------------------------------------

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

// ---------------------------------------------------------------------------
// main
// ---------------------------------------------------------------------------

int main(int argc, char* argv[]) {
    // --- Parse --fps N ---
    int fps = DEFAULT_FPS;
    for (int i = 1; i < argc - 1; i++) {
        if (strcmp(argv[i], "--fps") == 0) {
            int requested = atoi(argv[i + 1]);
            if (requested >= 1 && requested <= 300) {
                fps = requested;
            }
        }
    }

    const float frame_ms = 1000.0f / static_cast<float>(fps);
    // max_dt = 4-frame ceiling for delta-time clamping
    const float max_dt   = 4.0f / static_cast<float>(fps);

    // --- SDL init ---
    if (!SDL_Init(SDL_INIT_VIDEO)) {
        SDL_Log("SDL_Init failed: %s", SDL_GetError());
        return 1;
    }

    SDL_Window*   window   = nullptr;
    SDL_Renderer* renderer = nullptr;
    if (!SDL_CreateWindowAndRenderer("Enjin2", WIN_W, WIN_H, 0, &window, &renderer)) {
        SDL_Log("SDL_CreateWindowAndRenderer failed: %s", SDL_GetError());
        SDL_Quit();
        return 1;
    }

    // --- Streaming texture for canvas (RGB24, CANVAS_W x CANVAS_H) ---
    SDL_Texture* texture = SDL_CreateTexture(
        renderer,
        SDL_PIXELFORMAT_RGB24,
        SDL_TEXTUREACCESS_STREAMING,
        CANVAS_W,
        CANVAS_H
    );
    if (!texture) {
        SDL_Log("SDL_CreateTexture failed: %s", SDL_GetError());
        SDL_DestroyRenderer(renderer);
        SDL_DestroyWindow(window);
        SDL_Quit();
        return 1;
    }

    // MUST be set immediately after creation — SDL3 defaults to bilinear
    SDL_SetTextureScaleMode(texture, SDL_SCALEMODE_NEAREST);

    // Integer 4x scale via render scale (SDL_SetRenderLogicalPresentation has
    // a known SDL3 bug #11335 that ignores scale mode)
    SDL_SetRenderScale(renderer,
        static_cast<float>(DEFAULT_SCALE),
        static_cast<float>(DEFAULT_SCALE));

    // --- Lua initialization ---
#ifdef ENJIN2_BUILD_LUA
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
    {
        enjin2::LuaResult load_result = g_lua.loadScript("scripts/e2e_parity.lua");
        if (!load_result.success) {
            std::cerr << "Lua script load error: " << load_result.error << "\n";
            g_canvas.clear(enjin2::Pixel4(14));
        }
    }
#endif

    // --- Game loop ---
    bool running = true;
    Uint64 prev_ticks = SDL_GetTicks();

    while (running) {
        Uint64 frame_start = SDL_GetTicks();

        // --- Event pump (drain ALL events first, THEN call GetKeyboardState) ---
        SDL_Event event;
        while (SDL_PollEvent(&event)) {
            if (event.type == SDL_EVENT_QUIT) {
                running = false;
            } else if (event.type == SDL_EVENT_KEY_DOWN) {
                if (event.key.key == SDLK_ESCAPE) {
                    running = false;
                }
            }
        }

        if (!running) break;

        // --- Compute delta-time ---
        float dt = static_cast<float>(frame_start - prev_ticks) / 1000.0f;
        if (dt > max_dt) dt = max_dt;
        prev_ticks = frame_start;

        // --- Input: advance frame (snapshot prev), then poll current ---
        // Order MUST be: advance first (clears current), then poll (writes current).
        // Reversing would cause all inputs to appear as justReleased every frame.
        enjin2::input_advance_frame(&g_input);
        enjin2::input_platform_poll(&g_input);

        // --- Lua per-frame calls ---
#ifdef ENJIN2_BUILD_LUA
        g_lua.getBindings().setInput(&g_input);  // wire current-frame input AFTER poll
        {
            enjin2::LuaResult r = g_lua.callFunction("update", dt);
            if (!r.success) {
                std::cerr << "Lua update error: " << r.error << "\n";
                g_canvas.clear(enjin2::Pixel4(14));
            }
        }
        {
            enjin2::LuaResult r = g_lua.callFunction("draw");
            if (!r.success) {
                std::cerr << "Lua draw error: " << r.error << "\n";
                g_canvas.clear(enjin2::Pixel4(14));
            }
        }
#endif

        // --- Render ---
        expand_canvas_to_rgb();

        // Upload RGB24 staging buffer; pitch = CANVAS_W * 3 (3 bytes/pixel, not 4)
        SDL_UpdateTexture(texture, nullptr, g_rgb_staging, CANVAS_W * 3);

        SDL_RenderClear(renderer);
        SDL_RenderTexture(renderer, texture, nullptr, nullptr);
        SDL_RenderPresent(renderer);

        // --- Frame pacing ---
        Uint64 elapsed_ms = SDL_GetTicks() - frame_start;
        if (static_cast<float>(elapsed_ms) < frame_ms) {
            SDL_Delay(static_cast<Uint32>(frame_ms - static_cast<float>(elapsed_ms)));
        }
    }

    // --- Clean shutdown ---
#ifdef ENJIN2_BUILD_LUA
    g_lua.shutdown();
#endif
    SDL_DestroyTexture(texture);
    SDL_DestroyRenderer(renderer);
    SDL_DestroyWindow(window);
    SDL_Quit();

    return 0;
}
