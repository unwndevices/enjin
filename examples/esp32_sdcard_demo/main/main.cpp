/**
 * @file main.cpp
 * @brief ESP32 SD Card Lua demo — loads and runs a Lua script from microSD
 *
 * Board: Freenove ESP32-S3-WROOM FNK0104 (N16R8)
 * SD card wiring: 1-bit SDMMC (CLK=38, CMD=40, D0=39)
 */

#include <stdio.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_system.h"
#include "esp_log.h"
#include "esp_timer.h"
#include "nvs_flash.h"

#include "esp_vfs_fat.h"
#include "sdmmc_cmd.h"
#include "driver/sdmmc_host.h"

#include "enjin2/scripting/lua_engine.hpp"

static const char* TAG = "sdcard_demo";

static const char* MOUNT_POINT = "/sdcard";
static const char* SCRIPT_PATH = "/sdcard/demo.lua";

static const int SDMMC_CLK_GPIO = 38;
static const int SDMMC_CMD_GPIO = 40;
static const int SDMMC_D0_GPIO  = 39;

static const int TOTAL_FRAMES    = 300;
static const int FPS_LOG_INTERVAL = 60;

extern "C" void app_main() {
    ESP_LOGI(TAG, "Enjin2 ESP32 SD Card demo starting");

    // --- NVS init ---
    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_LOGW(TAG, "NVS needs erase — erasing and reinitializing");
        nvs_flash_erase();
        nvs_flash_init();
    }

    // --- Mount SD card (1-bit SDMMC) ---
    sdmmc_card_t* card = nullptr;

    esp_vfs_fat_sdmmc_mount_config_t mount_config = {};
    mount_config.format_if_mount_failed = false;
    mount_config.max_files = 5;
    mount_config.allocation_unit_size = 16 * 1024;

    sdmmc_host_t host = SDMMC_HOST_DEFAULT();
    host.max_freq_khz = SDMMC_FREQ_DEFAULT;

    sdmmc_slot_config_t slot_config = SDMMC_SLOT_CONFIG_DEFAULT();
    slot_config.width = 1;  // 1-bit mode
    slot_config.clk = static_cast<gpio_num_t>(SDMMC_CLK_GPIO);
    slot_config.cmd = static_cast<gpio_num_t>(SDMMC_CMD_GPIO);
    slot_config.d0  = static_cast<gpio_num_t>(SDMMC_D0_GPIO);
    slot_config.flags |= SDMMC_SLOT_FLAG_INTERNAL_PULLUP;

    ESP_LOGI(TAG, "Mounting SD card at %s (CLK=%d, CMD=%d, D0=%d)",
             MOUNT_POINT, SDMMC_CLK_GPIO, SDMMC_CMD_GPIO, SDMMC_D0_GPIO);

    ret = esp_vfs_fat_sdmmc_mount(MOUNT_POINT, &host, &slot_config, &mount_config, &card);
    if (ret != ESP_OK) {
        if (ret == ESP_FAIL) {
            ESP_LOGE(TAG, "Failed to mount FAT filesystem — is the SD card formatted as FAT32?");
        } else {
            ESP_LOGE(TAG, "SD card mount failed: %s", esp_err_to_name(ret));
        }
        return;
    }

    ESP_LOGI(TAG, "SD card mounted successfully");
    sdmmc_card_print_info(stdout, card);

    // --- Initialize Lua engine ---
    enjin2::LuaEngine engine;
    if (!engine.initialize()) {
        ESP_LOGE(TAG, "Failed to initialize Lua engine");
        esp_vfs_fat_sdcard_unmount(MOUNT_POINT, card);
        return;
    }
    ESP_LOGI(TAG, "Lua engine initialized");

    // --- Load script from SD card ---
    auto result = engine.executeFile(SCRIPT_PATH);
    if (!result.success) {
        ESP_LOGE(TAG, "Failed to load %s: %s", SCRIPT_PATH, result.error.c_str());
        engine.shutdown();
        esp_vfs_fat_sdcard_unmount(MOUNT_POINT, card);
        return;
    }
    ESP_LOGI(TAG, "demo.lua loaded from SD card");

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

    ret = esp_vfs_fat_sdcard_unmount(MOUNT_POINT, card);
    if (ret == ESP_OK) {
        ESP_LOGI(TAG, "SD card unmounted");
    } else {
        ESP_LOGW(TAG, "SD card unmount: %s", esp_err_to_name(ret));
    }

    ESP_LOGI(TAG, "Done. Free heap: %lu bytes", (unsigned long)esp_get_free_heap_size());
}
