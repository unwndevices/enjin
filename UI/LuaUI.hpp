#ifndef LUAUI_HPP
#define LUAUI_HPP

#if defined(VCV_RACK) || defined(ESP32)

#include "../Components/C_Draw.hpp"
#include "../Object.hpp"
#include "enjin2/core/object.hpp"
#include "enjin2/components/canvas.hpp"
#include "enjin2/components/position.hpp"
#include "enjin2/scripting/bindings.hpp"
#include "enjin2/scripting/lua_platform.hpp"
#include <algorithm>

#ifdef ESP32
#include "esp_system.h"
#include "esp_heap_caps.h"
#include "esp_log.h"
#endif

namespace enjin
{
    template <size_t W = 32, size_t H = 32>
    class LuaUI : public Object
    {
    public:
        LuaUI(int x = 64, int y = 64)
            : canvasWidth(W), canvasHeight(H), frameSkipCounter(0),
              updateFuncRef(LUA_NOREF), drawFuncRef(LUA_NOREF), functionsAreCached(false)
#ifdef ESP32
              , lastMemoryCheck(0), lowMemoryMode(false)
#endif
        {
            // Set position
            position->SetPosition(x, y);

            // Create enjin2 canvas and object
            enjin2Object = std::make_shared<enjin2::Object>();
            enjin2Canvas = enjin2Object->addComponent<enjin2::C_Canvas>(W, H);

            // Position the enjin2 object
            auto enjin2Position = enjin2Object->addComponent<enjin2::C_Position>();
            enjin2Position->setPosition(x, y);

            // Initialize platform-aware Lua scripting system
            luaScriptSystem = std::unique_ptr<enjin2::LuaScriptSystem>(new enjin2::LuaScriptSystem());
            luaAnimationTime = 0;

            if (luaScriptSystem->initialize())
            {
                // Create wrapper for the canvas
                auto &canvas = enjin2Canvas->template getCanvas<W, H>();
                luaCanvasWrapper = std::unique_ptr<enjin2::LuaCanvas>(new enjin2::LuaCanvas(&canvas));
                luaScriptSystem->setCanvas(luaCanvasWrapper.get());

#ifdef ESP32
                // ESP32-specific initialization
                printPlatformInfo();

                // Set more conservative frame skip for ESP32
                LUA_UPDATE_SKIP_ESP32 = determineLuaUpdateSkip();
#endif
            }
            else
            {
#ifdef ESP32
                ESP_LOGE("LuaUI", "Failed to initialize Lua scripting system");
#else
                printf("LuaUI: Failed to initialize Lua scripting system\n");
#endif
            }

            // Initialize the object
            enjin2Object->awake();
            enjin2Object->start();

            // Add draw component to integrate with enjin rendering
            draw = AddComponent<C_Draw>([this](EiseiCanvas &canvas)
                                        { Draw(canvas); });
            draw->SetDrawLayer(DrawLayer::UI);
            draw->SetBlendMode(BlendMode::Normal);
            draw->SetAnchorPoint(Anchor::TOP_LEFT);
            draw->SetVisibility(true);
        }

        ~LuaUI()
        {
            // Clean up cached function references
            clearFunctionCache();
        }

        void SetVisibility(bool visibility)
        {
            draw->SetVisibility(visibility);
        }

        void SetPosition(int x, int y)
        {
            position->SetPosition(x, y);
            if (enjin2Object)
            {
                auto enjin2Position = enjin2Object->getComponent<enjin2::C_Position>();
                if (enjin2Position)
                {
                    enjin2Position->setPosition(x, y);
                }
            }
        }

        bool LoadScript(const std::string &script)
        {
            if (!luaScriptSystem)
                return false;

            // Clear existing cached function references
            clearFunctionCache();

#ifdef ESP32
            // ESP32: Check available memory before loading script
            size_t freeHeap = getFreeHeapSize();
            if (freeHeap < 50 * 1024) // Less than 50KB free
            {
                ESP_LOGW("LuaUI", "Low memory (%zu KB), script might fail", freeHeap / 1024);
                lowMemoryMode = true;
            }
#endif

            auto result = luaScriptSystem->executeScript(script);
            if (!result.success)
            {
#ifdef ESP32
                ESP_LOGE("LuaUI", "Lua script error: %s", result.error.c_str());
#else
                printf("Lua script error: %s\n", result.error.c_str());
#endif
                return false;
            }

            // Cache function references for better performance
            cacheFunctionReferences();

#ifdef ESP32
            // ESP32: Print memory usage after script load
            printMemoryStats();
#endif

            return true;
        }

