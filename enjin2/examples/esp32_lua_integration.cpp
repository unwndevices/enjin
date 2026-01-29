/**
 * @file esp32_lua_integration.cpp
 * @brief Example showing how to integrate Lua scripting with ESP32
 * 
 * This example demonstrates:
 * 1. Setting up Lua scripting system on ESP32
 * 2. Creating a canvas for graphics
 * 3. Executing Lua scripts for UI animation
 * 4. Memory management considerations
 */

#ifdef ESP32

#include <enjin2/scripting/lua_platform.hpp>
#include <enjin2/scripting/bindings.hpp>
#include <enjin2/graphics/canvas.hpp>
#include <enjin2/graphics/primitives.hpp>

// ESP32-specific includes
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"
#include "esp_heap_caps.h"
#include "esp_spiffs.h"

using namespace enjin2;

static const char* TAG = "ESP32_LUA";

/**
 * @brief ESP32 Lua Graphics Demo
 * 
 * Demonstrates scripted animations on ESP32-S3 with 128x64 OLED display
 */
class ESP32LuaDemo {
private:
    // Canvas for OLED display (4-bit for memory efficiency)
    Canvas4<128, 64> canvas;
    LuaCanvas luaCanvas;
    LuaScriptSystem scriptSystem;
    
    // Animation state
    uint32_t frameCount;
    uint32_t lastHeapCheck;
    
public:
    ESP32LuaDemo() : luaCanvas(&canvas), frameCount(0), lastHeapCheck(0) {
        ESP_LOGI(TAG, "Initializing ESP32 Lua Demo");
    }
    
    /**
     * @brief Initialize the demo
     * @return True if initialization successful
     */
    bool initialize() {
        // Initialize SPIFFS for script storage
        if (!initializeFileSystem()) {
            ESP_LOGE(TAG, "Failed to initialize file system");
            return false;
        }
        
        // Initialize Lua scripting system
        if (!scriptSystem.initialize()) {
            ESP_LOGE(TAG, "Failed to initialize Lua scripting system");
            return false;
        }
        
        // Set up canvas for Lua
        scriptSystem.setCanvas(&luaCanvas);
        
        // Load animation scripts
        loadDemoScripts();
        
        ESP_LOGI(TAG, "ESP32 Lua Demo initialized successfully");
        printMemoryStats();
        
        return true;
    }
    
    /**
     * @brief Run the main demo loop
     */
    void run() {
        ESP_LOGI(TAG, "Starting ESP32 Lua animation loop");
        
        while (true) {
            // Clear canvas
            canvas.clear(Pixel4(0));
            
            // Execute Lua animation frame
            executeAnimationFrame();
            
            // Update frame counter
            frameCount++;
            
            // Periodic memory monitoring
            if (frameCount % 60 == 0) {  // Every ~1 second at 60fps
                printMemoryStats();
            }
            
            // Simulate display update (actual implementation would update OLED)
            simulateDisplayUpdate();
            
            // Frame rate control (targeting 30fps for ESP32)
            vTaskDelay(pdMS_TO_TICKS(33));
        }
    }

private:
    /**
     * @brief Initialize SPIFFS file system for script storage
     * @return True if successful
     */
    bool initializeFileSystem() {
        esp_vfs_spiffs_conf_t conf = {
            .base_path = "/spiffs",
            .partition_label = nullptr,
            .max_files = 5,
            .format_if_mount_failed = true
        };
        
        esp_err_t ret = esp_vfs_spiffs_register(&conf);
        if (ret != ESP_OK) {
            ESP_LOGE(TAG, "Failed to initialize SPIFFS (%s)", esp_err_to_name(ret));
            return false;
        }
        
        size_t total = 0, used = 0;
        esp_spiffs_info(nullptr, &total, &used);
        ESP_LOGI(TAG, "SPIFFS: %d KB total, %d KB used", total / 1024, used / 1024);
        
        return true;
    }
    
    /**
     * @brief Load demonstration Lua scripts
     */
    void loadDemoScripts() {
        // Create sample animation script in memory
        const char* animationScript = R"lua(
-- ESP32 Lua Animation Demo
local frame = 0
local centerX = getWidth() / 2
local centerY = getHeight() / 2

function animate()
    frame = frame + 1
    
    -- Animated circle
    local radius = 10 + math.sin(frame * 0.1) * 5
    setColor(15)  -- White
    circle("fill", centerX, centerY, radius)
    
    -- Orbiting dots
    for i = 0, 3 do
        local angle = (frame * 0.05) + (i * math.pi / 2)
        local x = centerX + math.cos(angle) * 20
        local y = centerY + math.sin(angle) * 20
        setColor(8 + i)  -- Different colors
        circle("fill", x, y, 3)
    end
    
    -- Frame counter
    setColor(15)
    -- Note: text rendering would require additional bindings
    -- For now, we'll draw a simple indicator
    local barWidth = (frame % 60) * 2
    rectangle("fill", 2, 2, barWidth, 4)
end
)lua";
        
