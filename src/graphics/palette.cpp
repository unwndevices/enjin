#include "../../include/enjin2/graphics/palette.hpp"
#include <cstring>
#include <cstdio>

namespace enjin2 {

// ============================================================
// The house default (ADR-0019): a placeholder until the designer picks, the
// green-ramp neutrals plus three hues picked to read on bg. Indices 0-7 are
// the Palette roles (palette.hpp ROLE_*).
//
// Temporary: entries 8-14 keep the old 15-step green ramp's colours so art
// not yet re-indexed by job still draws. The seal ticket removes them.
// Index 15 is transparent.
// ============================================================
static constexpr RGB DEFAULT_COLORS[15] = {
    {0x08, 0x1a, 0x1c}, // 0  bg
    {0x16, 0x46, 0x32}, // 1  surface
    {0x52, 0xa8, 0x2e}, // 2  muted
    {0xe8, 0xf4, 0xcc}, // 3  text
    {0xb2, 0xda, 0x63}, // 4  accent
    {0xf0, 0xa8, 0x30}, // 5  accent2 — amber
    {0x48, 0xc8, 0xd8}, // 6  accent3 — cyan
    {0xe8, 0x48, 0x40}, // 7  accent4 — red
    {0x52, 0xa8, 0x2e}, // 8  temporary: the old ramp's 8
    {0x6b, 0xb9, 0x35}, // 9  temporary
    {0x83, 0xca, 0x3c}, // 10 temporary
    {0x9b, 0xd2, 0x50}, // 11 temporary
    {0xb2, 0xda, 0x63}, // 12 temporary
    {0xce, 0xe8, 0x96}, // 13 temporary
    {0xe8, 0xf4, 0xcc}, // 14 temporary: the old ramp's lightest
};

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
    if (!name || std::strcmp(name, "default") != 0) {
        return false;
    }
    size = PALETTE_MAX_ENTRIES;
    for (uint8_t j = 0; j < PALETTE_MAX_ENTRIES; ++j) {
        colors[j] = DEFAULT_COLORS[j];
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
