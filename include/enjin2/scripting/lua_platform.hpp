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
    // Lua 5.4 for desktop (system package)
    extern "C" {
        #include "lua.h"
        #include "lauxlib.h"
        #include "lualib.h"
    }
#elif defined(ESP32)
    // Lua 5.4.8 for ESP32 (built from source via FetchContent)
    // Xtensa newlib <limits.h> may not expose LLONG_MAX in C++ mode;
    // Lua 5.4 luaconf.h uses it as a proxy for long long support.
    #include <climits>
    #ifndef LLONG_MAX
    #define LLONG_MAX __LONG_LONG_MAX__
    #define LLONG_MIN (-LLONG_MAX - 1LL)
    #define ULLONG_MAX (2ULL * LLONG_MAX + 1ULL)
    #endif
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
    static constexpr size_t MEMORY_LIMIT = 1024 * 1024;  ///< Memory limit for desktop (1MB)
    static constexpr bool ENABLE_ALL_LIBS = true;         ///< Enable all Lua libraries on desktop
    static constexpr bool ENABLE_FILE_IO = true;          ///< Enable file operations on desktop
    static constexpr bool ENABLE_DEBUG = true;            ///< Enable debug facilities on desktop
#elif defined(ESP32)
    static constexpr size_t MEMORY_LIMIT = 2 * 1024 * 1024; ///< Memory limit for ESP32 (2MB from PSRAM)
    static constexpr bool ENABLE_ALL_LIBS = false;        ///< Minimal libraries only on ESP32
    static constexpr bool ENABLE_FILE_IO = false;         ///< No file I/O on ESP32 for security
    static constexpr bool ENABLE_DEBUG = false;           ///< No debug on ESP32 to save memory
#endif

    // Common settings
    static constexpr size_t STACK_SIZE = 8192;            ///< Lua stack size (8KB)
    static constexpr int MAX_RECURSION_DEPTH = 32;       ///< Maximum recursion depth for stack overflow prevention
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
