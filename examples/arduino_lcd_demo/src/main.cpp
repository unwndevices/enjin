/**
 * @file main.cpp
 * @brief Arduino ILI9341 LCD demo — graphical Lua rendering on a 320x240 display
 *
 * Board: Freenove ESP32-S3-WROOM FNK0104 (N16R8)
 * Display: ILI9341 320x240 SPI LCD
 *
 * Rendering pipeline (per frame):
 *   Lua update(dt) -> LayerCompositor::composite() -> strip expand+DMA -> LCD
 *
 * Uses strip-based DMA pipelining with internal DRAM buffers:
 *   - Two small strip buffers in DMA-capable internal SRAM
 *   - Expand strip N to DRAM, DMA strip N to LCD, overlap with strip N+1
 *   - Avoids PSRAM DMA (which falls back to CPU-polling SPI transfers)
 *
 * Arduino framework port using TFT_eSPI for display and SPIFFS for Lua script.
 */

#include <Arduino.h>
#include <TFT_eSPI.h>
#include <SPIFFS.h>
#include <cstring>
#include <new>
#include "esp_heap_caps.h"

#include "enjin2/scripting/bindings.hpp"
#include "enjin2/graphics/layer_compositor.hpp"
#include "enjin2/graphics/palette.hpp"

// --- Display dimensions ---
static constexpr uint16_t LCD_W = 320;
static constexpr uint16_t LCD_H = 240;

// --- Strip-based DMA: 20-line strips, 12 strips per frame ---
static constexpr int STRIP_H = 20;
static constexpr int STRIP_COUNT = LCD_H / STRIP_H;
static constexpr size_t STRIP_BYTES = LCD_W * STRIP_H * 2;
static_assert(LCD_H % STRIP_H == 0, "LCD_H must be divisible by STRIP_H");

// --- Frame control ---
static constexpr int TOTAL_FRAMES     = 300;
static constexpr int FPS_LOG_INTERVAL = 60;

// --- TFT_eSPI display ---
static TFT_eSPI tft = TFT_eSPI();

// --- Large objects live in PSRAM (too big for DRAM) ---
using Compositor = enjin2::LayerCompositor<LCD_W, LCD_H>;
static Compositor* g_comp = nullptr;

template <typename T, typename... Args>
static T* psram_new(Args&&... args) {
    void* mem = heap_caps_malloc(sizeof(T), MALLOC_CAP_SPIRAM);
    if (!mem) return nullptr;
    return new (mem) T(std::forward<Args>(args)...);
}

template <typename T>
static void psram_delete(T* ptr) {
    if (ptr) {
        ptr->~T();
        heap_caps_free(ptr);
    }
}

/**
 * @brief Pre-computed palette -> RGB565 big-endian lookup table (16 entries)
 */
static uint16_t s_rgb565_lut[16];

static void build_rgb565_lut() {
    for (int i = 0; i < 16; i++) {
        if (enjin2::g_palette.isTransparent(i)) {
            s_rgb565_lut[i] = 0;
        } else {
            enjin2::RGB c = enjin2::g_palette.resolve(i);
            uint16_t rgb565 = ((c.r & 0xF8) << 8) | ((c.g & 0xFC) << 3) | (c.b >> 3);
            // TFT_eSPI pushPixelsDMA expects big-endian RGB565
            s_rgb565_lut[i] = __builtin_bswap16(rgb565);
        }
    }
}

/**
 * @brief Expand one horizontal strip of Canvas4 -> RGB565 into an internal DRAM buffer
 */
static void IRAM_ATTR expand_strip(const enjin2::Canvas4<LCD_W, LCD_H>& canvas,
                                   int y_start, uint16_t* out) {
    const enjin2::PackedPixel4* buf = canvas.getBuffer();
    static constexpr int ROW_BYTES = LCD_W / 2;

    for (int row = 0; row < STRIP_H; row++) {
        const enjin2::PackedPixel4* src = buf + (y_start + row) * ROW_BYTES;
        uint16_t* dst = out + row * LCD_W;

        for (int i = 0; i < ROW_BYTES; i++) {
            uint8_t packed = src[i].getByte();
            dst[i * 2]     = s_rgb565_lut[packed & 0x0F];
            dst[i * 2 + 1] = s_rgb565_lut[(packed >> 4) & 0x0F];
        }
    }
}

