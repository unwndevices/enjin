#pragma once

#include <cstdint>
#include <string>

#include "enjin2/graphics/layered_asset.hpp"
#include "enjin2/graphics/palette.hpp"

namespace enjin2 {

/**
 * @brief A generic, engine-level skin pack: a luminance-ordered palette plus a
 *        set of named `.njn` assets loaded into a `LayeredAssetStore`'s arena.
 *
 * This is the reusable half of the skin seam (#154). It knows *how* to parse a
 * `skin.lua` folder pack and load its parts, but nothing about what any name
 * *means* — the player-private vocabulary (which name is the tileset, which is
 * the controls sheet, the `SkinIcon` CLIP enum, the resolved `Skin` struct, and
 * the per-asset `RomDefaultSkin` fallback) lives entirely in the consumer.
 *
 * Only the palette is mandatory; every other part is optional and simply absent
 * when omitted, so a caller can fall back per-asset.
 */
struct SkinPack {
    /// Named-asset capacity. A skin holds a handful of sheets (tileset, border,
    /// controls, cover fallback, room to grow).
    static constexpr int MAX_ASSETS = 8;
    /// Inclusive cap on a skin-asset name (bytes, NUL-terminated).
    static constexpr int NAME_CAP = 24;

    /// One loaded, named part of the pack.
    struct Asset {
        char name[NAME_CAP];              ///< Author's key, e.g. "controls".
        LayeredAssetStore::Handle handle; ///< Live handle into the store.
        int16_t cornerW;                  ///< nineSlice corner width (0 = none).
        int16_t cornerH;                  ///< nineSlice corner height (0 = none).
    };

    Palette palette;               ///< Filled from the mandatory 15-hex table.
    Asset assets[MAX_ASSETS] = {}; ///< Present parts, in manifest-encounter order.
    int numAssets = 0;             ///< Count of live entries in @ref assets.

    /// Handle of the named asset, or `INVALID_HANDLE` if the pack omits it.
    LayeredAssetStore::Handle handleByName(const char* name) const;

    /// nineSlice corner of the named asset; false if absent or no corner set.
    bool cornerByName(const char* name, int16_t& w, int16_t& h) const;
};

/**
 * @brief Parse `<folder>/skin.lua` and load its assets into @p store.
 *
 * The manifest is a sandboxed "return-a-table" Lua chunk (empty environment,
 * text-only, instruction-limited), mirroring the applet `manifest.lua` reader:
 *
 * @code
 * return {
 *   palette = { "#0f0f1b", ... },          -- mandatory, luminance-ordered
 *   assets  = { tileset = "tileset", border = "border", ... },  -- optional
 *   nineslice = { border = { 4, 4 } },     -- optional {cornerW, cornerH}
 * }
 * @endcode
 *
 * The `palette` table is mandatory (1..15 hex entries, extras ignored). Every
 * `assets` entry names a `.njn` basename resolved against @p folder; a present,
 * loadable asset is registered, a missing or unloadable one is simply left out
 * (per-asset fallback is the caller's policy). `nineslice[name] = {w, h}` sets
 * the corner on the matching asset.
 *
 * @return false (with @p error set) only when the manifest is missing or
 *         unparseable, or the mandatory palette is absent/empty.
 */
bool loadSkinPack(const std::string& folder, LayeredAssetStore& store,
                  SkinPack& out, std::string* error = nullptr);

/**
 * @brief Copy a pack's palette colours + size into @p dst.
 *
 * The live-switch palette step (#147): overwrite the destination palette from
 * the pack. The caller rebuilds any RGB565 LUT and redraws — covers and sprites
 * store indices, so they recolour for free.
 */
void applySkinPalette(const SkinPack& pack, Palette& dst);

} // namespace enjin2
