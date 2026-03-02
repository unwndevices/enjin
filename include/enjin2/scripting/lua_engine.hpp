/**
 * @file lua_engine.hpp
 * @brief Lua engine for embedded scripting with static memory management
 *
 * Provides a lightweight Lua scripting environment optimized for embedded systems
 * with love2d.graphics-style API for familiar drawing operations.
 */
#pragma once

#include "../core/types.hpp"
#include "lua_platform.hpp"
#include <string>
#include <vector>
#include <functional>
#include <memory>

namespace enjin2 {

/**
 * @brief Lua script execution result
 */
struct LuaResult {
    bool success;           ///< Whether execution was successful
    std::string error;      ///< Error message if execution failed
    
    /**
     * @brief Constructor for successful result
     */
    LuaResult() : success(true) {}
    
    /**
     * @brief Constructor for failed result
     * @param errorMsg Error message
     */
    LuaResult(const std::string& errorMsg) : success(false), error(errorMsg) {}
};

/**
 * @brief Lua function callback type
 */
using LuaCallback = std::function<int(lua_State*)>;

/**
 * @brief Lua engine for embedded scripting support
 * 
 * Provides a lightweight Lua scripting environment optimized for embedded systems.
 * Features static memory management and love2d.graphics-style API for familiarity.
 */
class LuaEngine {
private:
    lua_State* L;                           ///< Lua state
    bool initialized;                       ///< Whether engine is initialized
    std::vector<std::string> loadedScripts; ///< List of loaded script names
    
    // Static memory management  
    static size_t memoryUsed;               ///< Current memory usage
    static char* memoryPool; ///< Memory pool pointer (PSRAM on ESP32, heap on desktop)
    
public:
    /**
     * @brief Constructor initializes Lua state
     */
    LuaEngine();
    
    /**
     * @brief Destructor cleans up Lua state
     */
    ~LuaEngine();
    
    /**
     * @brief Initialize the Lua engine
     * @return True if initialization successful
     */
    bool initialize();
    
    /**
     * @brief Shutdown the Lua engine
     */
    void shutdown();
    
    /**
     * @brief Check if engine is initialized
     * @return True if initialized
     */
    bool isInitialized() const { return initialized; }
    
    /**
     * @brief Execute Lua code string
     * @param code Lua code to execute
     * @return Execution result
     */
    LuaResult executeString(const std::string& code);
    
    /**
     * @brief Load and execute Lua script file
     * @param filename Path to Lua script file
     * @return Execution result
     */
    LuaResult executeFile(const std::string& filename);
    
    /**
     * @brief Register C function with Lua
     * @param name Function name in Lua
     * @param callback C function callback
     */
    void registerFunction(const std::string& name, LuaCallback callback);
    
    /**
     * @brief Register C function with Lua (C-style)
     * @param name Function name in Lua
     * @param func C function pointer
     */
    void registerFunction(const std::string& name, lua_CFunction func);
    
    /**
     * @brief Create Lua table
     * @param name Table name
     */
    void createTable(const std::string& name);

    /**
     * @brief Set global number variable in Lua
     * @param name Variable name
     * @param value Number value to set
     */
    void setGlobal(const std::string& name, double value);

    /**
     * @brief Set global string variable in Lua
     * @param name Variable name
     * @param value String value to set
     */
    void setGlobal(const std::string& name, const std::string& value);

    /**
     * @brief Set global boolean variable in Lua
     * @param name Variable name
     * @param value Boolean value to set
     */
    void setGlobal(const std::string& name, bool value);
    
    /**
     * @brief Get global number variable from Lua
     * @param name Variable name
     * @param defaultValue Default value if variable not found
     * @return Number value (or default if not found)
     */
    double getGlobalNumber(const std::string& name, double defaultValue = 0.0);

    /**
     * @brief Get global string variable from Lua
     * @param name Variable name
     * @param defaultValue Default value if variable not found
     * @return String value (or default if not found)
     */
    std::string getGlobalString(const std::string& name, const std::string& defaultValue = "");

    /**
     * @brief Get global boolean variable from Lua
     * @param name Variable name
     * @param defaultValue Default value if variable not found
     * @return Boolean value (or default if not found)
     */
    bool getGlobalBool(const std::string& name, bool defaultValue = false);
    
    /**
     * @brief Call Lua function
     * @param functionName Name of Lua function
     * @param args Function arguments
     * @return Execution result
     */
    template<typename... Args>
    LuaResult callFunction(const std::string& functionName, Args... args) {
        if (!initialized) {
            return LuaResult("Lua engine not initialized");
        }
        
        lua_getglobal(L, functionName.c_str());
        if (!lua_isfunction(L, -1)) {
            lua_pop(L, 1);
            return LuaResult("Function not found: " + functionName);
        }
        
        // Push arguments
        pushArgs(std::forward<Args>(args)...);
        
        // Call function
        int result = lua_pcall(L, sizeof...(args), 0, 0);
        return checkResult(result);
    }
    
    /**
     * @brief Get current memory usage
     * @return Memory usage in bytes
     */
    size_t getMemoryUsage() const;
    
    /**
     * @brief Get list of loaded scripts
     * @return Vector of script names
     */
    const std::vector<std::string>& getLoadedScripts() const { return loadedScripts; }
    
    /**
     * @brief Clear all loaded scripts
     */
    void clearScripts();
    
    /**
     * @brief Get Lua state (for advanced operations)
     * @return Lua state pointer
     */
    lua_State* getState() { return L; }

private:
    /**
     * @brief Custom Lua allocator using static memory pool
     * @param ud User data (LuaEngine instance)
     * @param ptr Pointer to reallocate
     * @param osize Original size
     * @param nsize New size
     * @return Allocated memory or nullptr
     */
    static void* luaAllocator(void* ud, void* ptr, size_t osize, size_t nsize);
    
    /**
     * @brief Handle Lua panic
     * @param L Lua state
     * @return Never returns
     */
    static int luaPanic(lua_State* L);
    
    /**
     * @brief Push single argument to Lua stack
     * @param arg Argument to push
     */
    template<typename T>
    void pushArg(T&& arg);
    
    /**
     * @brief Push single argument to Lua stack (base case)
     * @param arg Argument to push
     */
    // Simplified recursive template for C++11 compatibility
    template<typename T>
    void pushArgs(T&& arg) {
        pushArg(std::forward<T>(arg));
    }
    
    template<typename T, typename... Args>
    void pushArgs(T&& first, Args&&... rest) {
        pushArg(std::forward<T>(first));
        pushArgs(std::forward<Args>(rest)...);
    }
    
    /**
     * @brief Base case for pushArgs (no arguments)
     */
    void pushArgs() {}
    
    
    /**
     * @brief Check Lua execution result
     * @param result Lua function result code
     * @return LuaResult with success/error information
     */
    LuaResult checkResult(int result);
};

} // namespace enjin2