#include "../../include/enjin2/scripting/bindings.hpp"
#include "../../include/enjin2/scripting/lua_platform.hpp"

#include <cstdio>

namespace enjin2 {

//==============================================================================
// Sprite Asset Loading (.njn files)
//==============================================================================

// engine.sprite.load(name) -> handle(0..15) or -1
// Resolves path: assetPath_ + "/" + name + ".njn"
// Validates header, copies pixels into fixed assetBuffer_, returns pool slot.
int LuaBindings::lua_loadSprite(lua_State* L) {
    LuaBindings* b = getBindings(L);
    if (!b) { lua_pushinteger(L, -1); return 1; }

    const char* name = luaL_checkstring(L, 1);
    if (!name) { lua_pushinteger(L, -1); return 1; }

    // Find first inactive slot
    int handle = -1;
    for (int i = 0; i < LUA_SPRITE_POOL_SIZE; ++i) {
        if (!b->spritePool[i].active) { handle = i; break; }
    }
    if (handle < 0) {
        printf("Sprite pool is full (max %d slots)\n", LUA_SPRITE_POOL_SIZE);
        lua_pushinteger(L, -1);
        return 1;
    }

    std::string path = b->assetPath_;
    if (!path.empty() && path.back() != '/') {
        path += '/';
    }
    path += name;
    path += ".njn";

    // Read .njn file using direct binary I/O (works on all platforms —
    // LuaFileSystem::readScriptFile only works under VCV_RACK or ESP32 defines)
    FILE* fp = fopen(path.c_str(), "rb");
    if (!fp) {
        printf("Failed to read sprite file: %s\n", path.c_str());
        lua_pushinteger(L, -1);
        return 1;
    }
    fseek(fp, 0, SEEK_END);
    long fileSize = ftell(fp);
    fseek(fp, 0, SEEK_SET);
    if (fileSize <= 0) {
        fclose(fp);
        printf("Empty sprite file: %s\n", path.c_str());
        lua_pushinteger(L, -1);
        return 1;
    }
    std::string fileContent(static_cast<size_t>(fileSize), '\0');
    size_t bytesRead = fread(&fileContent[0], 1, static_cast<size_t>(fileSize), fp);
    fclose(fp);
    if (bytesRead != static_cast<size_t>(fileSize)) {
        printf("Incomplete read of sprite file: %s\n", path.c_str());
        lua_pushinteger(L, -1);
        return 1;
    }

    NjnHeader header;
    if (!parseNjnHeader(reinterpret_cast<const uint8_t*>(fileContent.data()), fileContent.size(), header)) {
        printf("Invalid or corrupt .njn sprite asset: %s\n", path.c_str());
        lua_pushinteger(L, -1);
        return 1;
    }

    uint32_t pxSize = njnPixelDataSize(header);

    // Check if we have enough space in the asset buffer
    if (b->assetBufferUsed_ + pxSize > sizeof(b->assetBuffer_)) {
        printf("Sprite asset buffer full (needs %u bytes, %u free)\n",
                         pxSize, static_cast<uint32_t>(sizeof(b->assetBuffer_)) - b->assetBufferUsed_);
        lua_pushinteger(L, -1);
        return 1;
    }

    // Allocate from asset buffer
    uint8_t* destPixels = b->assetBuffer_ + b->assetBufferUsed_;
    memcpy(destPixels, fileContent.data() + sizeof(NjnHeader), pxSize);
    b->assetBufferUsed_ += pxSize;

    // Record asset metadata
    b->loadedAssets_[handle].header = header;
    b->loadedAssets_[handle].pixelData = destPixels;
    b->loadedAssets_[handle].pixelDataSize = pxSize;

    // Initialize SpritePool slot
    auto& s = b->spritePool[handle];
    s.sheet.data  = destPixels;
    s.sheet.cellW = header.cellW;
    s.sheet.cellH = header.cellH;
    s.sheet.cols  = header.cols;
    s.sheet.rows  = header.rows;
    s.fps      = 8.0f;
    s.accumSec = 0.0f;
    s.frame    = 0;
    s.mode    = AnimMode::Loop;
    s.forward = true;
    s.done    = false;
    s.active  = true;

    lua_pushinteger(L, handle);
    return 1;
}

// freeSprite(handle)
// Marks slot as inactive. If this was the most recently loaded asset, reclaims buffer space.
// If not, creates fragmentation (acceptable limitation for simple scope).
int LuaBindings::lua_freeSprite(lua_State* L) {
    LuaBindings* b = getBindings(L);
    if (!b) return 0;

    int handle = static_cast<int>(luaL_checkinteger(L, 1));
    if (handle < 0 || handle >= LUA_SPRITE_POOL_SIZE || !b->spritePool[handle].active) return 0;

    auto& s = b->spritePool[handle];
    auto& asset = b->loadedAssets_[handle];

    // If this asset was loaded from .njn and happens to be at the very tip of the arena buffer,
    // we can reclaim its memory. This supports simple free/reload loops.
    // If it's buried in the middle, the memory remains "leaked" until resetSpritePool() is called.
    if (asset.pixelDataSize > 0 &&
        asset.pixelData + asset.pixelDataSize == b->assetBuffer_ + b->assetBufferUsed_) {
        b->assetBufferUsed_ -= asset.pixelDataSize;
    }

    // Clear slot
    s.active = false;
    asset = SpriteAsset{}; // zero out metadata

    return 0;
}

} // namespace enjin2
