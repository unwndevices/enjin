/**
 * @file main.cpp
 * @brief ESP-IDF main entry point for Enjin2 Lua integration
 */

#include <stdio.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_system.h"
#include "esp_log.h"

// Forward declaration from esp32_lua_integration.cpp
extern "C" void app_main();

// ESP-IDF requires app_main to be defined
// The actual implementation is in esp32_lua_integration.cpp