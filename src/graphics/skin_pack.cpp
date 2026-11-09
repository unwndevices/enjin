#include "enjin2/graphics/skin_pack.hpp"
#include "enjin2/scripting/table_file.hpp"

extern "C" {
#include "lua.h"
}

#include <cstdio>
#include <cstring>

namespace enjin2 {
namespace {

void fail(std::string* error, const char* msg) {
    if (error) *error = msg;
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

// Copy a string `face` into the pack; anything else (a number included, which
// lua_tostring would coerce) names no face.
void readFace(lua_State* L, SkinPack& out) {
    lua_getfield(L, -1, "face");
    if (lua_type(L, -1) == LUA_TSTRING) copyName(out.face, lua_tostring(L, -1));
    lua_pop(L, 1);
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
    if (!readTableFileText(folder + "/skin.lua", 65536, source, nullptr)) {
        fail(error, "missing or unreadable skin.lua");
        return false;
    }

    // A `palette` field (skins made before Tomodachi #417) is ignored: skins
    // carry no colours. readFace/readAssets/readNineSlice read the table at
    // the top.
    return readTableFile(
        source, "skin.lua",
        [&](lua_State* L, int, std::string*) {
            readFace(L, out);
            readAssets(L, folder, store, out);
            readNineSlice(L, out);
            return true;
        },
        error);
}

} // namespace enjin2
