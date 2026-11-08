#pragma once

#include <cstdint>
#include <string>

#include "enjin2/graphics/layered_asset.hpp"

namespace enjin2 {

/**
 * @brief A generic, engine-level skin pack: a set of named `.njn` assets
 *        loaded into a `LayeredAssetStore`'s arena. No colours: the palette is
 *        the system's (Tomodachi #417).
 *
 * This is the reusable half of the skin seam (#154). It knows *how* to parse a
 * `skin.lua` folder pack and load its parts, but nothing about what any name
 * *means* — the player-private vocabulary (which name is the tileset, which is
 * the controls sheet, the `SkinIcon` CLIP enum, the resolved `Skin` struct, and
 * the per-asset `RomDefaultSkin` fallback) lives entirely in the consumer.
 *
 * Every part is optional and simply absent when omitted, so a caller can fall
 * back per-asset.
 */
struct SkinPack {
    /// Named-asset capacity. A skin holds a handful of sheets (e.g. tileset,
    /// border, controls), with room to grow.
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

    Asset assets[MAX_ASSETS] = {}; ///< Present parts, in manifest-encounter order.
    int numAssets = 0;             ///< Count of live entries in @ref assets.
    /// The layout the pack names (`face = "..."`), "" when it names none. What
    /// an id means is the consumer's (Tomodachi's player Faces, ADR-0026).
    char face[NAME_CAP] = {};

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
 *   face    = "tapedeck",                  -- optional layout id
 *   assets  = { tileset = "tileset", border = "border", ... },  -- optional
 *   nineslice = { border = { 4, 4 } },     -- optional {cornerW, cornerH}
 * }
 * @endcode
 *
 * A skin carries no colours (Tomodachi #417, ADR-0022: one global palette); a
 * `palette` field left in an old manifest is ignored. Every `assets` entry
 * names a `.njn` basename resolved against @p folder; a present, loadable asset is registered, a missing or unloadable one is simply left out
 * (per-asset fallback is the caller's policy). `nineslice[name] = {w, h}` sets
 * the corner on the matching asset. `face` is copied into `SkinPack::face`
 * when it is a string (cut to `NAME_CAP - 1` bytes like an asset name); any
 * other value names no face.
 *
 * @return false (with @p error set) only when the manifest is missing or
 *         unparseable.
 */
bool loadSkinPack(const std::string& folder, LayeredAssetStore& store,
                  SkinPack& out, std::string* error = nullptr);

} // namespace enjin2
