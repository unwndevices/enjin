#include "../../include/enjin2/graphics/palette.hpp"
#include <cstring>
#include <cstdio>

namespace enjin2 {

// ============================================================
// The house default (ADR-0019): a placeholder until the designer picks, the
// green-ramp neutrals plus three hues picked to read on bg. Indices 0-7 are
// the Palette roles (palette.hpp ROLE_*); 8-14 are invalid and 15 is
// transparent.
// ============================================================
static constexpr RGB DEFAULT_COLORS[ROLE_COUNT] = {
    {0x08, 0x1a, 0x1c}, // 0  bg
    {0x16, 0x46, 0x32}, // 1  surface
    {0x52, 0xa8, 0x2e}, // 2  muted
    {0xe8, 0xf4, 0xcc}, // 3  text
    {0xb2, 0xda, 0x63}, // 4  accent
    {0xf0, 0xa8, 0x30}, // 5  accent2 — amber
    {0x48, 0xc8, 0xd8}, // 6  accent3 — cyan
    {0xe8, 0x48, 0x40}, // 7  accent4 — red
};

// ============================================================
// Palette methods
// ============================================================

Palette::Palette()
    : debugTransparent(false)
{
    for (uint8_t i = 0; i < ROLE_COUNT; ++i) {
        colors[i] = DEFAULT_COLORS[i];
    }
}

void Palette::setColor(uint8_t index, uint8_t r, uint8_t g, uint8_t b)
{
    if (index >= ROLE_COUNT) {
        return; // transparent (15) and the invalid 8-14 hold no colour
    }
    colors[index] = RGB{r, g, b};
}

RGB Palette::getColor(uint8_t index) const
{
    return index < ROLE_COUNT ? colors[index] : RGB{0, 0, 0};
}

RGB Palette::resolve(uint8_t index) const
{
    // Caller must have checked isTransparent() first; 8-14 are unused slots.
    return index < ROLE_COUNT ? colors[index] : RGB{0, 0, 0};
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
    for (uint8_t j = 0; j < ROLE_COUNT; ++j) {
        colors[j] = DEFAULT_COLORS[j];
    }
    return true;
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