/**
 * @brief Flush the composited frame to LCD using strip-pipelined DMA
 *
 * Uses TFT_eSPI's DMA support. Two internal DRAM buffers alternate:
 * expand strip N+1 while DMA sends strip N.
 */
static void flush_frame_strips(const enjin2::Canvas4<LCD_W, LCD_H>& canvas,
                                uint16_t* strip_buf[2]) {
    int cur = 0;
    tft.startWrite();
    for (int s = 0; s < STRIP_COUNT; s++) {
        int y = s * STRIP_H;

        // Expand this strip: PSRAM canvas -> internal DRAM buffer
        expand_strip(canvas, y, strip_buf[cur]);

        // Wait for previous DMA to finish before reusing the buffer
        tft.dmaWait();

        // Set address window and push pixels via DMA
        tft.setAddrWindow(0, y, LCD_W, STRIP_H);
        tft.pushPixelsDMA(strip_buf[cur], LCD_W * STRIP_H);

        cur ^= 1;
    }
    // Wait for final strip DMA to complete
    tft.dmaWait();
    tft.endWrite();
}

// --- Enjin2 objects (allocated in setup, must outlive setup() scope) ---
static enjin2::LuaScriptSystem* g_lua = nullptr;
static enjin2::LuaCanvas* g_lua_layers = nullptr;  // Array of 4, PSRAM-allocated
static uint16_t* strip_buf[2] = {nullptr, nullptr};

// --- Frame timing ---
static unsigned long last_time_us = 0;
static unsigned long fps_accumulator = 0;
static int fps_frame_count = 0;
static int frame_number = 0;

void setup() {
    Serial.begin(115200);
    delay(100);
    Serial.println("Enjin2 Arduino LCD demo starting");
    Serial.printf("Free heap: %lu bytes, PSRAM: %lu bytes\n",
                  (unsigned long)ESP.getFreeHeap(),
                  (unsigned long)ESP.getFreePsram());

    // --- Mount SPIFFS ---
    if (!SPIFFS.begin(true, "/spiffs")) {
        Serial.println("ERROR: Failed to mount SPIFFS");
        return;
    }
    Serial.println("SPIFFS mounted");

    // --- TFT init ---
    tft.init();
    tft.setRotation(1);  // Landscape 320x240
    tft.fillScreen(TFT_BLACK);
    tft.initDMA();
    // Backlight is handled by TFT_eSPI via TFT_BL pin define
    Serial.printf("ILI9341 initialized: %dx%d landscape\n", LCD_W, LCD_H);

    // --- Strip DMA buffers (internal DRAM, DMA-capable) ---
    strip_buf[0] = static_cast<uint16_t*>(
        heap_caps_malloc(STRIP_BYTES, MALLOC_CAP_DMA | MALLOC_CAP_INTERNAL));
    strip_buf[1] = static_cast<uint16_t*>(
        heap_caps_malloc(STRIP_BYTES, MALLOC_CAP_DMA | MALLOC_CAP_INTERNAL));

    if (!strip_buf[0] || !strip_buf[1]) {
        Serial.printf("ERROR: Failed to allocate strip DMA buffers (%d bytes each)\n", STRIP_BYTES);
        return;
    }
    Serial.printf("Strip DMA buffers allocated (%d bytes each, %d lines/strip)\n",
                  STRIP_BYTES, STRIP_H);

    // --- Compositor (PSRAM) ---
    g_comp = psram_new<Compositor>();
    if (!g_comp) {
        Serial.println("ERROR: Failed to allocate compositor in PSRAM");
        return;
    }
    Serial.printf("LayerCompositor allocated in PSRAM (%d bytes)\n", sizeof(Compositor));

    // LuaCanvas wrappers — must outlive setup(), allocated in PSRAM
    void* layers_mem = heap_caps_malloc(sizeof(enjin2::LuaCanvas) * 4, MALLOC_CAP_SPIRAM);
    if (!layers_mem) {
        Serial.println("ERROR: Failed to allocate LuaCanvas array");
        psram_delete(g_comp);
        return;
    }
    g_lua_layers = static_cast<enjin2::LuaCanvas*>(layers_mem);
    for (int i = 0; i < 4; i++) {
        new (&g_lua_layers[i]) enjin2::LuaCanvas(&g_comp->layers[i]);
    }
    enjin2::LuaCanvas* layer_ptrs[4] = {
        &g_lua_layers[0], &g_lua_layers[1], &g_lua_layers[2], &g_lua_layers[3],
    };

    // --- Lua script system (PSRAM) ---
    g_lua = psram_new<enjin2::LuaScriptSystem>();
    if (!g_lua) {
        Serial.println("ERROR: Failed to allocate LuaScriptSystem in PSRAM");
        psram_delete(g_comp);
        return;
    }

    if (!g_lua->initialize()) {
        Serial.println("ERROR: Failed to initialize Lua script system");
        psram_delete(g_lua);
        psram_delete(g_comp);
        return;
    }
    Serial.println("Lua script system initialized (in PSRAM)");

    // Wire layer canvases into the Lua bindings
    g_lua->getBindings().setLayers(layer_ptrs, 4, g_comp->visible);
    g_lua->getBindings().setCanvas(layer_ptrs[0]);

    // Load the demo script
    auto result = g_lua->loadScript("/spiffs/demo.lua");
    if (!result.success) {
        Serial.printf("ERROR: Failed to load demo.lua: %s\n", result.error.c_str());
        g_lua->shutdown();
        psram_delete(g_lua);
        psram_delete(g_comp);
        return;
    }
    Serial.println("demo.lua loaded from SPIFFS");

    // Build palette -> RGB565 LUT
    build_rgb565_lut();

    last_time_us = micros();
    Serial.printf("Running %d frames...\n", TOTAL_FRAMES);
}

