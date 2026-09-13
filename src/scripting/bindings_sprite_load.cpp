#include "../../include/enjin2/scripting/bindings.hpp"
#include "../../include/enjin2/scripting/lua_platform.hpp"

#include <cstdio>

namespace enjin2 {

//==============================================================================
// Sprite Asset Loading (.njn files)
//==============================================================================

// Shared loader for both engine.sprite.load and engine.tilemap.load (#91).
// Reads a .njn — v1 flat 8-byte header OR v2 typed-chunk container — copies its
// pixel bytes into the fixed asset arena, fills the target slot's SpriteSheet
// plus loadedAssets_/loadedClips_ metadata, and decodes the v2 ATTR table into
// outAttrs. Leaves the pool slot's active flag and animation state to the
// caller. Returns false (and prints a diagnostic) on any I/O or format error.
bool LuaBindings::loadNjnAsset(const std::string& path, int handle,
                              std::vector<TileAttr>& outAttrs) {
    outAttrs.clear();
    loadedClips_[handle].clear();

    // Read the file with direct binary I/O (works on all platforms —
    // LuaFileSystem::readScriptFile only works under VCV_RACK or ESP32 defines).
    FILE* fp = fopen(path.c_str(), "rb");
    if (!fp) {
        printf("Failed to read .njn file: %s\n", path.c_str());
        return false;
    }
    fseek(fp, 0, SEEK_END);
    long fileSize = ftell(fp);
    fseek(fp, 0, SEEK_SET);
    if (fileSize <= 0) {
        fclose(fp);
        printf("Empty .njn file: %s\n", path.c_str());
        return false;
    }
    std::string fileContent(static_cast<size_t>(fileSize), '\0');
    size_t bytesRead = fread(&fileContent[0], 1, static_cast<size_t>(fileSize), fp);
    fclose(fp);
    if (bytesRead != static_cast<size_t>(fileSize)) {
        printf("Incomplete read of .njn file: %s\n", path.c_str());
        return false;
    }

    const uint8_t* raw = reinterpret_cast<const uint8_t*>(fileContent.data());
    const size_t rawSize = fileContent.size();

    // Both v1 and v2 share the 'N','J' magic; byte 2 is the version discriminator.
    uint8_t cellW = 0, cellH = 0, cols = 0, rows = 0;
    const uint8_t* srcPixels = nullptr;
    uint32_t pxSize = 0;

    if (rawSize >= 3 && raw[0] == NJN2_MAGIC_0 && raw[1] == NJN2_MAGIC_1 &&
        raw[2] == NJN2_VERSION) {
        // ---- v2 typed-chunk container (#76): META + PIXL, optional CLIP/ATTR ----
        NjnV2Reader reader;
        if (!reader.open(raw, rawSize)) {
            printf("Invalid or corrupt .njn v2 container: %s\n", path.c_str());
            return false;
        }
        if (!njn2DecodeMeta(reader.find(NJN2_CHUNK_META), cellW, cellH, cols, rows) ||
            cellW == 0 || cellH == 0 || cols == 0 || rows == 0) {
            printf(".njn v2 missing/invalid META chunk: %s\n", path.c_str());
            return false;
        }
        const NjnV2Chunk* pixl = reader.find(NJN2_CHUNK_PIXL);
        const uint32_t expected =
            static_cast<uint32_t>(cellW) * cellH * cols * rows;
        if (!pixl || pixl->size < expected) {
            printf(".njn v2 missing/short PIXL chunk: %s\n", path.c_str());
            return false;
        }
        srcPixels = pixl->data;
        pxSize = expected;

        // CLIP (#55) and ATTR (#51) are optional — absence is not an error.
        njn2DecodeClip(reader.find(NJN2_CHUNK_CLIP), loadedClips_[handle]);
        njn2DecodeAttr(reader.find(NJN2_CHUNK_ATTR), outAttrs);
    } else {
        // ---- v1 flat 8-byte header (legacy embedded sheets) ----
        NjnHeader header;
        if (!parseNjnHeader(raw, rawSize, header)) {
            printf("Invalid or corrupt .njn sprite asset: %s\n", path.c_str());
            return false;
        }
        cellW = header.cellW;
        cellH = header.cellH;
        cols  = header.cols;
        rows  = header.rows;
        srcPixels = raw + sizeof(NjnHeader);
        pxSize = njnPixelDataSize(header);
    }

    // Copy pixels into the fixed asset arena (bump allocator).
    if (assetBufferUsed_ + pxSize > sizeof(assetBuffer_)) {
        printf(".njn asset buffer full (needs %u bytes, %u free): %s\n",
               pxSize, static_cast<uint32_t>(sizeof(assetBuffer_)) - assetBufferUsed_,
               path.c_str());
        loadedClips_[handle].clear();
        outAttrs.clear();
        return false;
    }
    uint8_t* destPixels = assetBuffer_ + assetBufferUsed_;
    memcpy(destPixels, srcPixels, pxSize);
    assetBufferUsed_ += pxSize;

    // Record asset metadata (header mirrors the resolved geometry so freeSprite's
    // tip-reclaim and any v1 consumers see a consistent NjnHeader).
    NjnHeader meta{};
    meta.magic[0] = NJN_MAGIC_0;
    meta.magic[1] = NJN_MAGIC_1;
    meta.version  = NJN_VERSION;
    meta.cellW = cellW;
    meta.cellH = cellH;
    meta.cols  = cols;
    meta.rows  = rows;
    loadedAssets_[handle].header = meta;
    loadedAssets_[handle].pixelData = destPixels;
    loadedAssets_[handle].pixelDataSize = pxSize;

    // Fill the pool slot's sheet (animation state left to the caller).
    auto& s = spritePool[handle];
    s.sheet.data  = destPixels;
    s.sheet.cellW = cellW;
    s.sheet.cellH = cellH;
    s.sheet.cols  = cols;
    s.sheet.rows  = rows;
    return true;
}

// engine.sprite.load(name) -> handle(0..15) or -1
// Resolves path: assetPath_ + "/" + name + ".njn". Accepts v1 flat headers and
// v2 typed-chunk containers (with CLIP + ATTR); pixels land in assetBuffer_.
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

    std::vector<TileAttr> attrs;  // sprite sheets ignore ATTR
    if (!b->loadNjnAsset(path, handle, attrs)) {
        lua_pushinteger(L, -1);
        return 1;
    }

    // Initialize SpritePool animation state (loadNjnAsset filled the sheet).
    auto& s = b->spritePool[handle];
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
