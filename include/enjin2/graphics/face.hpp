#pragma once
#include "gfxfont.h"
#include <cstdint>
namespace enjin2 {
/// A face: one GFXfont plus the line metrics text layout needs (ADR-0029).
/// y passed to text calls is the line's top; the baseline is y + ascent * size.
struct Face {
    const char* name;      ///< the name Lua's gfx.setFont takes
    const ::GFXfont* font; ///< glyphs U+0020..U+00FF; xAdvance 0 marks a missing one
    uint8_t ascent;        ///< line top to baseline, px at size 1
    uint8_t capHeight;     ///< cap height, px at size 1 (the tofu's height)
};
}  // namespace enjin2
