#include <emscripten/bind.h>

#include "../../include/enjin2/graphics/canvas.hpp"
#include "../../include/enjin2/graphics/palette.hpp"
#include "../../include/enjin2/graphics/primitives.hpp"
#include "../../include/enjin2/input/input_state.hpp"
#include "../../include/enjin2/ui/components.hpp"
#include "../../include/enjin2/ui/scene_player.hpp"
#include "../../include/enjin2/ui/widgets/gauge.hpp"
#include "../../include/enjin2/ui/widgets/label.hpp"
#include "../../include/enjin2/ui/world.hpp"

#include <algorithm>
#include <cmath>
#include <cstdint>

// Configurable canvas dimensions — set via CMake compile definitions,
// fall back to 128x128 for standalone compilation.
#ifndef ENJIN2_CANVAS_WIDTH
#define ENJIN2_CANVAS_WIDTH 128
#endif
#ifndef ENJIN2_CANVAS_HEIGHT
#define ENJIN2_CANVAS_HEIGHT 128
#endif

using namespace emscripten;
using namespace enjin2;

/**
 * @file editor_preview.cpp
 * @brief Scene-editor M0 embind surface: init / tick / getFramebuffer / injectInput
 *
 * The Eisei scene editor's preview pane (unwn ADR-0006) drives the engine through
 * a handful of embind entry points instead of a scripting VM. This file provides
 * the M0 skeleton of that surface: a C++-owned demo scene rendered by the same
 * ui-ECS widget systems and primitives the firmware uses, exposed to JS as
 *
 *   init()                          -> bool   (build the demo scene, idempotent)
 *   tick(dtSeconds)                 -> int    (advance + render one frame, returns frame index)
 *   getFramebuffer()                -> Uint8Array view, one 4-bit value (0-15) per pixel
 *   injectInput(buttons, ax0, ay0)  -> void   (per-frame input, call BEFORE tick)
 *
 * Everything here is Lua-free by design — this surface is what a
 * `-DENJIN2_BUILD_LUA=OFF` WASM build exports (plus the palette/canvas-size
 * helpers from emscripten_bindings.cpp).
 *
 * M2 (unwn #184) adds the scene-file surface on top, backed by the shared
 * enjin2::ScenePlayer rig — the same header eisei_preview compiles natively,
 * so the CI parity goldens compare the toolchains, never two wirings:
 *
 *   loadScene(jsonText)             -> bool   (versioned scene JSON, ADR-0005;
 *                                              dispatches scene.activate)
 *   sceneActive()                   -> bool
 *   sceneDispatch(name, jsonText)   -> void   (event into the tables, "" = no payload)
 *   sceneTick()                     -> void   (advance + render one fixed 16 ms frame)
 *   getSceneFramebuffer()           -> Uint8Array view, the PACKED 127x127
 *                                     Canvas4 buffer (8128 bytes) — byte-for-byte
 *                                     the native golden .bin payload
 */