void loop() {
    if (frame_number >= TOTAL_FRAMES) {
        // Demo complete — clean shutdown (once)
        if (frame_number == TOTAL_FRAMES) {
            Serial.println("Demo complete — shutting down");
            tft.dmaWait();

            if (g_lua) {
                g_lua->shutdown();
                psram_delete(g_lua);
                g_lua = nullptr;
            }
            if (g_lua_layers) {
                for (int i = 0; i < 4; i++) g_lua_layers[i].~LuaCanvas();
                heap_caps_free(g_lua_layers);
                g_lua_layers = nullptr;
            }
            psram_delete(g_comp);
            g_comp = nullptr;
            heap_caps_free(strip_buf[0]);
            heap_caps_free(strip_buf[1]);
            strip_buf[0] = strip_buf[1] = nullptr;

            SPIFFS.end();
            Serial.println("SPIFFS unmounted");
            Serial.printf("Done. Free heap: %lu bytes\n", (unsigned long)ESP.getFreeHeap());
            frame_number++;  // Prevent re-entry
        }
        delay(1000);
        return;
    }

    if (!g_comp || !g_lua) {
        delay(1000);
        return;
    }

    unsigned long now = micros();
    unsigned long elapsed_us = now - last_time_us;
    last_time_us = now;

    float dt = static_cast<float>(elapsed_us) / 1000000.0f;
    if (dt <= 0.0f || dt > 0.1f) {
        dt = 1.0f / 60.0f;
    }

    // 1. Clear layers for new frame
    g_comp->clearAll();

    // 2. Lua update — draws to layer canvases via bindings
    g_lua->callFunction("update", dt);

    // 3. Composite all layers -> output
    g_comp->composite();

    // 4. Strip-pipelined DMA: expand + send 12 strips with overlap
    flush_frame_strips(g_comp->output, strip_buf);

    // FPS tracking
    fps_accumulator += elapsed_us;
    fps_frame_count++;
    if (fps_frame_count >= FPS_LOG_INTERVAL) {
        float avg_fps = 1000000.0f * fps_frame_count / static_cast<float>(fps_accumulator);
        Serial.printf("FPS: %.1f (avg over %d frames)\n", avg_fps, fps_frame_count);
        fps_accumulator = 0;
        fps_frame_count = 0;
    }

    frame_number++;
    yield();
}
