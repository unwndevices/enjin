# HUD digit strips (ADR-0003 §8, #83)

Two fixed-width `.njn` v2 digit strips consumed by `gfx.number` / `gfx.timer`
(and the C++ `drawNumber` / `drawTimer`):

| File        | Frames | Glyphs                      |
|-------------|--------|-----------------------------|
| `score.njn` | 10     | `0`–`9`                     |
| `timer.njn` | 11     | `0`–`9`, then `:` at frame 10 |

The frame index *is* the glyph: digit `d` → frame `d`, colon → frame
`kNumeralColonFrame` (10). Cells are 6×8, monospace (advance = `cellW`).
Foreground is palette index 7 (WHITE); background is index 15 (transparent).

These are legible placeholders baked from a built-in 3×5 font by
`tools/gen_digit_strips.py`. Regenerate after editing the font:

```
python3 tools/gen_digit_strips.py
```

The import pipeline (#87) can replace them with authored art later — there are
no users yet, so the format/size is free to change.
