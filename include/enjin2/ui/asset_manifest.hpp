#pragma once

#include "json.hpp"

#include <cstdint>
#include <string>
#include <vector>

/**
 * @file asset_manifest.hpp
 * @brief The scene's content-addressed asset manifest (unwn #204)
 *
 * A v2 scene carries an **asset manifest** — the list of assets it references
 * by content hash — not the asset bytes (those live in the content-addressed
 * store at `/assets/<hash>.njn`, ADR-0010). Each entry travels with its
 * dimensions so a loader can size-check (and reject over-budget) before the
 * bytes arrive, and a picker can validate a selection.
 *
 * The manifest serializes as a JSON array of
 * `{ hash, kind: "bitmap"|"sprite", w, h [, cols, rows] }`; it is one of the
 * scene document's tolerant DOM sections (like `state`/`timers`). This header
 * gives the structured view the player uses to load assets at activate.
 */

namespace enjin2 {

/// The byte source for content-addressed assets — the **only** per-target
/// difference in the asset path (ADR-0010). The registry shape and decode are
/// identical everywhere; the WASM editor reads in-memory authored bytes, the
/// native preview reads a local directory, the firmware reads LittleFS
/// `/assets/<hash>.njn`. A player with no source loads no owned assets.
struct AssetSource {
    virtual ~AssetSource() = default;
    /// Read the raw `.njn` bytes for @p hash into @p out.
    /// @return false when the asset is not resident (the player skips it).
    virtual bool read(const std::string& hash, std::vector<uint8_t>& out) = 0;
};

/// A manifest asset is either a single static bitmap or an animation sheet.
enum class AssetKind : uint8_t { Bitmap, Sprite };

inline AssetKind parseAssetKind(const std::string& s) {
    return s == "sprite" ? AssetKind::Sprite : AssetKind::Bitmap;
}

/// One manifest entry: a content hash plus the geometry needed to size-check
/// and decode the referenced `.njn` v2 asset. A static bitmap is cols=rows=1.
struct AssetManifestEntry {
    std::string hash;         ///< Content hash — the store key and registry name
    AssetKind kind = AssetKind::Bitmap;
    uint16_t w = 0;           ///< Cell width in pixels
    uint16_t h = 0;           ///< Cell height in pixels
    uint8_t cols = 1;         ///< Sheet grid columns (1 for a static bitmap)
    uint8_t rows = 1;         ///< Sheet grid rows (1 for a static bitmap)
};

/// @brief Structured, tolerant read of a manifest DOM array.
///
/// Skips entries without a non-empty `hash`; missing `cols`/`rows` default to
/// 1 (static bitmap). Everything else is best-effort — an unrecognised `kind`
/// reads as `bitmap`. Non-array input yields an empty list.
inline std::vector<AssetManifestEntry> parseAssetManifest(const JsonValue& v) {
    std::vector<AssetManifestEntry> out;
    if (v.type != JsonValue::Type::Array) return out;
    for (const JsonValue& item : v.array) {
        if (item.type != JsonValue::Type::Object) continue;
        AssetManifestEntry e;
        if (const JsonValue* h = item.find("hash")) {
            if (h->type == JsonValue::Type::String) e.hash = h->str;
        }
        if (e.hash.empty()) continue;
        if (const JsonValue* k = item.find("kind")) {
            if (k->type == JsonValue::Type::String) e.kind = parseAssetKind(k->str);
        }
        if (const JsonValue* w = item.find("w")) {
            if (w->type == JsonValue::Type::Number) e.w = static_cast<uint16_t>(w->number);
        }
        if (const JsonValue* hh = item.find("h")) {
            if (hh->type == JsonValue::Type::Number) e.h = static_cast<uint16_t>(hh->number);
        }
        if (const JsonValue* c = item.find("cols")) {
            if (c->type == JsonValue::Type::Number) e.cols = static_cast<uint8_t>(c->number);
        }
        if (const JsonValue* r = item.find("rows")) {
            if (r->type == JsonValue::Type::Number) e.rows = static_cast<uint8_t>(r->number);
        }
        out.push_back(std::move(e));
    }
    return out;
}

} // namespace enjin2
