#include "enjin2/graphics/skin_pack.hpp"

extern "C" {
#include "lauxlib.h"
#include "lua.h"
#include "lualib.h"
}

#include <cstdio>
#include <cstring>
#include <sys/stat.h>

namespace enjin2 {
namespace {

// Sandbox mirror of the applet manifest reader: load the manifest text with an
// empty environment (text-only, no bytecode), run it, and require a table back.
constexpr const char* kSkinManifestReader =
    "local s = ...\n"
    "local chunk = assert(load(s, 'skin.lua', 't', {}))\n"
    "local t = chunk()\n"
    "assert(type(t) == 'table', 'skin.lua must return a table')\n"
    "return t\n";

// Same acceptance window as the applet catalog: a regular, non-empty, NUL-free
// file within `limit` bytes.
bool readText(const std::string& path, size_t limit, std::string& out) {
    out.clear();
    struct stat st {};
    if (stat(path.c_str(), &st) || !S_ISREG(st.st_mode) || st.st_size <= 0 ||
        static_cast<size_t>(st.st_size) > limit)
        return false;
    FILE* file = std::fopen(path.c_str(), "rb");
    if (!file) return false;
    out.resize(static_cast<size_t>(st.st_size));
    const bool ok = std::fread(out.data(), 1, out.size(), file) == out.size() &&
                    out.find('\0') == std::string::npos;
    std::fclose(file);
    if (!ok) out.clear();
    return ok;
}

void instructionLimit(lua_State* L, lua_Debug*) {
    luaL_error(L, "skin.lua instruction limit exceeded");
}

void fail(std::string* error, const char* msg) {
    if (error) *error = msg;
}

// Read the mandatory `palette` array (field at stack top) into `out`. Returns
// the number of colours read (0 = missing/empty/invalid → mandatory failure).
int readPalette(lua_State* L, Palette& out) {
    lua_getfield(L, -1, "palette");
    if (!lua_istable(L, -1)) {
        lua_pop(L, 1);
        return 0;
    }
    int count = 0;
    for (int i = 1; i <= PALETTE_MAX_ENTRIES; ++i) {
        lua_rawgeti(L, -1, i);
        const char* hex = lua_tostring(L, -1);
        uint8_t r, g, b;
        if (!hex || !parseHexColor(hex, r, g, b)) {
            lua_pop(L, 1);
            break;
        }
        out.colors[count] = RGB(r, g, b);
        ++count;
        lua_pop(L, 1);
    }
    lua_pop(L, 1); // palette table
    out.size = static_cast<uint8_t>(count);
    out.debugTransparent = false;
    return count;
}

void copyName(char (&dst)[SkinPack::NAME_CAP], const char* src) {
    std::snprintf(dst, SkinPack::NAME_CAP, "%s", src ? src : "");
}

// Load every `assets` entry (name → .njn basename) into the store. Missing or
// unloadable assets are silently skipped (per-asset fallback is the caller's).
void readAssets(lua_State* L, const std::string& folder,
                LayeredAssetStore& store, SkinPack& out) {
    lua_getfield(L, -1, "assets");
    if (!lua_istable(L, -1)) {
        lua_pop(L, 1);
        return;
    }
    lua_pushnil(L);
    while (lua_next(L, -2) != 0) {
        // key at -2, value at -1
        const char* name = lua_tostring(L, -2);
        const char* basename = lua_tostring(L, -1);
        if (name && basename && out.numAssets < SkinPack::MAX_ASSETS) {
            const std::string path = folder + "/" + basename + ".njn";
            LayeredAssetStore::Handle h = store.load(path);
            if (h != LayeredAssetStore::INVALID_HANDLE) {
                SkinPack::Asset& a = out.assets[out.numAssets++];
                copyName(a.name, name);
                a.handle = h;
                a.cornerW = 0;
                a.cornerH = 0;
            }
        }
        lua_pop(L, 1); // value; keep key for lua_next
    }
    lua_pop(L, 1); // assets table
}

// Attach `nineslice[name] = {w, h}` corners onto already-loaded assets.
void readNineSlice(lua_State* L, SkinPack& out) {
    lua_getfield(L, -1, "nineslice");
    if (!lua_istable(L, -1)) {
        lua_pop(L, 1);
        return;
    }
    lua_pushnil(L);
    while (lua_next(L, -2) != 0) {
        const char* name = lua_tostring(L, -2);
        if (name && lua_istable(L, -1)) {
            lua_rawgeti(L, -1, 1);
            const int w = static_cast<int>(lua_tointeger(L, -1));
            lua_pop(L, 1);
            lua_rawgeti(L, -1, 2);
            const int h = static_cast<int>(lua_tointeger(L, -1));
            lua_pop(L, 1);
            for (int i = 0; i < out.numAssets; ++i) {
                if (std::strcmp(out.assets[i].name, name) == 0) {
                    out.assets[i].cornerW = static_cast<int16_t>(w);
                    out.assets[i].cornerH = static_cast<int16_t>(h);
                    break;
                }
            }
        }
        lua_pop(L, 1); // value; keep key
    }
    lua_pop(L, 1); // nineslice table
}

} // namespace

LayeredAssetStore::Handle SkinPack::handleByName(const char* name) const {
    if (!name) return LayeredAssetStore::INVALID_HANDLE;
    for (int i = 0; i < numAssets; ++i) {
        if (std::strcmp(assets[i].name, name) == 0) return assets[i].handle;
    }
    return LayeredAssetStore::INVALID_HANDLE;
}

bool SkinPack::cornerByName(const char* name, int16_t& w, int16_t& h) const {
    if (!name) return false;
    for (int i = 0; i < numAssets; ++i) {
        if (std::strcmp(assets[i].name, name) == 0) {
            if (assets[i].cornerW == 0 && assets[i].cornerH == 0) return false;
            w = assets[i].cornerW;
            h = assets[i].cornerH;
            return true;
        }
    }
    return false;
}

bool loadSkinPack(const std::string& folder, LayeredAssetStore& store,
                  SkinPack& out, std::string* error) {
    out = SkinPack{};

    std::string source;
    if (!readText(folder + "/skin.lua", 65536, source)) {
        fail(error, "missing or unreadable skin.lua");
        return false;
    }

    lua_State* L = luaL_newstate();
    if (!L) {
        fail(error, "cannot allocate skin Lua state");
        return false;
    }
    luaL_openlibs(L);
    lua_sethook(L, instructionLimit, LUA_MASKCOUNT, 100000);
    int status = luaL_loadstring(L, kSkinManifestReader);
    if (status == LUA_OK) {
        lua_pushlstring(L, source.data(), source.size());
        status = lua_pcall(L, 1, 1, 0);
    }
    lua_sethook(L, nullptr, 0, 0);
    if (status != LUA_OK) {
        if (error) *error = lua_tostring(L, -1) ? lua_tostring(L, -1)
                                                : "skin.lua parse error";
        lua_close(L);
        return false;
    }

    // Manifest table now sits at the stack top.
    if (readPalette(L, out.palette) == 0) {
        lua_close(L);
        out = SkinPack{};
        fail(error, "skin.lua palette is mandatory and must list hex colours");
        return false;
    }
    readAssets(L, folder, store, out);
    readNineSlice(L, out);

    lua_close(L);
    return true;
}

void applySkinPalette(const SkinPack& pack, Palette& dst) {
    for (int i = 0; i < PALETTE_MAX_ENTRIES; ++i) dst.colors[i] = pack.palette.colors[i];
    dst.size = pack.palette.size ? pack.palette.size : PALETTE_MAX_ENTRIES;
    dst.debugTransparent = false;
}

} // namespace enjin2
