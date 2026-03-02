/**
 * @file main.cpp
 * @brief ESP-IDF main entry point for Enjin2 Lua integration
 */

#include <stdio.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_system.h"
#include "esp_log.h"
#include "nvs_flash.h"

#include "enjin2/scripting/lua_engine.hpp"

static const char* TAG = "enjin2";

extern "C" void app_main() {
    ESP_LOGI(TAG, "Enjin2 ESP32 Lua example starting");

    // Initialize NVS — required before any NVS handle can be opened.
    esp_err_t nvs_ret = nvs_flash_init();
    if (nvs_ret == ESP_ERR_NVS_NO_FREE_PAGES || nvs_ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_LOGW(TAG, "NVS partition needs erase — erasing and reinitializing");
        nvs_flash_erase();
        nvs_flash_init();
    }

    enjin2::LuaEngine engine;
    if (!engine.initialize()) {
        ESP_LOGE(TAG, "Failed to initialize Lua engine");
        return;
    }

    const char* script = "print('Hello from Enjin2 Lua on ESP32!')";
    engine.executeString(script);

    ESP_LOGI(TAG, "Script executed. Idling.");
    while (true) {
        vTaskDelay(pdMS_TO_TICKS(1000));
    }
}
