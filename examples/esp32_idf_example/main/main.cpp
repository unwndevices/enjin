/**
 * @file main.cpp
 * @brief ESP-IDF main entry point for Enjin2 Lua integration
 */

#include <stdio.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_system.h"
#include "esp_log.h"

#include "enjin2/scripting/lua_engine.hpp"

static const char* TAG = "enjin2";

extern "C" void app_main() {
    ESP_LOGI(TAG, "Enjin2 ESP32 Lua example starting");

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
