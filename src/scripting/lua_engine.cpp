#include "../../include/enjin2/scripting/lua_engine.hpp"
#include "../../include/enjin2/scripting/lua_platform.hpp"
#include <cstring>
#include <fstream>
#include <iostream>
#ifdef ESP32
#include <esp_heap_caps.h>
#endif

namespace enjin2 {

// Static member definitions
size_t LuaEngine::memoryUsed = 0;
char* LuaEngine::memoryPool = nullptr;

LuaEngine::LuaEngine() : L(nullptr), initialized(false) {
}

LuaEngine::~LuaEngine() {
    shutdown();
}

bool LuaEngine::initialize() {
    if (initialized) {
        return true;
    }
    
    // Allocate memory pool
#ifdef ESP32
    // Prefer PSRAM (MALLOC_CAP_SPIRAM) on ESP32-S3 with 8MB PSRAM to keep DRAM free.
    // Fall back to internal heap if PSRAM is unavailable.
    memoryPool = static_cast<char*>(
        heap_caps_malloc(LuaPlatformConfig::MEMORY_LIMIT, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT));
    if (!memoryPool) {
        memoryPool = static_cast<char*>(
            heap_caps_malloc(LuaPlatformConfig::MEMORY_LIMIT, MALLOC_CAP_8BIT));
    }
    if (!memoryPool) {
        return false;
    }
#else
    memoryPool = new char[LuaPlatformConfig::MEMORY_LIMIT];
#endif
    memoryUsed = 0;
    std::memset(memoryPool, 0, LuaPlatformConfig::MEMORY_LIMIT);
    
    // Create platform-appropriate Lua state
    L = LuaPlatform::createState();
    if (!L) {
        return false;
    }
    
    // Set panic handler
    lua_atpanic(L, luaPanic);
    
    // Open platform-appropriate libraries and configure security
    LuaPlatform::openLibraries(L);
    
    // Tune garbage collector for platform
    LuaPlatform::tuneGarbageCollector(L);
    
    initialized = true;
    return true;
}

void LuaEngine::shutdown() {
    if (L) {
        lua_close(L);
        L = nullptr;
    }
    initialized = false;
    loadedScripts.clear();
    memoryUsed = 0;
    if (memoryPool) {
#ifdef ESP32
        free(memoryPool);
#else
        delete[] memoryPool;
#endif
        memoryPool = nullptr;
    }
}

LuaResult LuaEngine::executeString(const std::string& code) {
    if (!initialized) {
        return LuaResult("Lua engine not initialized");
    }
    
    int result = luaL_loadstring(L, code.c_str());
    if (result != LUA_OK) {
        return checkResult(result);
    }
    
    result = lua_pcall(L, 0, LUA_MULTRET, 0);
    return checkResult(result);
}

LuaResult LuaEngine::executeFile(const std::string& filename) {
    if (!initialized) {
        return LuaResult("Lua engine not initialized");
    }
    
    // Use platform-specific file reading
    std::string code;
    if (!LuaFileSystem::readScriptFile(filename, code)) {
        return LuaResult("Could not read script file: " + filename);
    }
    
    // Execute the code
    LuaResult result = executeString(code);
    if (result.success) {
        loadedScripts.push_back(filename);
    }
    
    return result;
}

void LuaEngine::registerFunction(const std::string& name, LuaCallback /*callback*/) {
    if (!initialized) return;
    // LuaCallback overload is vestigial — all bindings use lua_CFunction.
    // Body intentionally left as no-op to prevent dangling-pointer UB.
    // The lightuserdata approach stored &local which was invalid after return.
}

void LuaEngine::registerFunction(const std::string& name, lua_CFunction func) {
    if (!initialized) return;
    
    lua_pushcfunction(L, func);
    lua_setglobal(L, name.c_str());
}

void LuaEngine::createTable(const std::string& name) {
    if (!initialized) return;
    
    lua_newtable(L);
    lua_setglobal(L, name.c_str());
}

void LuaEngine::setGlobal(const std::string& name, double value) {
    if (!initialized) return;
    
    lua_pushnumber(L, value);
    lua_setglobal(L, name.c_str());
}

void LuaEngine::setGlobal(const std::string& name, const std::string& value) {
    if (!initialized) return;
    
    lua_pushstring(L, value.c_str());
    lua_setglobal(L, name.c_str());
}

void LuaEngine::setGlobal(const std::string& name, bool value) {
    if (!initialized) return;
    
    lua_pushboolean(L, value ? 1 : 0);
    lua_setglobal(L, name.c_str());
}

double LuaEngine::getGlobalNumber(const std::string& name, double defaultValue) {
    if (!initialized) return defaultValue;
    
    lua_getglobal(L, name.c_str());
    if (!lua_isnumber(L, -1)) {
        lua_pop(L, 1);
        return defaultValue;
    }
    
    double value = lua_tonumber(L, -1);
    lua_pop(L, 1);
    return value;
}

std::string LuaEngine::getGlobalString(const std::string& name, const std::string& defaultValue) {
    if (!initialized) return defaultValue;
    
    lua_getglobal(L, name.c_str());
    if (!lua_isstring(L, -1)) {
        lua_pop(L, 1);
        return defaultValue;
    }
    
    const char* str = lua_tostring(L, -1);
    std::string value = str ? str : defaultValue;
    lua_pop(L, 1);
    return value;
}

bool LuaEngine::getGlobalBool(const std::string& name, bool defaultValue) {
    if (!initialized) return defaultValue;
    
    lua_getglobal(L, name.c_str());
    if (!lua_isboolean(L, -1)) {
        lua_pop(L, 1);
        return defaultValue;
    }
    
    bool value = lua_toboolean(L, -1) != 0;
    lua_pop(L, 1);
    return value;
}

size_t LuaEngine::getMemoryUsage() const {
    if (!L) return 0;
    return LuaPlatform::getMemoryUsage(L);
}

void LuaEngine::clearScripts() {
    loadedScripts.clear();
}

void* LuaEngine::luaAllocator(void* ud, void* ptr, size_t osize, size_t nsize) {
    LuaEngine* engine = static_cast<LuaEngine*>(ud);
    
    if (nsize == 0) {
        // Free memory
        if (ptr && osize > 0) {
            memoryUsed -= osize;
        }
        return nullptr;
    }
    
    if (ptr == nullptr) {
        // Allocate new memory
        if (memoryUsed + nsize > LuaPlatformConfig::MEMORY_LIMIT) {
            return nullptr; // Out of memory
        }
        
        void* newPtr = &memoryPool[memoryUsed];
        memoryUsed += nsize;
        return newPtr;
    } else {
        // Reallocate memory
        if (memoryUsed - osize + nsize > LuaPlatformConfig::MEMORY_LIMIT) {
            return nullptr; // Out of memory
        }
        
        if (nsize <= osize) {
            // Shrinking, no need to move
            memoryUsed = memoryUsed - osize + nsize;
            return ptr;
        } else {
            // Growing, need to move to end of pool
            void* newPtr = &memoryPool[memoryUsed];
            std::memcpy(newPtr, ptr, osize);
            memoryUsed = memoryUsed - osize + nsize;
            return newPtr;
        }
    }
}

int LuaEngine::luaPanic(lua_State* L) {
    const char* msg = lua_tostring(L, -1);
    std::cerr << "Lua panic: " << (msg ? msg : "unknown error") << std::endl;
    // In embedded systems, we might want to reset instead of aborting
    std::abort();
}

LuaResult LuaEngine::checkResult(int result) {
    if (result == LUA_OK) {
        return LuaResult();
    }
    
    const char* error = lua_tostring(L, -1);
    std::string errorMsg = error ? error : "Unknown Lua error";
    lua_pop(L, 1); // Remove error from stack
    
    return LuaResult(errorMsg);
}

// Template specializations for argument pushing
template<>
void LuaEngine::pushArg<double>(double&& arg) {
    lua_pushnumber(L, arg);
}

template<>
void LuaEngine::pushArg<float>(float&& arg) {
    lua_pushnumber(L, static_cast<double>(arg));
}

template<>
void LuaEngine::pushArg<int>(int&& arg) {
    lua_pushinteger(L, arg);
}

template<>
void LuaEngine::pushArg<bool>(bool&& arg) {
    lua_pushboolean(L, arg ? 1 : 0);
}

template<>
void LuaEngine::pushArg<std::string>(std::string&& arg) {
    lua_pushstring(L, arg.c_str());
}

template<>
void LuaEngine::pushArg<const char*>(const char*&& arg) {
    lua_pushstring(L, arg);
}


} // namespace enjin2