        // Execute the animation script
        LuaResult result = scriptSystem.executeScript(animationScript);
        if (!result.success) {
            ESP_LOGE(TAG, "Failed to load animation script: %s", result.error.c_str());
        } else {
            ESP_LOGI(TAG, "Animation script loaded successfully");
        }
    }
    
    /**
     * @brief Execute one animation frame
     */
    void executeAnimationFrame() {
        // Call Lua animate function
        LuaResult result = scriptSystem.callFunction("animate");
        if (!result.success) {
            ESP_LOGE(TAG, "Animation frame error: %s", result.error.c_str());
            
            // Fallback: native animation
            drawFallbackAnimation();
        }
    }
    
    /**
     * @brief Fallback animation when Lua fails
     */
    void drawFallbackAnimation() {
        // Simple native circle animation
        int16_t centerX = canvas.getWidth() / 2;
        int16_t centerY = canvas.getHeight() / 2;
        uint16_t radius = 10 + (frameCount % 20);
        
        Primitives4::drawCircle(canvas, centerX, centerY, radius, Pixel4(15));
    }
    
    /**
     * @brief Print memory usage statistics
     */
    void printMemoryStats() {
        // ESP32 heap information
        multi_heap_info_t heap_info;
        heap_caps_get_info(&heap_info, MALLOC_CAP_8BIT);
        
        // Lua memory usage
        size_t lua_memory = scriptSystem.getMemoryUsage();
        
        ESP_LOGI(TAG, "Memory Stats - Frame: %lu", frameCount);
        ESP_LOGI(TAG, "  ESP32 Free Heap: %d KB", heap_info.total_free_bytes / 1024);
        ESP_LOGI(TAG, "  Lua Memory: %d KB", lua_memory / 1024);
        ESP_LOGI(TAG, "  Canvas Memory: %d bytes", canvas.getMemoryUsage());
        
        // Warning if memory is getting low
        if (heap_info.total_free_bytes < 100 * 1024) {  // Less than 100KB
            ESP_LOGW(TAG, "Low memory warning: %d KB free", heap_info.total_free_bytes / 1024);
        }
    }
    
    /**
     * @brief Simulate display update (placeholder for actual OLED update)
     */
    void simulateDisplayUpdate() {
        // In real implementation, this would:
        // 1. Transfer canvas buffer to OLED via SPI/I2C
        // 2. Handle display refresh timing
        // 3. Manage display power states
        
        static uint32_t last_update = 0;
        uint32_t now = xTaskGetTickCount() * portTICK_PERIOD_MS;
        
        if (now - last_update >= 33) {  // ~30fps
            // ESP_LOGD(TAG, "Display updated - Frame %lu", frameCount);
            last_update = now;
        }
    }
};

/**
 * @brief ESP32 FreeRTOS task for Lua demo
 * @param pvParameters Task parameters (unused)
 */
extern "C" void esp32_lua_demo_task(void* pvParameters) {
    ESP32LuaDemo demo;
    
    if (demo.initialize()) {
        demo.run();
    } else {
        ESP_LOGE(TAG, "Demo initialization failed");
    }
    
    // Should never reach here
    vTaskDelete(nullptr);
}

/**
 * @brief Main entry point for ESP32 Lua integration example
 */
extern "C" void app_main() {
    ESP_LOGI(TAG, "ESP32 Lua Integration Example Starting");
    ESP_LOGI(TAG, "ESP32-S3 @ %d MHz, %d KB RAM", 
             CONFIG_ESP32S3_DEFAULT_CPU_FREQ_MHZ,
             CONFIG_ESP32S3_SPIRAM_SUPPORT ? 8192 : 512);
    
    // Create demo task
    xTaskCreate(
        esp32_lua_demo_task,
        "lua_demo",
        8192,  // 8KB stack
        nullptr,
        5,     // Priority
        nullptr
    );
}

#else
#include <iostream>
int main() {
    std::cout << "This example requires ESP32 build environment." << std::endl;
    std::cout << "Build with: idf.py build" << std::endl;
    return 0;
}
#endif // ESP32