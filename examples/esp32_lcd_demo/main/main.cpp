/**
 * @file main.cpp
 * @brief ESP32 ILI9341 LCD demo — graphical Lua rendering on a 320x240 display
 *
 * Board: Freenove ESP32-S3-WROOM FNK0104 (N16R8)
 * Display: ILI9341 320x240 SPI LCD
 *
 * Rendering pipeline (per frame):
 *   Lua update(dt) → LayerCompositor::composite() → strip expand+DMA → LCD
 *
 * Uses strip-based DMA pipelining with internal DRAM buffers (like LVGL):
 *   - Two small strip buffers in DMA-capable internal SRAM
 *   - Expand strip N to DRAM, DMA strip N to LCD, overlap with strip N+1
 *   - Avoids PSRAM DMA (which falls back to CPU-polling SPI transfers)
 */

#include <cstdio>
#include <cstring>
#include <new>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/semphr.h"
#include "esp_system.h"
#include "esp_log.h"
#include "esp_timer.h"
#include "esp_heap_caps.h"
#include "nvs_flash.h"
#include "esp_spiffs.h"
#include "driver/spi_master.h"
#include "driver/gpio.h"
#include "esp_lcd_panel_io.h"
#include "esp_lcd_panel_vendor.h"
#include "esp_lcd_panel_ops.h"
#include "esp_lcd_ili9341.h"

#include "enjin2/scripting/bindings.hpp"
#include "enjin2/graphics/layer_compositor.hpp"
#include "enjin2/graphics/palette.hpp"

static const char* TAG = "lcd_demo";

// --- Pin definitions (Freenove ESP32-S3-WROOM FNK0104 + ILI9341) ---
static constexpr gpio_num_t PIN_LCD_CS   = GPIO_NUM_10;
static constexpr gpio_num_t PIN_LCD_MOSI = GPIO_NUM_11;
static constexpr gpio_num_t PIN_LCD_SCK  = GPIO_NUM_12;
static constexpr gpio_num_t PIN_LCD_MISO = GPIO_NUM_13;
static constexpr gpio_num_t PIN_LCD_BL   = GPIO_NUM_45;
static constexpr gpio_num_t PIN_LCD_DC   = GPIO_NUM_46;
static constexpr int        PIN_LCD_RST  = -1;  // No reset pin

// --- Display dimensions ---
static constexpr uint16_t LCD_W = 320;
static constexpr uint16_t LCD_H = 240;

// --- Strip-based DMA: 20-line strips, 12 strips per frame ---
static constexpr int STRIP_H = 20;                              // Lines per strip
static constexpr int STRIP_COUNT = LCD_H / STRIP_H;             // 12 strips
static constexpr size_t STRIP_BYTES = LCD_W * STRIP_H * 2;      // 12,800 bytes per strip
static_assert(LCD_H % STRIP_H == 0, "LCD_H must be divisible by STRIP_H");

// --- Frame control ---
static constexpr int TOTAL_FRAMES     = 300;
static constexpr int FPS_LOG_INTERVAL = 60;

// --- DMA completion semaphore ---
static SemaphoreHandle_t s_dma_done_sem = nullptr;

static bool IRAM_ATTR on_color_trans_done(esp_lcd_panel_io_handle_t panel_io,
                                          esp_lcd_panel_io_event_data_t* edata,
                                          void* user_ctx) {
    BaseType_t high_task_woken = pdFALSE;
    xSemaphoreGiveFromISR(s_dma_done_sem, &high_task_woken);
    return high_task_woken == pdTRUE;
}

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
 * @brief Pre-computed palette → RGB565 big-endian lookup table (16 entries)
 */
static uint16_t s_rgb565_lut[16];

static void build_rgb565_lut() {
    for (int i = 0; i < 16; i++) {
        if (enjin2::g_palette.isTransparent(i)) {
            s_rgb565_lut[i] = 0;
        } else {
            enjin2::RGB c = enjin2::g_palette.resolve(i);
            uint16_t rgb565 = ((c.r & 0xF8) << 8) | ((c.g & 0xFC) << 3) | (c.b >> 3);
            s_rgb565_lut[i] = __builtin_bswap16(rgb565);
        }
    }
}

