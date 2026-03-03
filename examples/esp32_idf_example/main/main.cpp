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
#include "enjin2/input/input_state.hpp"

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

    // Load the script once — init() will be called on first update().
    const char* script =
        "function init(self) print('ESP32: init') end\n"
        "function update(self, dt) end\n";
    engine.executeString(script);

    // Input state — zero-alloc value type on the stack.
    // In a real project, replace input_platform_poll with your GPIO/button polling code.
    static enjin2::InputState g_input{};

    // Per-frame game loop: target ~60fps via pdMS_TO_TICKS(16).
    // Replace the tick interval with your desired frame rate.
    const TickType_t FRAME_TICKS = pdMS_TO_TICKS(16);  // ~62.5 fps
    TickType_t last_wake = xTaskGetTickCount();

    ESP_LOGI(TAG, "Entering game loop");
    while (true) {
        // 1. Advance input state (copies current->prev, zeros current).
        enjin2::input_advance_frame(&g_input);

        // 2. Poll hardware input — stub: returns all-zero (no buttons pressed).
        //    In a real project, read GPIO pins here and set g_input.buttons / g_input.axes.
        enjin2::input_platform_poll(&g_input);

        // 3. Wire input into Lua bindings so scripts can read engine.input.
        //    Must happen AFTER poll and BEFORE any Lua update call.
        //    Requires migrating this example from LuaEngine to LuaScriptSystem:
        // scriptSystem.getBindings().setInput(&g_input);

        // 4. Tick the engine — invoke the Lua-side update() function directly.
        //    LuaEngine has no update() method; use callFunction() to invoke scripts.
        //    For full per-frame tick (coroutines, tweens, camera follow) migrate
        //    to LuaScriptSystem and call getBindings().tickCoroutines(dt), etc.
        const float dt = 0.016f;
        engine.callFunction("update", dt);

        // 5. Sleep until next frame period (vTaskDelayUntil is jitter-free).
        vTaskDelayUntil(&last_wake, FRAME_TICKS);
    }
}
