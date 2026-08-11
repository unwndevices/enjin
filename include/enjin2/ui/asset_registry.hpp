#pragma once

#include "../graphics/gfxfont.h"
#include <cstddef>
#include <cstdint>
#include <cstring>

/**
 * @file asset_registry.hpp
 * @brief Name ↔ pointer table for compiled-in assets (fonts, icon bitmaps)
 *
 * Widgets hold borrowed pointers to compiled-in assets (`const GFXfont*`,
 * icon bitmaps). Scene files are data, so those pointers serialize as **name
 * references** resolved through this registry — assets themselves stay
 * reference-only (unwn scene-editor spec, v1 cut line), and the compiled-in
 * set is enumerable for the editor's pickers.
 *
 * Fixed-capacity and allocation-free; names are borrowed `const char*` (string
 * literals) and must outlive the registry. The host (firmware, preview, editor
 * WASM) registers its compiled-in set once at startup — both sides of a scene
 * push must register the same names for a scene to resolve identically.
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

    /// @brief Resolve a bitmap name to its pixels (nullptr when unregistered).
    const uint8_t* findBitmap(const char* name) const {
        for (size_t i = 0; i < bitmapCount_; ++i)
            if (std::strcmp(bitmaps_[i].name, name) == 0) return bitmaps_[i].data;
        return nullptr;
    }

    /// @brief Reverse-lookup a bitmap pointer's name (nullptr when unregistered).
    const char* bitmapName(const uint8_t* data) const {
        for (size_t i = 0; i < bitmapCount_; ++i)
            if (bitmaps_[i].data == data) return bitmaps_[i].name;
        return nullptr;
    }

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
};

} // namespace enjin2