/**
 * @brief Expand one horizontal strip of Canvas4 → RGB565 into an internal DRAM buffer
 *
 * Reads from PSRAM (compositor output), writes to internal DRAM (strip buffer).
 * Uses pre-computed LUT for zero-branch conversion.
 */
static void IRAM_ATTR expand_strip(const enjin2::Canvas4<LCD_W, LCD_H>& canvas,
                                   int y_start, uint16_t* out) {
    const enjin2::PackedPixel4* buf = canvas.getBuffer();
    static constexpr int ROW_BYTES = LCD_W / 2;  // 160 packed bytes per row

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
 * Splits the 320×240 frame into 12 strips of 20 lines each.
 * Uses two internal DRAM buffers: while DMA sends strip N, the CPU
 * expands strip N+1 into the other buffer (true overlap).
 */
static void flush_frame_strips(esp_lcd_panel_handle_t panel,
                                const enjin2::Canvas4<LCD_W, LCD_H>& canvas,
                                uint16_t* strip_buf[2]) {
    int cur = 0;
    for (int s = 0; s < STRIP_COUNT; s++) {
        int y = s * STRIP_H;

        // Expand this strip: PSRAM canvas → internal DRAM buffer
        expand_strip(canvas, y, strip_buf[cur]);

        // Wait for previous strip's DMA to finish
        xSemaphoreTake(s_dma_done_sem, portMAX_DELAY);

        // Start DMA of this strip from internal DRAM → SPI → LCD
        esp_lcd_panel_draw_bitmap(panel, 0, y, LCD_W, y + STRIP_H, strip_buf[cur]);

        cur ^= 1;
    }
}

extern "C" void app_main() {
    ESP_LOGI(TAG, "Enjin2 ESP32 LCD demo starting");
    ESP_LOGI(TAG, "Free heap: %lu bytes, PSRAM: %lu bytes",
             (unsigned long)esp_get_free_heap_size(),
             (unsigned long)heap_caps_get_free_size(MALLOC_CAP_SPIRAM));

    // --- NVS init ---
    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_LOGW(TAG, "NVS needs erase — erasing and reinitializing");
        nvs_flash_erase();
        nvs_flash_init();
    }

    // --- Mount SPIFFS ---
    esp_vfs_spiffs_conf_t spiffs_conf = {};
    spiffs_conf.base_path = "/spiffs";
    spiffs_conf.partition_label = "storage";
    spiffs_conf.max_files = 5;
    spiffs_conf.format_if_mount_failed = false;

    ret = esp_vfs_spiffs_register(&spiffs_conf);
    if (ret != ESP_OK) {
        if (ret == ESP_ERR_NOT_FOUND) {
            ESP_LOGE(TAG, "SPIFFS partition not found — check partitions.csv");
        } else if (ret == ESP_FAIL) {
            ESP_LOGE(TAG, "Failed to mount SPIFFS — partition may be corrupted");
        } else {
            ESP_LOGE(TAG, "SPIFFS mount failed: %s", esp_err_to_name(ret));
        }
        return;
    }

    size_t total = 0, used = 0;
    esp_spiffs_info("storage", &total, &used);
    ESP_LOGI(TAG, "SPIFFS mounted — total: %d bytes, used: %d bytes", total, used);

    // =========================================================================
    // LCD initialization
    // =========================================================================

    // Backlight: GPIO 45 high = on
    gpio_config_t bl_cfg = {};
    bl_cfg.pin_bit_mask = 1ULL << PIN_LCD_BL;
    bl_cfg.mode = GPIO_MODE_OUTPUT;
    gpio_config(&bl_cfg);
    gpio_set_level(PIN_LCD_BL, 1);
    ESP_LOGI(TAG, "Backlight ON (GPIO %d)", PIN_LCD_BL);

    // SPI bus
    spi_bus_config_t buscfg = {};
    buscfg.mosi_io_num = PIN_LCD_MOSI;
    buscfg.miso_io_num = PIN_LCD_MISO;
    buscfg.sclk_io_num = PIN_LCD_SCK;
    buscfg.quadwp_io_num = -1;
    buscfg.quadhd_io_num = -1;
    buscfg.max_transfer_sz = STRIP_BYTES;  // Only need to fit one strip

    ret = spi_bus_initialize(SPI2_HOST, &buscfg, SPI_DMA_CH_AUTO);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "SPI bus init failed: %s", esp_err_to_name(ret));
        return;
    }
    ESP_LOGI(TAG, "SPI bus initialized");

    // LCD panel IO (SPI)
    esp_lcd_panel_io_handle_t io_handle = nullptr;
    esp_lcd_panel_io_spi_config_t io_config = {};
    io_config.cs_gpio_num = PIN_LCD_CS;
    io_config.dc_gpio_num = PIN_LCD_DC;
    io_config.spi_mode = 0;
    io_config.pclk_hz = 40 * 1000 * 1000;  // 40MHz SPI clock
    io_config.trans_queue_depth = 10;
    io_config.on_color_trans_done = on_color_trans_done;
    io_config.lcd_cmd_bits = 8;
    io_config.lcd_param_bits = 8;

    ret = esp_lcd_new_panel_io_spi(SPI2_HOST, &io_config, &io_handle);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "LCD panel IO init failed: %s", esp_err_to_name(ret));
        return;
    }
    ESP_LOGI(TAG, "LCD panel IO created (40MHz, CS=%d, DC=%d)", PIN_LCD_CS, PIN_LCD_DC);

    // LCD panel (ILI9341)
    esp_lcd_panel_handle_t panel = nullptr;
    esp_lcd_panel_dev_config_t panel_config = {};
    panel_config.reset_gpio_num = PIN_LCD_RST;
    panel_config.rgb_ele_order = LCD_RGB_ELEMENT_ORDER_BGR;
    panel_config.bits_per_pixel = 16;

    ret = esp_lcd_new_panel_ili9341(io_handle, &panel_config, &panel);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "ILI9341 panel init failed: %s", esp_err_to_name(ret));
        return;
    }

    esp_lcd_panel_reset(panel);
    esp_lcd_panel_init(panel);
    esp_lcd_panel_swap_xy(panel, true);
    esp_lcd_panel_mirror(panel, false, true);
    esp_lcd_panel_disp_on_off(panel, true);
    ESP_LOGI(TAG, "ILI9341 initialized: %dx%d landscape", LCD_W, LCD_H);

    // =========================================================================
    // Strip DMA buffers — internal DRAM (true DMA-capable, not PSRAM)
    // =========================================================================
    s_dma_done_sem = xSemaphoreCreateBinary();
    xSemaphoreGive(s_dma_done_sem);  // Start with "done" so first strip doesn't block

    uint16_t* strip_buf[2];
    strip_buf[0] = static_cast<uint16_t*>(
        heap_caps_malloc(STRIP_BYTES, MALLOC_CAP_DMA | MALLOC_CAP_INTERNAL));
    strip_buf[1] = static_cast<uint16_t*>(
        heap_caps_malloc(STRIP_BYTES, MALLOC_CAP_DMA | MALLOC_CAP_INTERNAL));

    if (!strip_buf[0] || !strip_buf[1]) {
        ESP_LOGE(TAG, "Failed to allocate strip DMA buffers (%d bytes each)", STRIP_BYTES);
        return;
    }
    ESP_LOGI(TAG, "Strip DMA buffers allocated (%d bytes each in internal DRAM, %d lines/strip)",
             STRIP_BYTES, STRIP_H);

    // =========================================================================
    // Graphics pipeline setup (large objects in PSRAM)
    // =========================================================================

    // Compositor: 4 layers × 38KB + output = ~192KB → PSRAM
    g_comp = psram_new<Compositor>();
    if (!g_comp) {
        ESP_LOGE(TAG, "Failed to allocate compositor in PSRAM");
        return;
    }
    ESP_LOGI(TAG, "LayerCompositor allocated in PSRAM (%d bytes)", sizeof(Compositor));

    // LuaCanvas wrappers (tiny, ok on stack)
    enjin2::LuaCanvas lua_layers[4] = {
        enjin2::LuaCanvas(&g_comp->layers[0]),
        enjin2::LuaCanvas(&g_comp->layers[1]),
        enjin2::LuaCanvas(&g_comp->layers[2]),
        enjin2::LuaCanvas(&g_comp->layers[3]),
    };
    enjin2::LuaCanvas* layer_ptrs[4] = {
        &lua_layers[0], &lua_layers[1], &lua_layers[2], &lua_layers[3],
    };

    // LuaScriptSystem: ~65KB (assetBuffer) → PSRAM
    auto* lua = psram_new<enjin2::LuaScriptSystem>();
    if (!lua) {
        ESP_LOGE(TAG, "Failed to allocate LuaScriptSystem in PSRAM");
        psram_delete(g_comp);
        return;
    }

    if (!lua->initialize()) {
        ESP_LOGE(TAG, "Failed to initialize Lua script system");
        psram_delete(lua);
        psram_delete(g_comp);
        esp_vfs_spiffs_unregister("storage");
        return;
    }
    ESP_LOGI(TAG, "Lua script system initialized (in PSRAM)");

    // Wire layer canvases into the Lua bindings
    lua->getBindings().setLayers(layer_ptrs, 4, g_comp->visible);
    lua->getBindings().setCanvas(layer_ptrs[0]);

    // Load the demo script
    auto result = lua->loadScript("/spiffs/demo.lua");
    if (!result.success) {
        ESP_LOGE(TAG, "Failed to load demo.lua: %s", result.error.c_str());
        lua->shutdown();
        psram_delete(lua);
        psram_delete(g_comp);
        esp_vfs_spiffs_unregister("storage");
        return;
    }
    ESP_LOGI(TAG, "demo.lua loaded from SPIFFS");

    // Build palette→RGB565 LUT (avoids per-pixel function calls in hot loop)
    build_rgb565_lut();

    // =========================================================================
    // Frame loop
    // =========================================================================
    int64_t last_time = esp_timer_get_time();
    int64_t fps_accumulator = 0;
    int fps_frame_count = 0;

    ESP_LOGI(TAG, "Running %d frames...", TOTAL_FRAMES);

    for (int frame = 0; frame < TOTAL_FRAMES; frame++) {
        int64_t now = esp_timer_get_time();
        int64_t elapsed_us = now - last_time;
        last_time = now;

        float dt = static_cast<float>(elapsed_us) / 1000000.0f;
        if (dt <= 0.0f || dt > 0.1f) {
            dt = 1.0f / 60.0f;
        }

        // 1. Clear layers for new frame
        g_comp->clearAll();

        // 2. Lua update — draws to layer canvases via bindings
        lua->callFunction("update", dt);

        // 3. Composite all layers → output
        g_comp->composite();

        // 4. Strip-pipelined DMA: expand + send 12 strips with overlap
        flush_frame_strips(panel, g_comp->output, strip_buf);

        // FPS tracking
        fps_accumulator += elapsed_us;
        fps_frame_count++;
        if (fps_frame_count >= FPS_LOG_INTERVAL) {
            float avg_fps = 1000000.0f * fps_frame_count / static_cast<float>(fps_accumulator);
            ESP_LOGI(TAG, "FPS: %.1f (avg over %d frames)", avg_fps, fps_frame_count);
            fps_accumulator = 0;
            fps_frame_count = 0;
        }

        // Yield to IDLE task so watchdog gets fed
        vTaskDelay(1);
    }

    // =========================================================================
    // Clean shutdown
    // =========================================================================
    ESP_LOGI(TAG, "Demo complete — shutting down");

    // Wait for last DMA transfer
    xSemaphoreTake(s_dma_done_sem, portMAX_DELAY);

    lua->shutdown();
    psram_delete(lua);
    psram_delete(g_comp);
    heap_caps_free(strip_buf[0]);
    heap_caps_free(strip_buf[1]);
    vSemaphoreDelete(s_dma_done_sem);

    esp_vfs_spiffs_unregister("storage");
    ESP_LOGI(TAG, "SPIFFS unmounted");
    ESP_LOGI(TAG, "Done. Free heap: %lu bytes", (unsigned long)esp_get_free_heap_size());
}
