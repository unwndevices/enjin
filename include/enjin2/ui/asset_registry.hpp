#pragma once

#include "../graphics/gfxfont.h"
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <memory>
#include <string>
#include <vector>

/**
 * @file asset_registry.hpp
 * @brief Name ↔ pointer table for compiled-in + scene-owned assets
 *
 * Widgets hold borrowed pointers to assets (`const GFXfont*`, icon bitmaps).
 * Scene files are data, so those pointers serialize as **name references**
 * resolved through this registry — the compiled-in set is enumerable for the
 * editor's pickers.
 *
 * The registry holds **two entry kinds** (unwn #204, ADR-0010):
 *
 * - **Borrowed** compiled-in fonts and default bitmaps: fixed-capacity,
 *   allocation-free, names are `const char*` string literals that must outlive
 *   the registry. Registered once at startup on every target.
 * - **Owned** heap-backed bitmaps whose lifetime is the active scene: loaded
 *   from the content-addressed store at `scene.activate` and freed on teardown
 *   (`clearOwned`). The registry owns both the decoded pixels and the hash
 *   name. `findBitmap` resolves owned entries first, then borrowed — so a
 *   scene's manifest assets and the compiled-in set share one lookup.
 *
 * Both sides of a scene push must register the same names for a scene to
 * resolve identically; content-addressed owned assets satisfy this by
 * construction (the hash is the name).
 */

namespace enjin2 {

/// @brief Fixed-capacity name↔pointer table for compiled-in fonts and bitmaps.
class AssetRegistry {
public:
    static constexpr size_t kMaxFonts = 16;
    static constexpr size_t kMaxBitmaps = 32;

    struct FontEntry {
        const char* name;
        const GFXfont* font;
    };
    struct BitmapEntry {
        const char* name;
        const uint8_t* data;
    };

    /// A scene-owned bitmap: the registry owns the hash name and the decoded
    /// byte-per-pixel buffer (one byte per pixel; transparent pixels carry the
    /// icon transparent sentinel). Lifetime = the active scene.
    struct OwnedBitmap {
        std::string name;              ///< Content hash — the store key and reference
        std::vector<uint8_t> pixels;   ///< Decoded pixels (owned)
        uint16_t w = 0;                ///< Cell width in pixels
        uint16_t h = 0;                ///< Cell height in pixels
    };

    /**
     * @brief Register a compiled-in font under a stable name
     * @return false when full or the name is already taken
     */
    bool registerFont(const char* name, const GFXfont* font) {
        if (!name || !font || fontCount_ >= kMaxFonts || findFont(name)) return false;
        fonts_[fontCount_++] = FontEntry{name, font};
        return true;
    }

    /**
     * @brief Register a compiled-in icon bitmap under a stable name
     * @return false when full or the name is already taken
     */
    bool registerBitmap(const char* name, const uint8_t* data) {
        if (!name || !data || bitmapCount_ >= kMaxBitmaps || findBitmap(name)) return false;
        bitmaps_[bitmapCount_++] = BitmapEntry{name, data};
        return true;
    }

    /// @brief Resolve a font name to its pointer (nullptr when unregistered).
    const GFXfont* findFont(const char* name) const {
        for (size_t i = 0; i < fontCount_; ++i)
            if (std::strcmp(fonts_[i].name, name) == 0) return fonts_[i].font;
        return nullptr;
    }

    /// @brief Reverse-lookup a font pointer's name (nullptr when unregistered).
    const char* fontName(const GFXfont* font) const {
        for (size_t i = 0; i < fontCount_; ++i)
            if (fonts_[i].font == font) return fonts_[i].name;
        return nullptr;
    }

    /**
     * @brief Register a scene-owned bitmap: the registry copies @p data and
     *        owns it until clearOwned() (unwn #204).
     * @return false when the name is empty/duplicate or the data is invalid
     */
    bool registerOwnedBitmap(const char* name, const uint8_t* data, size_t len,
                             uint16_t w, uint16_t h) {
        if (!name || !name[0] || !data || len == 0) return false;
        if (findBitmap(name)) return false; // no shadowing of an existing name
        auto entry = std::make_unique<OwnedBitmap>();
        entry->name.assign(name);
        entry->pixels.assign(data, data + len);
        entry->w = w;
        entry->h = h;
        owned_.push_back(std::move(entry));
        return true;
    }

    /// @brief Free every scene-owned bitmap (scene teardown). Borrowed
    /// compiled-in entries are untouched.
    void clearOwned() { owned_.clear(); }

    /// @brief Resolve a bitmap name to its pixels (nullptr when unregistered).
    /// Owned scene assets resolve first, then borrowed compiled-in bitmaps.
    const uint8_t* findBitmap(const char* name) const {
        if (!name) return nullptr;
        for (const auto& e : owned_)
            if (e->name == name) return e->pixels.data();
        for (size_t i = 0; i < bitmapCount_; ++i)
            if (std::strcmp(bitmaps_[i].name, name) == 0) return bitmaps_[i].data;
        return nullptr;
    }

    /// @brief Resolve a name to its owned entry (dimensions + pixels), or
    /// nullptr when it is not an owned bitmap.
    const OwnedBitmap* findOwnedBitmap(const char* name) const {
        if (!name) return nullptr;
        for (const auto& e : owned_)
            if (e->name == name) return e.get();
        return nullptr;
    }

    /// @brief Reverse-lookup a bitmap pointer's name (nullptr when unregistered).
    const char* bitmapName(const uint8_t* data) const {
        for (const auto& e : owned_)
            if (e->pixels.data() == data) return e->name.c_str();
        for (size_t i = 0; i < bitmapCount_; ++i)
            if (bitmaps_[i].data == data) return bitmaps_[i].name;
        return nullptr;
    }

    size_t ownedBitmapCount() const { return owned_.size(); }

    // ----- Enumeration (editor pickers) -----

    size_t fontCount() const { return fontCount_; }
    const FontEntry& fontAt(size_t i) const { return fonts_[i]; }
    size_t bitmapCount() const { return bitmapCount_; }
    const BitmapEntry& bitmapAt(size_t i) const { return bitmaps_[i]; }

private:
    FontEntry fonts_[kMaxFonts] = {};
    BitmapEntry bitmaps_[kMaxBitmaps] = {};
    size_t fontCount_ = 0;
    size_t bitmapCount_ = 0;
    // Heap-backed scene-owned bitmaps. unique_ptr keeps each buffer's address
    // stable as the vector grows, so a borrowed pixel pointer never dangles.
    std::vector<std::unique_ptr<OwnedBitmap>> owned_;
};

} // namespace enjin2