        bool LoadScriptFromFile(const std::string &filename)
        {
            if (!luaScriptSystem)
                return false;

            auto result = luaScriptSystem->loadScript(filename);
            if (!result.success)
            {
                printf("Lua script file error: %s\n", result.error.c_str());
                return false;
            }
            return true;
        }

        void Update(uint16_t deltaTime) override
        {
            Object::Update(deltaTime);

            if (luaScriptSystem && enjin2Canvas)
            {
                luaAnimationTime += deltaTime;

#ifdef ESP32
                // ESP32: Check memory usage periodically
                if (luaAnimationTime - lastMemoryCheck > 2000) // Every 2 seconds
                {
                    checkMemoryUsage();
                    lastMemoryCheck = luaAnimationTime;
                }

                // Use platform-specific frame skip
                uint16_t frameSkip = lowMemoryMode ? (LUA_UPDATE_SKIP_ESP32 * 2) : LUA_UPDATE_SKIP_ESP32;
#else
                // Desktop: Use original frame skip
                uint16_t frameSkip = LUA_UPDATE_SKIP;
#endif

                // Frame skipping for performance - only update Lua logic every Nth frame
                if (++frameSkipCounter >= frameSkip)
                {
                    frameSkipCounter = 0;

                    double timeInSeconds = luaAnimationTime / 1000.0;

                    // Set time variables for Lua script
                    luaScriptSystem->getEngine().setGlobal("dt", deltaTime / 1000.0);
                    luaScriptSystem->getEngine().setGlobal("time", timeInSeconds);

#ifdef ESP32
                    // ESP32: Add platform-specific globals
                    luaScriptSystem->getEngine().setGlobal("free_heap", getFreeHeapSize());
                    luaScriptSystem->getEngine().setGlobal("low_memory_mode", lowMemoryMode);
#endif

                    // Call cached Lua functions for better performance
                    if (functionsAreCached)
                    {
                        // Call cached update function
                        if (updateFuncRef != LUA_NOREF)
                        {
                            auto updateResult = callCachedFunction(updateFuncRef, deltaTime / 1000.0, timeInSeconds);
                            if (!updateResult.success)
                            {
                                handleLuaError("update", updateResult.error);
                            }
                        }

                        // Call cached draw function (skip in low memory mode on ESP32)
#ifdef ESP32
                        if (!lowMemoryMode && drawFuncRef != LUA_NOREF)
#else
                        if (drawFuncRef != LUA_NOREF)
#endif
                        {
                            auto drawResult = callCachedFunction(drawFuncRef);
                            if (!drawResult.success)
                            {
                                handleLuaError("draw", drawResult.error);
                            }
                        }
                    }
                    else
                    {
                        // Fallback to function name lookup (slower but more compatible)
                        auto updateResult = luaScriptSystem->callFunction("update", deltaTime / 1000.0, timeInSeconds);
#ifdef ESP32
                        if (!lowMemoryMode)
#endif
                        {
                            auto drawResult = luaScriptSystem->callFunction("draw");
                        }
                    }
                }
            }

            // Update enjin2 object (less frequently)
            if (enjin2Object && frameSkipCounter == 0)
            {
                enjin2Object->update(deltaTime);
            }
        }

        void Draw(EiseiCanvas &canvas)
        {
            if (!enjin2Canvas)
                return;

            // Get the enjin2 canvas and copy its pixels to the enjin canvas
            auto &enjin2CanvasRef = enjin2Canvas->template getCanvas<W, H>();

            int x = position->x;
            int y = position->y;

            // Optimized bulk pixel copying
            const int canvasW = canvas.width();
            const int canvasH = canvas.height();

            // Bounds check once
            const int maxX = std::min(x + canvasWidth, canvasW);
            const int maxY = std::min(y + canvasHeight, canvasH);
            const int startX = std::max(x, 0);
            const int startY = std::max(y, 0);

            // Copy only visible region
            for (int py = startY; py < maxY; py++)
            {
                const int srcY = py - y;
                for (int px = startX; px < maxX; px++)
                {
                    const int srcX = px - x;
                    uint8_t pixel = enjin2CanvasRef.getPixel(srcX, srcY);
                    if (pixel > 0)
                    {
                        canvas.setPixel(px, py, pixel);
                    }
                }
            }
        }

