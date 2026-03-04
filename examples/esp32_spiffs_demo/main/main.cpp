/**
 * @file main.cpp
 * @brief ESP32 SPIFFS Lua demo — runs a Lua script from internal flash
 *
 * Board: Freenove ESP32-S3-WROOM FNK0104 (N16R8)
 * The Lua script is embedded in a SPIFFS partition and flashed with the firmware.
 */

#include <stdio.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_system.h"
#include "esp_log.h"
#include "esp_timer.h"
#include "nvs_flash.h"
#include "esp_spiffs.h"

#include "enjin2/scripting/lua_engine.hpp"

static const char* TAG = "spiffs_demo";

static const char* MOUNT_POINT = "/spiffs";
static const char* SCRIPT_PATH = "/spiffs/demo.lua";

static const int TOTAL_FRAMES     = 300;
static const int FPS_LOG_INTERVAL = 60;

extern "C" void app_main() {
    ESP_LOGI(TAG, "Enjin2 ESP32 SPIFFS demo starting");

    // --- NVS init ---
    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_LOGW(TAG, "NVS needs erase — erasing and reinitializing");
        nvs_flash_erase();
        nvs_flash_init();
    }

    // --- Mount SPIFFS ---
    esp_vfs_spiffs_conf_t spiffs_conf = {};
    spiffs_conf.base_path = MOUNT_POINT;
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

    // --- Initialize Lua engine ---
    enjin2::LuaEngine engine;
    if (!engine.initialize()) {
        ESP_LOGE(TAG, "Failed to initialize Lua engine");
        esp_vfs_spiffs_unregister("storage");
        return;
    }
    ESP_LOGI(TAG, "Lua engine initialized");

    // --- Load script from SPIFFS ---
    auto result = engine.executeFile(SCRIPT_PATH);
    if (!result.success) {
        ESP_LOGE(TAG, "Failed to load %s: %s", SCRIPT_PATH, result.error.c_str());
        engine.shutdown();
        esp_vfs_spiffs_unregister("storage");
        return;
    }
    ESP_LOGI(TAG, "demo.lua loaded from SPIFFS");

    // --- Game loop: 300 frames with real dt measurement ---
    int64_t last_time = esp_timer_get_time();
    int64_t fps_accumulator = 0;
    int fps_frame_count = 0;

    ESP_LOGI(TAG, "Running %d frames...", TOTAL_FRAMES);

    for (int frame = 0; frame < TOTAL_FRAMES; frame++) {
        int64_t now = esp_timer_get_time();
        int64_t elapsed_us = now - last_time;
        last_time = now;

        float dt = static_cast<float>(elapsed_us) / 1000000.0f;

        // Clamp dt on first frame (elapsed is near-zero)
        if (dt <= 0.0f || dt > 0.1f) {
            dt = 1.0f / 60.0f;
        }

        engine.callFunction("update", dt);

        // FPS tracking
        fps_accumulator += elapsed_us;
        fps_frame_count++;
        if (fps_frame_count >= FPS_LOG_INTERVAL) {
            float avg_fps = 1000000.0f * fps_frame_count / static_cast<float>(fps_accumulator);
            ESP_LOGI(TAG, "FPS: %.1f (avg over %d frames)", avg_fps, fps_frame_count);
            fps_accumulator = 0;
            fps_frame_count = 0;
        }

        // Target ~60fps
        vTaskDelay(pdMS_TO_TICKS(16));
    }

    // --- Clean shutdown ---
    ESP_LOGI(TAG, "Demo complete — shutting down");
    engine.shutdown();
    esp_vfs_spiffs_unregister("storage");
    ESP_LOGI(TAG, "SPIFFS unmounted");
    ESP_LOGI(TAG, "Done. Free heap: %lu bytes", (unsigned long)esp_get_free_heap_size());
}
