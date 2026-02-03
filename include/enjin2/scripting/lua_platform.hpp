/**
 * @file lua_platform.hpp
 * @brief Platform-specific Lua configuration and file system abstraction
 *
 * Provides platform-appropriate Lua initialization, library loading,
 * and file I/O for desktop (VCV Rack) and ESP32 platforms.
 */
#pragma once

#include "../core/types.hpp"
#include <string>
#include <vector>

// Platform-specific Lua includes
#ifdef VCV_RACK
    // LuaJIT for VCV Rack (desktop)
    extern "C" {
        #include "lua.h"
        #include "lauxlib.h"
        #include "lualib.h"
    }
    // LuaJIT compatibility: map lua_pcallk to lua_pcall only for Lua 5.1
    #if !defined(LUA_VERSION_NUM) || LUA_VERSION_NUM < 502
        #ifndef lua_pcallk
        #define lua_pcallk(L,n,r,c,ctx,k) lua_pcall(L,n,r,c)
        #endif
    #endif
#elif defined(ESP32)
    // ESP32-specific Lua implementation
    extern "C" {
        #include "lua.h"
        #include "lauxlib.h"
        #include "lualib.h"
    }
#else
    #error "Platform not supported for Lua integration"
#endif

namespace enjin2 {

/**
 * @brief Platform-specific Lua configuration
 */
struct LuaPlatformConfig {
#ifdef VCV_RACK
    static constexpr size_t MEMORY_LIMIT = 1024 * 1024;  // 1MB for desktop
    static constexpr bool ENABLE_ALL_LIBS = true;        // Full Lua libraries
    static constexpr bool ENABLE_FILE_IO = true;         // File operations allowed
    static constexpr bool ENABLE_DEBUG = true;           // Debug facilities
#elif defined(ESP32)
    static constexpr size_t MEMORY_LIMIT = 64 * 1024;    // 64KB for ESP32
    static constexpr bool ENABLE_ALL_LIBS = false;       // Minimal libraries only
    static constexpr bool ENABLE_FILE_IO = false;        // No file I/O for security
    static constexpr bool ENABLE_DEBUG = false;          // No debug to save memory
#endif
    
    // Common settings
    static constexpr size_t STACK_SIZE = 8192;            // 8KB Lua stack
    static constexpr int MAX_RECURSION_DEPTH = 32;       // Prevent stack overflow
};

/**
 * @brief Platform abstraction for Lua initialization
 */
class LuaPlatform {
public:
    /**
     * @brief Create platform-appropriate Lua state
     * @param allocator Custom allocator function (nullptr for default)
     * @param ud User data for allocator
     * @return Lua state or nullptr on failure
     */
    static lua_State* createState(lua_Alloc allocator = nullptr, void* ud = nullptr);
    
    /**
     * @brief Open platform-appropriate standard libraries
     * @param L Lua state
     */
    static void openLibraries(lua_State* L);
    
    /**
     * @brief Configure platform-specific security restrictions
     * @param L Lua state
     */
    static void configureSecurityRestrictions(lua_State* L);
    
    /**
     * @brief Get platform memory statistics
     * @param L Lua state
     * @return Memory usage in bytes
     */
    static size_t getMemoryUsage(lua_State* L);
    
    /**
     * @brief Platform-specific garbage collection tuning
     * @param L Lua state
     */
    static void tuneGarbageCollector(lua_State* L);

private:
    // Platform-specific implementations
#ifdef VCV_RACK
    static void openDesktopLibraries(lua_State* L);
#elif defined(ESP32)
    static void openEmbeddedLibraries(lua_State* L);
    static void configureESP32Memory(lua_State* L);
#endif
};

/**
 * @brief Platform-specific file system interface for Lua scripts
 */
class LuaFileSystem {
public:
    /**
     * @brief Check if file operations are supported on this platform
     * @return True if file I/O is available
     */
    static bool isFileIOSupported();
    
    /**
     * @brief Read script file content (platform-specific)
     * @param filename Script file path
     * @param content Output buffer for file content
     * @return True if successful
     */
    static bool readScriptFile(const std::string& filename, std::string& content);
    
    /**
     * @brief List available script files (platform-specific)
     * @param path Directory path to search
     * @return Vector of script filenames
     */
    static std::vector<std::string> listScriptFiles(const std::string& path = "");

private:
#ifdef VCV_RACK
    static bool readDesktopFile(const std::string& filename, std::string& content);
#elif defined(ESP32)
    static bool readESP32File(const std::string& filename, std::string& content);
#endif
};

} // namespace enjin2