        // Lua script system access
        enjin2::LuaScriptSystem *getLuaSystem() { return luaScriptSystem.get(); }
        enjin2::LuaCanvas *getLuaCanvas() { return luaCanvasWrapper.get(); }

    private:
        // Caching helper methods
        void cacheFunctionReferences()
        {
            if (!luaScriptSystem)
                return;

            lua_State *L = luaScriptSystem->getEngine().getState();
            if (!L)
                return;

            // Cache update function reference
            lua_getglobal(L, "update");
            if (lua_isfunction(L, -1))
            {
                updateFuncRef = luaL_ref(L, LUA_REGISTRYINDEX);
            }
            else
            {
                lua_pop(L, 1);
                updateFuncRef = LUA_NOREF;
            }

            // Cache draw function reference
            lua_getglobal(L, "draw");
            if (lua_isfunction(L, -1))
            {
                drawFuncRef = luaL_ref(L, LUA_REGISTRYINDEX);
            }
            else
            {
                lua_pop(L, 1);
                drawFuncRef = LUA_NOREF;
            }

            functionsAreCached = true;
        }

        void clearFunctionCache()
        {
            if (!luaScriptSystem)
                return;

            lua_State *L = luaScriptSystem->getEngine().getState();
            if (!L)
                return;

            if (updateFuncRef != LUA_NOREF)
            {
                luaL_unref(L, LUA_REGISTRYINDEX, updateFuncRef);
                updateFuncRef = LUA_NOREF;
            }

            if (drawFuncRef != LUA_NOREF)
            {
                luaL_unref(L, LUA_REGISTRYINDEX, drawFuncRef);
                drawFuncRef = LUA_NOREF;
            }

            functionsAreCached = false;
        }

        template <typename... Args>
        enjin2::LuaResult callCachedFunction(int funcRef, Args... args)
        {
            if (!luaScriptSystem || funcRef == LUA_NOREF)
            {
                enjin2::LuaResult result;
                result.success = false;
                result.error = "Function not cached";
                return result;
            }

            lua_State *L = luaScriptSystem->getEngine().getState();
            if (!L)
            {
                enjin2::LuaResult result;
                result.success = false;
                result.error = "No Lua state";
                return result;
            }

            // Push cached function onto stack
            lua_rawgeti(L, LUA_REGISTRYINDEX, funcRef);

            // Push arguments
            int argCount = 0;
            pushLuaArgs(L, argCount, args...);

            // Call function
            if (lua_pcall(L, argCount, 0, 0) != LUA_OK)
            {
                std::string error = lua_tostring(L, -1);
                lua_pop(L, 1);
                enjin2::LuaResult result;
                result.success = false;
                result.error = error;
                return result;
            }

            enjin2::LuaResult result;
            result.success = true;
            result.error = "";
            return result;
        }

        // Helper to push arguments recursively
        void pushLuaArgs(lua_State *L, int &count)
        {
            // Base case - no more arguments
        }

        template <typename T, typename... Args>
        void pushLuaArgs(lua_State *L, int &count, T &&first, Args &&...rest)
        {
            pushLuaArg(L, std::forward<T>(first));
            count++;
            pushLuaArgs(L, count, std::forward<Args>(rest)...);
        }

        void pushLuaArg(lua_State *L, double value) { lua_pushnumber(L, value); }
        void pushLuaArg(lua_State *L, float value) { lua_pushnumber(L, value); }
        void pushLuaArg(lua_State *L, int value) { lua_pushinteger(L, value); }
        void pushLuaArg(lua_State *L, const std::string &value) { lua_pushstring(L, value.c_str()); }
        void pushLuaArg(lua_State *L, const char *value) { lua_pushstring(L, value); }

#ifdef ESP32
        // ESP32-specific helper methods
        void printPlatformInfo()
        {
            esp_chip_info_t chip_info;
            esp_chip_info(&chip_info);

            ESP_LOGI("LuaUI", "ESP32 Chip: %s, %d cores, WiFi%s%s",
                     CONFIG_IDF_TARGET,
                     chip_info.cores,
                     (chip_info.features & CHIP_FEATURE_BT) ? "/BT" : "",
                     (chip_info.features & CHIP_FEATURE_BLE) ? "/BLE" : "");

            size_t freeHeap = heap_caps_get_free_size(MALLOC_CAP_8BIT);
            ESP_LOGI("LuaUI", "Free heap: %zu KB", freeHeap / 1024);
        }