namespace {

using PreviewCanvas = Canvas4<ENJIN2_CANVAS_WIDTH, ENJIN2_CANVAS_HEIGHT>;
using PreviewWorld =
    World<8, PositionComponent, SizeComponent, GaugeComponent, LabelComponent>;

PreviewCanvas g_canvas;
PreviewWorld g_world;
GaugeSystem<PreviewWorld, PreviewCanvas> g_gaugeSystem(&g_world, &g_canvas);
LabelSystem<PreviewWorld, PreviewCanvas> g_labelSystem(&g_world, &g_canvas);

Entity g_gauge{};
InputState g_input{};
float g_time = 0.0f;
uint32_t g_frame = 0;
bool g_initialized = false;

// Fixed star positions for the backdrop (x, y, phase offset).
constexpr struct { uint8_t x, y, phase; } kStars[] = {
    {9, 14, 0},   {31, 6, 3},   {54, 19, 6},  {77, 9, 1},   {103, 15, 4},
    {119, 30, 7}, {14, 44, 2},  {116, 58, 5}, {6, 76, 3},   {121, 88, 0},
    {18, 105, 6}, {44, 118, 2}, {73, 112, 5}, {99, 108, 1}, {113, 119, 4},
    {36, 98, 7},
};

bool previewInit() {
    if (g_initialized) return true;

    // Center gauge — the Eisei "planet": bidirectional VU meter.
    g_gauge = g_world.create();
    g_world.add<PositionComponent>(g_gauge, Point(36, 36));
    g_world.add<GaugeComponent>(g_gauge, static_cast<uint16_t>(56), Pixel4(13),
                                GaugeMode::Bidirectional);

    // Title label, transparent background, built-in 5x7 font.
    Entity label = g_world.create();
    g_world.add<PositionComponent>(label, Point(34, 4));
    g_world.add<SizeComponent>(label, Size(60, 14));
    auto* lc = g_world.add<LabelComponent>(label, std::string("EISEI"));
    if (lc) lc->setColor(Pixel4(12));

    g_initialized = true;
    return true;
}

int previewTick(float dt) {
    if (!g_initialized) previewInit();

    dt = std::clamp(dt, 0.0f, 0.1f);
    // Button 0 held = freeze time (proves the input round trip end to end).
    const bool paused = g_input.held(0);
    if (!paused) g_time += dt;
    ++g_frame;

    // Axis 0 biases the gauge; the rest of the motion is engine-driven.
    if (auto* gauge = g_world.get<GaugeComponent>(g_gauge)) {
        gauge->setValue(std::sin(g_time * 1.7f) + g_input.axes[0]);
    }

    g_canvas.clear(Pixel4(0));

    // Twinkling star backdrop (primitives path).
    for (const auto& star : kStars) {
        const float tw = std::sin(g_time * 2.3f + star.phase * 0.9f);
        const uint8_t v = static_cast<uint8_t>(4.0f + 3.5f * tw + 4.0f);
        g_canvas.setPixel(star.x, star.y, Pixel4(static_cast<uint8_t>(std::min<int>(v, 12))));
    }

    // Orbit ring + satellite around the logical center pixel (63, 63).
    constexpr int16_t kCx = 63, kCy = 63, kOrbitR = 48;
    Primitives<Pixel4>::drawCircle(g_canvas, kCx, kCy, kOrbitR, Pixel4(2));
    const float a = g_time * 1.1f;
    const int16_t sx = static_cast<int16_t>(kCx + std::lround(std::cos(a) * kOrbitR));
    const int16_t sy = static_cast<int16_t>(kCy + std::lround(std::sin(a) * kOrbitR));
    Primitives<Pixel4>::fillCircle(g_canvas, sx, sy, 2, Pixel4(15));

    // Widget systems (ui ECS path), lowest priority first.
    g_labelSystem.update(dt);
    g_gaugeSystem.update(dt);

    return static_cast<int>(g_frame);
}

val previewGetFramebuffer() {
    constexpr int w = ENJIN2_CANVAS_WIDTH;
    constexpr int h = ENJIN2_CANVAS_HEIGHT;
    // Static buffer + typed_memory_view: a live view into WASM linear memory,
    // no per-frame allocation (same pattern as getCanvasData).
    static uint8_t data[w * h];
    for (int y = 0; y < h; ++y) {
        for (int x = 0; x < w; ++x) {
            data[y * w + x] = g_canvas.getPixel(x, y).value;
        }
    }
    return val(typed_memory_view(static_cast<size_t>(w) * h, data));
}

void previewInjectInput(int buttons, float ax0, float ay0) {
    input_advance_frame(&g_input);
    g_input.buttons = static_cast<uint16_t>(buttons & 0xFFFF);
    g_input.axes[0] = std::clamp(ax0, -1.0f, 1.0f);
    g_input.axes[1] = std::clamp(ay0, -1.0f, 1.0f);
}

// -- Scene-file surface (M2, unwn #184) --

ScenePlayer g_scenePlayer;

bool sceneLoad(std::string jsonText) { return g_scenePlayer.loadText(jsonText); }

bool sceneActive() { return g_scenePlayer.active(); }

void sceneDispatch(std::string event, std::string payloadJson) {
    g_scenePlayer.dispatch(event, payloadJson);
}

void sceneTick() { g_scenePlayer.stepFrame(); }

val sceneGetFramebuffer() {
    // Live view straight over the packed canvas buffer — the same bytes
    // writeGoldenRaw() captures natively. The caller copies before storing.
    ScenePlayer::Canvas* cv = g_scenePlayer.canvas();
    return val(typed_memory_view(cv->getBufferSize(),
                                 reinterpret_cast<uint8_t*>(cv->getBuffer())));
}

int sceneCanvasWidth() { return ScenePlayer::Canvas::kWidth; }
int sceneCanvasHeight() { return ScenePlayer::Canvas::kHeight; }

val sceneGetFramebufferUnpacked() {
    // Display accessor (unwn #185): one byte per pixel, row-major — the same
    // shape as the M0 getFramebuffer(), so the editor's blit path never learns
    // the Canvas4 nibble layout. The packed getSceneFramebuffer() stays the
    // parity-harness surface.
    constexpr int w = ScenePlayer::Canvas::kWidth;
    constexpr int h = ScenePlayer::Canvas::kHeight;
    static uint8_t data[w * h];
    ScenePlayer::Canvas* cv = g_scenePlayer.canvas();
    for (int y = 0; y < h; ++y) {
        for (int x = 0; x < w; ++x) {
            data[y * w + x] = cv->getPixel(x, y).value;
        }
    }
    return val(typed_memory_view(static_cast<size_t>(w) * h, data));
}

} // namespace

EMSCRIPTEN_BINDINGS(enjin2_editor_preview) {
    function("init", &previewInit);
    function("tick", &previewTick);
    function("getFramebuffer", &previewGetFramebuffer);
    function("injectInput", &previewInjectInput);

    function("loadScene", &sceneLoad);
    function("sceneActive", &sceneActive);
    function("sceneDispatch", &sceneDispatch);
    function("sceneTick", &sceneTick);
    function("getSceneFramebuffer", &sceneGetFramebuffer);
    function("getSceneFramebufferUnpacked", &sceneGetFramebufferUnpacked);
    function("getSceneCanvasWidth", &sceneCanvasWidth);
    function("getSceneCanvasHeight", &sceneCanvasHeight);
}
