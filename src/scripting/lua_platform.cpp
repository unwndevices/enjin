#include "../../include/enjin2/scripting/lua_platform.hpp"
#include <cstring>
#include <iostream>

#ifdef VCV_RACK
    #include <fstream>
#endif

#ifdef ESP32
    #include "esp_system.h"
    #include "esp_heap_caps.h"
    #include "esp_spiffs.h"
    #include "esp_vfs.h"
#endif

namespace enjin2 {

//==============================================================================
// LuaPlatform Implementation
//==============================================================================

lua_State* LuaPlatform::createState(lua_Alloc allocator, void* ud) {
#ifdef VCV_RACK
    // Desktop: Use standard Lua state creation
    if (allocator) {
        return lua_newstate(allocator, ud);
    } else {
        return luaL_newstate();
    }
    
#elif defined(ESP32)
    // ESP32: Use custom allocator with heap caps for optimal memory management
    if (!allocator) {
        // Default ESP32 allocator using DMA-capable memory when possible
        allocator = [](void* ud, void* ptr, size_t osize, size_t nsize) -> void* {
            if (nsize == 0) {
                if (ptr) {
                    heap_caps_free(ptr);
                }
                return nullptr;
            }
            
            if (ptr == nullptr) {
                // Try DMA-capable memory first, fall back to regular heap
                void* new_ptr = heap_caps_malloc(nsize, MALLOC_CAP_DMA);
                if (!new_ptr) {
                    new_ptr = heap_caps_malloc(nsize, MALLOC_CAP_8BIT);
                }
                return new_ptr;
            } else {
                // Reallocate
                return heap_caps_realloc(ptr, nsize, MALLOC_CAP_8BIT);
            }
        };
    }
    
    lua_State* L = lua_newstate(allocator, ud);
    if (L) {
        configureESP32Memory(L);
    }
    return L;
#endif
}

void LuaPlatform::openLibraries(lua_State* L) {
    if (!L) return;
    
#ifdef VCV_RACK
    openDesktopLibraries(L);
#elif defined(ESP32)
    openEmbeddedLibraries(L);
#endif
    
    // Configure security restrictions after opening libraries
    configureSecurityRestrictions(L);
}

void LuaPlatform::configureSecurityRestrictions(lua_State* L) {
    if (!L) return;
    
    // Common security restrictions for embedded environments
    if (!LuaPlatformConfig::ENABLE_FILE_IO) {
        // Disable potentially dangerous file I/O functions
        lua_pushnil(L);
        lua_setglobal(L, "dofile");
        lua_pushnil(L);
        lua_setglobal(L, "loadfile");
        lua_pushnil(L);
        lua_setglobal(L, "require");
        
        // Remove io library if it was loaded
        lua_pushnil(L);
        lua_setglobal(L, "io");
    }
    
    if (!LuaPlatformConfig::ENABLE_DEBUG) {
        // Remove debug library
        lua_pushnil(L);
        lua_setglobal(L, "debug");
    }
    
#ifdef ESP32
    // ESP32-specific restrictions
    lua_pushnil(L);
    lua_setglobal(L, "package");  // Disable package system
    lua_pushnil(L);
    lua_setglobal(L, "os");       // Disable OS access
#endif
}

size_t LuaPlatform::getMemoryUsage(lua_State* L) {
    if (!L) return 0;
    
#ifdef VCV_RACK
    // Desktop: Use Lua's built-in memory reporting
    return lua_gc(L, LUA_GCCOUNT, 0) * 1024 + lua_gc(L, LUA_GCCOUNTB, 0);
    
#elif defined(ESP32)
    // ESP32: Combine Lua memory with ESP32 heap info
    size_t lua_memory = lua_gc(L, LUA_GCCOUNT, 0) * 1024 + lua_gc(L, LUA_GCCOUNTB, 0);
    
    // Add ESP32 heap information for debugging
    multi_heap_info_t info;
    heap_caps_get_info(&info, MALLOC_CAP_8BIT);
    
    // Return Lua memory usage (ESP32 heap info available separately)
    return lua_memory;
#endif
}

void LuaPlatform::tuneGarbageCollector(lua_State* L) {
    if (!L) return;
    
#ifdef VCV_RACK
    // Desktop: Use standard GC settings
    lua_gc(L, LUA_GCSETPAUSE, 200);     // Run GC when memory grows by 200%
    lua_gc(L, LUA_GCSETSTEPMUL, 200);   // GC speed multiplier
    
#elif defined(ESP32)
    // ESP32: More aggressive GC for memory-constrained environment
    lua_gc(L, LUA_GCSETPAUSE, 120);     // Run GC when memory grows by 120%
    lua_gc(L, LUA_GCSETSTEPMUL, 400);   // Faster GC stepping
    
    // Set memory limits based on available heap
    size_t free_heap = heap_caps_get_free_size(MALLOC_CAP_8BIT);
    if (free_heap < 100 * 1024) {  // Less than 100KB free
        // Very aggressive GC
        lua_gc(L, LUA_GCSETPAUSE, 100);
        lua_gc(L, LUA_GCSETSTEPMUL, 600);
    }
#endif
}

//==============================================================================
// Platform-specific Library Management
//==============================================================================

#ifdef VCV_RACK
void LuaPlatform::openDesktopLibraries(lua_State* L) {
    // Desktop: Open all standard libraries
    luaL_openlibs(L);
}
#endif

#ifdef ESP32
void LuaPlatform::openEmbeddedLibraries(lua_State* L) {
    // Lua 5.1 (used on ESP32) does not have luaL_requiref or per-library open functions
    // (coroutines are part of base in 5.1; luaL_requiref was added in 5.2).
    // Open all standard libs — io/os/debug are not harmful on a constrained device
    // and selective loading is not available in Lua 5.1.
    luaL_openlibs(L);
}

void LuaPlatform::configureESP32Memory(lua_State* L) {
    // Configure Lua for ESP32 memory constraints
    
    // Set reasonable stack size limit
    lua_checkstack(L, LuaPlatformConfig::STACK_SIZE / 8);  // Rough estimate
    
    // Configure GC for ESP32
    tuneGarbageCollector(L);
}
#endif

//==============================================================================
// LuaFileSystem Implementation
//==============================================================================

bool LuaFileSystem::isFileIOSupported() {
    return LuaPlatformConfig::ENABLE_FILE_IO;
}

bool LuaFileSystem::readScriptFile(const std::string& filename, std::string& content) {
#ifdef VCV_RACK
    return readDesktopFile(filename, content);
#elif defined(ESP32)
    return readESP32File(filename, content);
#else
    return false;
#endif
}

std::vector<std::string> LuaFileSystem::listScriptFiles(const std::string& path) {
    std::vector<std::string> files;
    
    if (!isFileIOSupported()) {
        return files;
    }
    
#ifdef VCV_RACK
    // Desktop implementation would use standard filesystem APIs
    // For now, return empty vector - implement as needed
    
#elif defined(ESP32)
    // ESP32 SPIFFS implementation
    // For now, return empty vector - implement as needed
#endif
    
    return files;
}

//==============================================================================
// Platform-specific File I/O
//==============================================================================

#ifdef VCV_RACK
bool LuaFileSystem::readDesktopFile(const std::string& filename, std::string& content) {
    std::ifstream file(filename);
    if (!file.is_open()) {
        return false;
    }
    
    content.assign((std::istreambuf_iterator<char>(file)),
                   std::istreambuf_iterator<char>());
    file.close();
    return true;
}
#endif

#ifdef ESP32
bool LuaFileSystem::readESP32File(const std::string& filename, std::string& content) {
    // ESP32 SPIFFS/LittleFS implementation
    FILE* file = fopen(filename.c_str(), "r");
    if (!file) {
        return false;
    }
    
    // Get file size
    fseek(file, 0, SEEK_END);
    long size = ftell(file);
    fseek(file, 0, SEEK_SET);
    
    if (size <= 0) {
        fclose(file);
        return false;
    }
    
    // Read file content
    content.resize(size);
    size_t bytes_read = fread(&content[0], 1, size, file);
    fclose(file);
    
    if (bytes_read != static_cast<size_t>(size)) {
        content.clear();
        return false;
    }
    
    return true;
}
#endif

} // namespace enjin2