        uint16_t determineLuaUpdateSkip()
        {
            size_t freeHeap = getFreeHeapSize();

            if (freeHeap > 200 * 1024)
            {
                return 1; // Update every 2nd frame (30fps) with plenty of memory
            }
            else if (freeHeap > 100 * 1024)
            {
                return 2; // Update every 3rd frame (20fps) with moderate memory
            }
            else
            {
                return 4; // Update every 5th frame (12fps) with low memory
            }
        }

        size_t getFreeHeapSize()
        {
            return heap_caps_get_free_size(MALLOC_CAP_8BIT);
        }

        void checkMemoryUsage()
        {
            size_t freeHeap = getFreeHeapSize();
            size_t luaMemory = luaScriptSystem ? luaScriptSystem->getMemoryUsage() : 0;

            // Check if we should enter/exit low memory mode
            if (!lowMemoryMode && freeHeap < 75 * 1024)
            {
                lowMemoryMode = true;
                ESP_LOGW("LuaUI", "Entering low memory mode (Free: %zu KB)", freeHeap / 1024);
            }
            else if (lowMemoryMode && freeHeap > 150 * 1024)
            {
                lowMemoryMode = false;
                ESP_LOGI("LuaUI", "Exiting low memory mode (Free: %zu KB)", freeHeap / 1024);
            }

            // Warning if memory is critically low
            if (freeHeap < 50 * 1024)
            {
                ESP_LOGE("LuaUI", "Critical memory: %zu KB free, %zu KB Lua",
                         freeHeap / 1024, luaMemory / 1024);
            }
        }

        void printMemoryStats()
        {
            size_t freeHeap = getFreeHeapSize();
            size_t luaMemory = luaScriptSystem ? luaScriptSystem->getMemoryUsage() : 0;

            ESP_LOGI("LuaUI", "Memory - Free: %zu KB, Lua: %zu KB, Canvas: %zu bytes",
                     freeHeap / 1024, luaMemory / 1024, W * H / 2);
        }

        void handleLuaError(const char *context, const std::string &error)
        {
            // Only print errors occasionally to avoid spam
            static uint32_t lastErrorTime = 0;
            if (luaAnimationTime - lastErrorTime > 5000) // Every 5 seconds max
            {
                ESP_LOGE("LuaUI", "Lua %s error: %s", context, error.c_str());
                lastErrorTime = luaAnimationTime;
            }
        }
#else
        // Desktop fallback for error handling
        void handleLuaError(const char *context, const std::string &error)
        {
            // Only print errors occasionally to avoid spam
            static uint32_t lastErrorTime = 0;
            if (luaAnimationTime - lastErrorTime > 5000) // Every 5 seconds max
            {
                printf("Lua %s error: %s\n", context, error.c_str());
                lastErrorTime = luaAnimationTime;
            }
        }
#endif

        std::shared_ptr<C_Draw> draw;

        // enjin2 components
        std::shared_ptr<enjin2::Object> enjin2Object;
        enjin2::C_Canvas *enjin2Canvas;
        std::unique_ptr<enjin2::LuaScriptSystem> luaScriptSystem;
        std::unique_ptr<enjin2::LuaCanvas> luaCanvasWrapper;

        // Configuration
        int canvasWidth;
        int canvasHeight;
        uint32_t luaAnimationTime;

        // Performance optimization
        mutable uint16_t frameSkipCounter;
        static constexpr uint16_t LUA_UPDATE_SKIP = 0; // Update Lua every frame on desktop

        // Function reference caching for better performance
        mutable int updateFuncRef;
        mutable int drawFuncRef;
        mutable bool functionsAreCached;

        // ESP32-specific members
#ifdef ESP32
        uint32_t lastMemoryCheck;
        bool lowMemoryMode;
        mutable uint16_t LUA_UPDATE_SKIP_ESP32 = 2; // Default frame skip for ESP32
#endif
    };
} // namespace enjin

#endif // VCV_RACK || ESP32

#endif // LUAUI_HPP