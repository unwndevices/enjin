---
id: TextRenderer
title: TextRenderer
sidebar_label: TextRenderer
---

# TextRenderer

`enjin2::TextRenderer<TPixel>` (`graphics/text_renderer.hpp`) is the one way text reaches a canvas, from
native code and from Lua (Tomodachi ADR-0029). It is a view of one canvas's sticky `TextState`
(`graphics/text_state.hpp`): every setting it makes stays on that canvas until changed.

## The rules

- `y` is the top of the line; the baseline is `y + face.ascent * size`.
- A string's width is its advance.
- Text is UTF-8. A character the face lacks draws a tofu (a hollow box one advance wide), logged once per
  character and face (`text::missingLog()` replaces the log).
- Overflow is one setting, `TextOverflow::Wrap | Clip | Ellipsis`, on a box that runs from the anchor to the
  given width or to the layer's edge. Wrapping breaks at spaces, and inside a word only when the word is
  longer than the line. A newline starts a line.

## Faces

`enjin2::Face` (`graphics/face.hpp`) is a `GFXfont` plus its name, ascent and cap height. Three ship, in
`graphics/faces/`: `faces::monogram` (the default), `faces::body` and `faces::display`, each covering
U+0020–U+00FF. `faces::find(name)` looks one up by the name Lua uses.

## State (sticky, per canvas)

| Call | Sets |
|---|---|
| `setFont(const Face&)` | the face |
| `setTextColor(uint8_t)` / `setTextColor(TPixel)` | the fill's palette index |
| `setTextSize(uint8_t)` | the integer scale |
| `setTextWrap(TextOverflow, int16_t width = 0)` | the overflow rule and the box width (0: the layer's edge) |
| `setCursor(x, y)` | where `print()` goes on |
| `setOutline(bool[, uint8_t])` | the legibility outline, bg unless a colour is given |
| `setEffect(const Effect&)` / `clearEffect()` | the index shader every fill pixel runs through |
| `setClip(x, y, w, h)` / `clearClip()` | a window text draws inside |
| `reset()` | every setting back to its default |

## Drawing and measuring

- `int draw(std::string_view s, int x, int y, TextAlign align = TextAlign::Left)`: x is the line's left
  edge, centre or right edge; returns the lines drawn and leaves the cursor at the end of the last one.
- `int print(std::string_view s)`: `draw` at the cursor.
- `int width(std::string_view s)`, `int lineHeight()`, `int ascent()`.
- Without a canvas: `text::width(face, s, size)`, `text::lineHeight(face, size)` and
  `text::forEachLine(face, size, overflow, s, boxW, fn)`, the line breaker `draw` uses (Dialog paginates
  with it).

```cpp
enjin2::TextRenderer<enjin2::Pixel4> t(layer);
t.reset();
t.setFont(enjin2::faces::body);
t.setTextWrap(enjin2::TextOverflow::Ellipsis, 80);
t.draw("Track 01", 10, 4);
t.draw("12:30", 150, 4, enjin2::TextAlign::Right);
```
