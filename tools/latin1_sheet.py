#!/usr/bin/env python3
"""latin1_sheet.py — the owner's review sheet for a face's Latin-1 glyphs (Tomodachi #476).

Renders U+00A0..U+00FF of a face, as ttf2enjin.py would emit it (the TTF raster with
`--glyphs` files merged over it), enlarged in a grid: each glyph beside its base
letter, labelled, drafted glyphs (from the --glyphs file) on a yellow cell and the
TTF's own on a grey one, with the line top (blue), cap height (grey) and baseline
(red) guides. Below the grid, one sample line at 1x and 4x.

Usage (from the Tomodachi repo root):
    python3 libs/enjin/tools/latin1_sheet.py pixel_fonts/monogram.ttf --size 16 \\
        --ascent 11 --cap-height 7 --glyphs libs/enjin/tools/faces/monogram.latin1.txt \\
        --scale 8 --title monogram -o latin1-monogram.png
"""

import argparse
import os
import sys

from PIL import Image, ImageDraw, ImageFont

HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, HERE)

import ttf2enjin  # noqa: E402
from draft_latin1 import BASE_LETTER  # noqa: E402

SAMPLE = "Ça va? Déjà vu — naïve façade, Ærøskøbing, Größe, ¿Qué tal? ½ °C ±5 ©"
LABEL_FONTS = ["/usr/share/fonts/noto/NotoSans-Regular.ttf",
               "/usr/share/fonts/TTF/DejaVuSans.ttf",
               "/usr/share/fonts/truetype/dejavu/DejaVuSans.ttf"]

BG = (255, 255, 255)
CELL_TTF = (232, 236, 242)
CELL_DRAFT = (255, 243, 196)
CELL_MISSING = (255, 210, 210)
INK = (20, 20, 20)
BASE_INK = (150, 150, 150)
LINE_TOP = (40, 90, 220)
BASELINE = (220, 40, 40)
CAP = (170, 170, 170)
GRID = (0, 0, 0, 18)


def label_font(size):
    for p in LABEL_FONTS:
        if os.path.exists(p):
            return ImageFont.truetype(p, size)
    return ImageFont.load_default()


def load_face(font, size, glyph_files):
    glyphs, y_adv = ttf2enjin.rasterize(font, size, 0x20, 0xFF)
    drawn = {}
    for p in glyph_files:
        drawn.update(ttf2enjin.parse_glyph_file(p))
    merged = {g.code: g for g in ttf2enjin.merge_glyphs(glyphs, drawn)}
    return merged, y_adv, set(drawn)


def missing(g):
    return g.x_advance == 0 and g.width == 0


