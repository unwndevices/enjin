#include "../../include/enjin2/graphics/palette.hpp"
#include <cstring>
#include <cstdio>

namespace enjin2 {

// ============================================================
// Default palette: PICO-8 minus #94b0c2, 15 colors (indices 0-14)
// ============================================================
static constexpr RGB DEFAULT_COLORS[15] = {
    {0x1a, 0x1c, 0x2c}, // 0  — dark navy (default background)
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
    const char* name;
    const RGB*  colors;
    uint8_t     size;
};

static const PalettePreset PRESETS[] = {
    {"default", DEFAULT_COLORS, 15},
    {"gameboy",  GAMEBOY_COLORS,  4},
};

static constexpr int PRESET_COUNT = static_cast<int>(sizeof(PRESETS) / sizeof(PRESETS[0]));

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
    if (!name) {
        return false;
    }
    for (int i = 0; i < PRESET_COUNT; ++i) {
        if (strcmp(name, PRESETS[i].name) == 0) {
            size = PRESETS[i].size;
            for (uint8_t j = 0; j < size && j < PALETTE_MAX_ENTRIES; ++j) {
                colors[j] = PRESETS[i].colors[j];
            }
            return true;
        }
    }
    return false;
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
