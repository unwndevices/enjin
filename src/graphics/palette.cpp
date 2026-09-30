#include "../../include/enjin2/graphics/palette.hpp"
#include "../../include/enjin2/core/name_index.hpp"
#include <cstring>
#include <cstdio>

namespace enjin2 {

// ============================================================
// Default (system) palette: Tomodachi's tape-deck green ramp (its music
// player's template skin), 15 opaque colors from darkest (0) to lightest (14).
// Index 15 is transparent. This is the one system theme every screen boots
// with.
// ============================================================
static constexpr RGB DEFAULT_COLORS[15] = {
    {0x08, 0x1a, 0x1c}, // 0  — darkest
    {0x0c, 0x28, 0x28}, // 1
    {0x11, 0x35, 0x33}, // 2
    {0x16, 0x46, 0x32}, // 3
    {0x1a, 0x57, 0x31}, // 4
    {0x21, 0x6d, 0x2a}, // 5
    {0x27, 0x83, 0x22}, // 6
    {0x3d, 0x96, 0x28}, // 7
    {0x52, 0xa8, 0x2e}, // 8
    {0x6b, 0xb9, 0x35}, // 9
    {0x83, 0xca, 0x3c}, // 10
    {0x9b, 0xd2, 0x50}, // 11
    {0xb2, 0xda, 0x63}, // 12
    {0xce, 0xe8, 0x96}, // 13
    {0xe8, 0xf4, 0xcc}, // 14 — lightest
};

// ============================================================
// tomo preset: the previous default, the authored `tomo_tune2` Aseprite
// palette (tools/testdata/tomo_tune2.gpl), ordered by hue. Kept as a named
// preset so hue-indexed art (the tomo_tune2 reference asset) still resolves.
// ============================================================
static constexpr RGB TOMO_COLORS[15] = {
    {0x00, 0x00, 0x00}, // 0  — black
    {0x89, 0x89, 0x89}, // 1  — grey
    {0xff, 0xff, 0xff}, // 2  — white
    {0x1a, 0x1c, 0x2c}, // 3  — navy
    {0x5d, 0x27, 0x5d}, // 4  — purple
    {0xb1, 0x3e, 0x53}, // 5  — red
    {0xef, 0x7d, 0x57}, // 6  — orange
    {0xff, 0xcd, 0x75}, // 7  — yellow
    {0xa7, 0xf0, 0x70}, // 8  — light green
    {0x38, 0xb7, 0x64}, // 9  — green
    {0x25, 0x71, 0x79}, // 10 — teal
    {0x3b, 0x5d, 0xc9}, // 11 — blue
    {0x73, 0xef, 0xf7}, // 12 — cyan
    {0x56, 0x6c, 0x86}, // 13 — slate
    {0x33, 0x3c, 0x57}, // 14 — dark slate
};

// ============================================================
// PICO-8 preset: the previous default (PICO-8 minus #94b0c2), 15 colors.
// Kept as a named preset so the old look is still selectable.
// ============================================================
static constexpr RGB PICO8_COLORS[15] = {
    {0x1a, 0x1c, 0x2c}, // 0  — dark navy
    {0x5d, 0x27, 0x5d}, // 1  — dark purple
    {0xb1, 0x3e, 0x53}, // 2  — dark red
    {0xef, 0x7d, 0x57}, // 3  — orange
    {0xff, 0xcd, 0x75}, // 4  — yellow
    {0xa7, 0xf0, 0x70}, // 5  — light green
    {0x38, 0xb7, 0x64}, // 6  — green
    {0x25, 0x71, 0x79}, // 7  — dark teal
    {0x29, 0x36, 0x6f}, // 8  — dark blue
    {0x3b, 0x5d, 0xc9}, // 9  — blue
    {0x41, 0xa6, 0xf6}, // 10 — light blue
    {0x73, 0xef, 0xf7}, // 11 — cyan
    {0xf4, 0xf4, 0xf4}, // 12 — near-white
    {0x56, 0x6c, 0x86}, // 13 — slate blue-grey
    {0x33, 0x3c, 0x57}, // 14 — dark slate
};

// ============================================================
// Gameboy preset: 4 shades of green
// ============================================================
static constexpr RGB GAMEBOY_COLORS[4] = {
    {0x0f, 0x38, 0x0f}, // 0 — darkest green
    {0x30, 0x62, 0x30}, // 1 — dark green
    {0x8b, 0xac, 0x0f}, // 2 — light green
    {0x9b, 0xbc, 0x0f}, // 3 — lightest green
};

// ============================================================
// Preset table
// ============================================================
struct PalettePreset {
    const RGB*  colors;
    uint8_t     size;
};

// Indexed like kPalettePresetNames.
static const PalettePreset PRESETS[] = {
    {DEFAULT_COLORS, 15},  // "default"
    {TOMO_COLORS,    15},  // "tomo"
    {PICO8_COLORS,   15},  // "pico8"
    {GAMEBOY_COLORS,  4},  // "gameboy"
};

static constexpr int PRESET_COUNT = static_cast<int>(sizeof(PRESETS) / sizeof(PRESETS[0]));
static_assert(PRESET_COUNT == sizeof(kPalettePresetNames) / sizeof(kPalettePresetNames[0]),
              "one preset per kPalettePresetNames entry");

// ============================================================
// Palette methods
// ============================================================

Palette::Palette()
    : size(15)
    , debugTransparent(false)
{
    for (uint8_t i = 0; i < PALETTE_MAX_ENTRIES; ++i) {
        colors[i] = DEFAULT_COLORS[i];
    }
}

void Palette::setColor(uint8_t index, uint8_t r, uint8_t g, uint8_t b)
{
    // Transparency check BEFORE modulo wrapping
    if (index == PALETTE_TRANSPARENT) {
        return; // Index 15 is always transparent — silently ignore
    }
    uint8_t wrapped = index % size;
    colors[wrapped] = RGB{r, g, b};
}

RGB Palette::getColor(uint8_t index) const
{
    // Transparency check BEFORE modulo wrapping
    if (index == PALETTE_TRANSPARENT) {
        return RGB{0, 0, 0};
    }
    uint8_t wrapped = index % size;
    return colors[wrapped];
}

RGB Palette::resolve(uint8_t index) const
{
    // Caller must have checked isTransparent() first.
    // Transparency check BEFORE modulo wrapping (guard for direct callers).
    if (index == PALETTE_TRANSPARENT) {
        return RGB{0, 0, 0};
    }
    uint8_t wrapped = index % size;
    return colors[wrapped];
}

bool Palette::isTransparent(uint8_t index) const
{
    return index == PALETTE_TRANSPARENT;
}

bool Palette::loadPreset(const char* name)
{
    const int i = nameIndex(name, kPalettePresetNames);
    if (i < 0) {
        return false;
    }
    size = PRESETS[i].size;
    for (uint8_t j = 0; j < size && j < PALETTE_MAX_ENTRIES; ++j) {
        colors[j] = PRESETS[i].colors[j];
    }
    return true;
}

uint8_t Palette::getSize() const
{
    return size;
}

// ============================================================
// Free function: parseHexColor
// ============================================================

bool parseHexColor(const char* hex, uint8_t& r, uint8_t& g, uint8_t& b)
{
    if (!hex) {
        return false;
    }
    const char* start = hex;
    if (*start == '#') {
        ++start;
    }
    unsigned int ri = 0, gi = 0, bi = 0;
    int parsed = sscanf(start, "%02x%02x%02x", &ri, &gi, &bi);
    if (parsed != 3) {
        return false;
    }
    r = static_cast<uint8_t>(ri);
    g = static_cast<uint8_t>(gi);
    b = static_cast<uint8_t>(bi);
    return true;
}

// ============================================================
// Global palette instance
// ============================================================

Palette g_palette;

} // namespace enjin2