def draw_glyph(draw, g, pen_x, base_y, s, colour, cap_height):
    """Draw glyph `g` with its pen at (pen_x, base_y) screen px, `s` px per font px."""
    if missing(g):  # the renderer's tofu: a hollow box one advance wide, cap tall
        w = max(3, cap_height * 2 // 3)
        draw.rectangle([pen_x, base_y - cap_height * s, pen_x + w * s - 1, base_y - 1],
                       outline=colour, width=max(1, s // 2))
        return w + 1
    for r, row in enumerate(g.bits):
        for c, bit in enumerate(row):
            if bit:
                x = pen_x + (g.x_offset + c) * s
                y = base_y + (g.y_offset + r) * s
                draw.rectangle([x, y, x + s - 1, y + s - 1], fill=colour)
    return g.x_advance


def render_line(face, text, s, ascent, y_adv, cap_height, pad=4):
    adv = sum((face[ord(ch)].x_advance if ord(ch) in face and not missing(face[ord(ch)])
               else cap_height * 2 // 3 + 1) for ch in text)
    img = Image.new("RGB", ((adv + 2 * pad) * s, (y_adv + 2 * pad) * s), BG)
    d = ImageDraw.Draw(img)
    pen = pad * s
    base = (pad + ascent) * s
    for ch in text:
        code = ord(ch)
        g = face.get(code, ttf2enjin.missing_glyph(code))
        pen += draw_glyph(d, g, pen, base, s, INK, cap_height) * s
    return img


def main(argv=None):
    ap = argparse.ArgumentParser(description=__doc__.split("\n")[0])
    ap.add_argument("font")
    ap.add_argument("--size", type=int, required=True)
    ap.add_argument("--ascent", type=int, required=True)
    ap.add_argument("--cap-height", type=int, required=True)
    ap.add_argument("--glyphs", action="append", default=[])
    ap.add_argument("--scale", type=int, default=8)
    ap.add_argument("--title", default="")
    ap.add_argument("--sample", default=SAMPLE)
    ap.add_argument("-o", "--output", required=True)
    args = ap.parse_args(argv)

    face, y_adv, drawn = load_face(args.font, args.size, args.glyphs)
    s = args.scale
    asc, cap = args.ascent, args.cap_height
    codes = list(range(0xA0, 0x100))
    max_adv = max(max(face[c].x_advance for c in codes), 4)
    base_w = max(face[ord(b)].x_advance for b in set(BASE_LETTER.values()))
    lab = label_font(13)
    title_f = label_font(22)

    pad = 8
    cell_w = pad + (base_w + 1) * s + pad + (max_adv + 1) * s + pad
    label_h = 20
    cell_h = label_h + (y_adv + 2) * s + pad
    cols = 16
    rows = (len(codes) + cols - 1) // cols
    head_h = 70

    one = render_line(face, args.sample, 1, asc, y_adv, cap)
    four = render_line(face, args.sample, 4, asc, y_adv, cap)
    grid_w = cols * cell_w
    width = max(grid_w, four.width) + 2 * pad
    height = head_h + rows * cell_h + 30 + one.height + 20 + four.height + 2 * pad
    img = Image.new("RGB", (width, height), BG)
    d = ImageDraw.Draw(img, "RGBA")

    n_drawn = sum(1 for c in codes if c in drawn)
    n_missing = sum(1 for c in codes if missing(face[c]))
    d.text((pad, 8), "%s — Latin-1 U+00A0..U+00FF  (%dx; ascent %d, cap %d, yAdvance %d)"
           % (args.title, s, asc, cap, y_adv), font=title_f, fill=INK)
    legend_y = 40
    x = pad
    for colour, text in [(CELL_DRAFT, "drafted (%d) — review these" % n_drawn),
                         (CELL_TTF, "from the TTF (%d)" % (len(codes) - n_drawn - n_missing)),
                         (CELL_MISSING, "missing (%d)" % n_missing)]:
        d.rectangle([x, legend_y, x + 16, legend_y + 16], fill=colour, outline=CAP)
        d.text((x + 22, legend_y), text, font=lab, fill=INK)
        x += 260
    for colour, text in [(LINE_TOP, "line top"), (CAP, "cap height"), (BASELINE, "baseline"),
                         (BASE_INK, "grey = base letter")]:
        d.line([x, legend_y + 8, x + 18, legend_y + 8], fill=colour, width=2)
        d.text((x + 24, legend_y), text, font=lab, fill=INK)
        x += 150

    for i, code in enumerate(codes):
        cx = pad + (i % cols) * cell_w
        cy = head_h + (i // cols) * cell_h
        g = face[code]
        fill = CELL_MISSING if missing(g) else (CELL_DRAFT if code in drawn else CELL_TTF)
        d.rectangle([cx + 1, cy + 1, cx + cell_w - 2, cy + cell_h - 2], fill=fill)
        name = {0xA0: "NBSP", 0xAD: "SHY"}.get(code, chr(code))
        d.text((cx + 5, cy + 3), "U+%04X %s" % (code, name), font=lab, fill=INK)
        top = cy + label_h + s  # the line top, screen px
        base = top + asc * s
        x0 = cx + pad
        x1 = cx + cell_w - pad
        # pixel grid over the line box
        for yy in range(top, top + y_adv * s + 1, s):
            d.line([x0, yy, x1, yy], fill=GRID)
        d.line([x0, top, x1, top], fill=LINE_TOP, width=1)
        d.line([x0, base - cap * s, x1, base - cap * s], fill=CAP, width=1)
        d.line([x0, base, x1, base], fill=BASELINE, width=1)
        bl = BASE_LETTER.get(code)
        if bl:
            draw_glyph(d, face[ord(bl)], x0, base, s, BASE_INK, cap)
        pen = x0 + (base_w + 1) * s + pad
        d.line([pen, top, pen, top + y_adv * s], fill=(0, 160, 0, 90))
        adv = g.x_advance
        d.line([pen + adv * s, top, pen + adv * s, top + y_adv * s], fill=(0, 160, 0, 90))
        draw_glyph(d, g, pen, base, s, INK, cap)

    y = head_h + rows * cell_h + 10
    d.text((pad, y), "1x:", font=lab, fill=INK)
    img.paste(one, (pad + 30, y + 18))
    y += 18 + one.height + 6
    d.text((pad, y), "4x:", font=lab, fill=INK)
    img.paste(four, (pad, y + 18))
    img.save(args.output)
    sys.stderr.write("latin1_sheet: wrote %s\n" % args.output)
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
