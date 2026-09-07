#!/usr/bin/env python3
"""Regenerate blocks.ttf — the deterministic fixture for test_ttf2enjin.py.

Glyphs are axis-aligned rectangles so FreeType's monochrome raster is exact and
hinting-independent. unitsPerEm=16, so at pixel size 16 one font unit is ~1 px.
Requires fonttools (`pip install fonttools`). Run from the enjin repo root.
"""
from fontTools.fontBuilder import FontBuilder
from fontTools.pens.ttGlyphPen import TTGlyphPen

UPEM = 16
GLYPH_ORDER = [".notdef", "space", "A", "B", "C"]
CMAP = {0x20: "space", 0x41: "A", 0x42: "B", 0x43: "C"}


def box(pen, x0, y0, x1, y1):
    pen.moveTo((x0, y0))
    pen.lineTo((x1, y0))
    pen.lineTo((x1, y1))
    pen.lineTo((x0, y1))
    pen.closePath()


def build_glyph(name):
    pen = TTGlyphPen(None)
    if name == "A":            # solid 6x10 block
        box(pen, 1, 0, 7, 10)
    elif name == "B":          # two stacked bars (a hole between)
        box(pen, 1, 0, 7, 4)
        box(pen, 1, 6, 7, 10)
    elif name == "C":          # left column + top bar
        box(pen, 1, 0, 3, 10)
        box(pen, 1, 8, 7, 10)
    return pen.glyph()


def main():
    fb = FontBuilder(UPEM, isTTF=True)
    fb.setupGlyphOrder(GLYPH_ORDER)
    fb.setupCharacterMap(CMAP)
    glyphs = {n: build_glyph(n) for n in GLYPH_ORDER}
    fb.setupGlyf(glyphs)
    metrics = {}
    for n in GLYPH_ORDER:
        adv = 0 if n == ".notdef" else (6 if n == "space" else 8)
        metrics[n] = (adv, 0)
    fb.setupHorizontalMetrics(metrics)
    fb.setupHorizontalHeader(ascent=12, descent=-4)
    fb.setupNameTable({"familyName": "TomoBlocks", "styleName": "Regular"})
    fb.setupOS2(sTypoAscender=12, sTypoDescender=-4, usWinAscent=12, usWinDescent=4)
    fb.setupPost()
    out = __file__.rsplit("/", 1)[0] + "/blocks.ttf"
    fb.save(out)
    print("wrote", out)


if __name__ == "__main__":
    main()
