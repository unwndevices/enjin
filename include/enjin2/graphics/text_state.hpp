#pragma once

// The sticky text state every canvas carries (ADR-0029), Adafruit-style: a
// setting stays until it is changed. It lives on the canvas, so each layer has
// its own and nothing set on an Applet's layer leaks onto the Menu panel's or
// the System layer's. Lua's gfx.setFont / setTextWrap / setTextSize and native
// code's TextRenderer reach the same state.

#include "effect.hpp"
#include "face.hpp"
#include "palette.hpp"
#include <cstdint>

namespace enjin2 {

/// Where a line goes when it is longer than its box.
enum class TextOverflow : uint8_t {
    Wrap,      ///< onto the next line: at a space, inside a word only when the word is longer than the line
    Clip,      ///< cut at the box's edge
    Ellipsis,  ///< cut so that "..." fits at the box's edge
};

/// Where x sits on a line: its left edge, its centre or its right edge.
enum class TextAlign : uint8_t { Left, Center, Right };

/// The names Lua spells them with, in enum order.
inline constexpr const char* kTextAlignNames[] = {"left", "center", "right"};
inline constexpr const char* kTextOverflowNames[] = {"wrap", "clip", "ellipsis"};

struct TextState {
    /// The face; nullptr is the default face (monogram).
    const Face* face = nullptr;
    /// The fill's palette index.
    uint8_t color = ROLE_TEXT;
    /// Integer scale (setTextSize), 1..255.
    uint8_t size = 1;
    TextOverflow overflow = TextOverflow::Wrap;
    /// The box's width; 0 runs the box to the layer's edge.
    int16_t width = 0;
    /// Where print() continues: the top of its line.
    int16_t cursorX = 0;
    int16_t cursorY = 0;
    /// The legibility outline: the glyph silhouette at the four ±1 px offsets
    /// behind the fill, in outlineColor (the bg role unless one was given).
    bool outline = false;
    uint8_t outlineColor = ROLE_BG;
    /// The index shader every fill pixel runs through, when set.
    bool hasEffect = false;
    Effect effect{};
    /// A window text draws inside, in canvas coordinates (x1/y1 exclusive);
    /// the default is the whole canvas.
    int16_t clipX0 = INT16_MIN, clipY0 = INT16_MIN, clipX1 = INT16_MAX, clipY1 = INT16_MAX;
};

}  // namespace enjin2
