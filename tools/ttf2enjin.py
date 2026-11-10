#!/usr/bin/env python3
"""ttf2enjin.py — Convert a TTF/OTF into an enjin `GFXfont` C header.

A companion to `aseprite2enjin.py`, kept separate on purpose: the two tools
serve different input domains (pixel sprites vs. outline fonts) and share no
parsing. This one rasterises a font **monochrome (1-bit, no anti-aliasing) at
its native em size** and emits the Adafruit-GFX `GFXfont` byte layout that
`enjin2::TextRenderer` consumes unchanged — so the type-system renderer (#40)
gets its faces as plain applet data, integer-scale only (R4).

Rendering uses Pillow's FreeType backend: drawing onto a mode-'1' image is the
non-antialiased mono raster FreeType's `FT_LOAD_TARGET_MONO` produces, so no
grey coverage is spent (R4 has none to spend). Glyph metrics follow the
Adafruit `fontconvert` convention:

    xAdvance = round(advance width)
    xOffset  = ink left bearing (px from the pen)
    yOffset  = ink top relative to the baseline (negative above it)
    width/height = ink bounding box
    yAdvance = ascent + descent (line height)

Each glyph's bits are packed MSB-first and **byte-aligned at the glyph
boundary** (bitmapOffset counts bytes), matching `TextRenderer::drawChar`,
which restarts its bit cursor at each glyph's `bitmapOffset`.

A code point the TTF has no cmap entry for (a Latin-1 letter the face lacks),
and always the controls 0x7F-0x9F, is emitted as `{offset, 0, 0, 0, 0, 0}`: xAdvance 0 marks it missing,
and the renderer draws a tofu (ADR-0029).

`--glyphs FILE` (repeatable, later files win) merges hand-drawn glyphs over the
raster, in the text format of tools/faces/*.latin1.txt (see `parse_glyph_file`).

`--face-name NAME --ascent N --cap-height N` emit the face form: everything
inside `namespace enjin2::faces`, `<id>Bitmaps` / `<id>Glyphs` / `<id>Font`
and an `enjin2::Face <id>` (enjin2/graphics/face.hpp). Without them the plain
global `GFXfont <id>` form is emitted, as before.

Usage:
    ttf2enjin.py FONT.ttf --size 16 --name tomoDisplay16 -o tomo_display_16.h
    ttf2enjin.py FONT.ttf --size 16 --first 0x20 --last 0xFF \
        --glyphs faces/monogram.latin1.txt \
        --name monogram --face-name monogram --ascent 11 --cap-height 7 -o monogram.h
"""

import argparse
import os
import re
import shlex
import sys

try:
    from PIL import Image, ImageDraw, ImageFont
except ImportError:  # pragma: no cover - environment guard
    sys.stderr.write("ttf2enjin: Pillow (PIL) is required — pip install pillow\n")
    raise


class Glyph:
    __slots__ = ("code", "width", "height", "x_advance", "x_offset",
                 "y_offset", "bits")

    def __init__(self, code, width, height, x_advance, x_offset, y_offset, bits):
        self.code = code
        self.width = width
        self.height = height
        self.x_advance = x_advance
        self.x_offset = x_offset
        self.y_offset = y_offset
        self.bits = bits  # list[list[int]] rows of 0/1, ink-cropped


def font_cmap(font_path):
    """The code points `font_path` maps, or None when fontTools is unavailable."""
    try:
        from fontTools.ttLib import TTFont
    except ImportError:  # pragma: no cover - environment guard
        sys.stderr.write("ttf2enjin: fontTools not found — missing glyphs are not "
                         "detected (pip install fonttools)\n")
        return None
    return set(TTFont(font_path).getBestCmap())


def missing_glyph(code):
    """xAdvance 0: the face lacks this code point (the renderer draws a tofu)."""
    return Glyph(code, 0, 0, 0, 0, 0, [])


def rasterize(font_path, size, first, last):
    """Rasterise [first, last] of `font_path` at `size` px, mono / no AA.

    Returns (glyphs, y_advance). `glyphs` is ordered by code point; each carries
    its ink bitmap (byte-unpacked rows) plus GFX metrics. A code point the font
    has no cmap entry for comes back as `missing_glyph` (all zeros).
    """
    cmap = font_cmap(font_path)
    font = ImageFont.truetype(font_path, size)
    ascent, descent = font.getmetrics()
    y_advance = ascent + descent

    # Margin large enough for negative bearings / overshoot at this size.
    margin = size * 2
    canvas = size * 4

    glyphs = []
    for code in range(first, last + 1):
        if (cmap is not None and code not in cmap) or 0x7F <= code <= 0x9F:
            glyphs.append(missing_glyph(code))
            continue
        ch = chr(code)
        advance = int(round(font.getlength(ch)))

        # Draw onto a 1-bit image: freetype MONO, no anti-aliasing.
        img = Image.new("1", (canvas, canvas), 0)
        draw = ImageDraw.Draw(img)
        draw.text((margin, margin), ch, fill=1, font=font)
        bbox = img.getbbox()  # ink bounds, or None for an empty glyph (space)

        if bbox is None:
            glyphs.append(Glyph(code, 0, 0, advance, 0, 0, []))
            continue

        l, t, r, b = bbox
        width = r - l
        height = b - t
        x_offset = l - margin
        baseline = margin + ascent
        y_offset = t - baseline

        rows = []
        px = img.load()
        for y in range(t, b):
            row = [1 if px[x, y] else 0 for x in range(l, r)]
            rows.append(row)
        glyphs.append(Glyph(code, width, height, advance, x_offset, y_offset, rows))

    return glyphs, y_advance


_HEADER_RE = re.compile(
    r"^U\+([0-9A-Fa-f]{4,6})\b.*?\bxAdvance=(-?\d+)\s+xOffset=(-?\d+)\s+yOffset=(-?\d+)\s*$")


def parse_glyph_file(path):
    """Read hand-drawn glyphs: {code: Glyph}.

    One block per glyph, blocks separated by blank lines:

        U+00C0 À xAdvance=6 xOffset=0 yOffset=-10
        ..#..
        ...#.

    The header gives the advance and the top-left of the rows (yOffset is the top
    row relative to the baseline, as in GFXglyph); rows are '.'/'#', all the same
    length. A block with no rows is a glyph with no ink (a space). Outside a block,
    lines starting with '#' are comments. Blank border rows/columns are cropped.
    """
    glyphs = {}
    block = None

    def close():
        if block is None:
            return
        code, adv, xo, yo, rows, lineno = block
        widths = {len(r) for r in rows}
        if len(widths) > 1:
            raise ValueError("%s:%d: U+%04X rows differ in length" % (path, lineno, code))
        bits = [[1 if c == "#" else 0 for c in r] for r in rows]
        glyphs[code] = crop(Glyph(code, len(rows[0]) if rows else 0, len(rows), adv, xo,
                                  yo, bits))

    with open(path, encoding="utf-8") as f:
        for lineno, raw in enumerate(f, 1):
            line = raw.rstrip("\n").rstrip()
            if not line:
                close()
                block = None
                continue
            if block is None:
                if line.startswith("#"):
                    continue
                m = _HEADER_RE.match(line)
                if not m:
                    raise ValueError("%s:%d: expected 'U+XXXX <char> xAdvance=N xOffset=N "
                                     "yOffset=N', got %r" % (path, lineno, line))
                code = int(m.group(1), 16)
                if code in glyphs:
                    raise ValueError("%s:%d: U+%04X given twice" % (path, lineno, code))
                block = (code, int(m.group(2)), int(m.group(3)), int(m.group(4)), [],
                         lineno)
                continue
            if not re.fullmatch(r"[.#]+", line):
                raise ValueError("%s:%d: a glyph row is '.'/'#' only, got %r"
                                 % (path, lineno, line))
            block[4].append(line)
    close()
    return glyphs


def crop(g):
    """Trim blank border rows/columns, moving the offsets so the ink stays put."""
    ink = [(x, y) for y, row in enumerate(g.bits) for x, b in enumerate(row) if b]
    if not ink:
        return Glyph(g.code, 0, 0, g.x_advance, 0, 0, [])
    l = min(x for x, _ in ink)
    r = max(x for x, _ in ink) + 1
    t = min(y for _, y in ink)
    b = max(y for _, y in ink) + 1
    rows = [row[l:r] for row in g.bits[t:b]]
    return Glyph(g.code, r - l, b - t, g.x_advance, g.x_offset + l, g.y_offset + t, rows)


def merge_glyphs(glyphs, overrides):
    """`glyphs` with each code point in `overrides` (in range) replaced."""
    return [overrides.get(g.code, g) for g in glyphs]


def pack_bits(glyph):
    """Pack a glyph's ink rows MSB-first, byte-aligned at the glyph boundary."""
    out = bytearray()
    acc = 0
    nbits = 0
    for row in glyph.bits:
        for bit in row:
            acc = (acc << 1) | (bit & 1)
            nbits += 1
            if nbits == 8:
                out.append(acc)
                acc = 0
                nbits = 0
    if nbits:  # flush the final partial byte, padded low with zeros
        out.append(acc << (8 - nbits))
    return bytes(out)


def emit_header(glyphs, y_advance, name, first, last):
    """Render the C header text for a GFXfont named `name`."""
    guard = name.upper() + "_H"
    bitmap = bytearray()
    offsets = []
    for g in glyphs:
        offsets.append(len(bitmap))
        bitmap.extend(pack_bits(g))

    lines = []
    lines.append("#ifndef %s" % guard)
    lines.append("#define %s" % guard)
    lines.append("")
    lines.append("// Generated by tools/ttf2enjin.py — do not edit by hand.")
    lines.append("// Mono 1-bit GFXfont for enjin2::TextRenderer (type system #40).")
    lines.append('#include "enjin2/graphics/gfxfont.h"')
    lines.append("")

    # Bitmap array. The three definitions are C++17 inline variables, so every
    # translation unit that includes the header shares one copy (and one address).
    lines.append("inline const uint8_t %sBitmaps[] = {" % name)
    row = "  "
    for i, byte in enumerate(bitmap):
        row += "0x%02X, " % byte
        if (i % 12) == 11:
            lines.append(row.rstrip())
            row = "  "
    if row.strip():
        lines.append(row.rstrip())
    lines.append("};")
    lines.append("")

    # Glyph array
    lines.append("inline const GFXglyph %sGlyphs[] = {" % name)
    lines.append("  // bitmapOffset, width, height, xAdvance, xOffset, yOffset")
    for g, off in zip(glyphs, offsets):
        ch = g.code
        printable = chr(ch) if 0x20 < ch <= 0x7E else " "
        lines.append(
            "  { %5d, %3d, %3d, %3d, %4d, %4d },   // 0x%02X '%s'"
            % (off, g.width, g.height, g.x_advance, g.x_offset, g.y_offset, ch,
               printable if ch != 0x20 else " ")
        )
    lines.append("};")
    lines.append("")

    # Font struct
    lines.append("inline const GFXfont %s = {" % name)
    lines.append("  (uint8_t  *)%sBitmaps," % name)
    lines.append("  (GFXglyph *)%sGlyphs, 0x%02X, 0x%02X, %d };"
                 % (name, first, last, y_advance))
    lines.append("")
    lines.append("#endif // %s" % guard)
    lines.append("")
    return "\n".join(lines)


def _printable(ch):
    if 0x20 < ch <= 0x7E or (0xA0 < ch <= 0xFF and ch != 0xAD):
        return chr(ch)
    return " "


def emit_face_header(glyphs, y_advance, ident, face_name, ascent, cap_height, first,
                     last, comments=()):
    """Render the face form: GFXfont + enjin2::Face inside namespace enjin2::faces."""
    bitmap = bytearray()
    offsets = []
    for g in glyphs:
        offsets.append(len(bitmap))
        bitmap.extend(pack_bits(g))
    if len(bitmap) > 0xFFFF:
        raise ValueError("bitmap is %d bytes; GFXglyph.bitmapOffset is 16-bit" % len(bitmap))

    lines = ["#pragma once", "",
             "// Generated by tools/ttf2enjin.py — do not edit by hand.",
             "// Mono 1-bit GFXfont + enjin2::Face for enjin2::TextRenderer (ADR-0029)."]
    lines += ["// " + c if c else "//" for c in comments]
    lines += ['#include "enjin2/graphics/face.hpp"', "", "namespace enjin2::faces {", ""]

    lines.append("inline const uint8_t %sBitmaps[] = {" % ident)
    row = "  "
    for i, byte in enumerate(bitmap):
        row += "0x%02X, " % byte
        if (i % 12) == 11:
            lines.append(row.rstrip())
            row = "  "
    if row.strip():
        lines.append(row.rstrip())
    lines.append("};")
    lines.append("")

    lines.append("inline const ::GFXglyph %sGlyphs[] = {" % ident)
    lines.append("  // bitmapOffset, width, height, xAdvance, xOffset, yOffset")
    for g, off in zip(glyphs, offsets):
        note = "  missing" if (g.x_advance == 0 and g.width == 0) else ""
        lines.append(
            "  { %5d, %3d, %3d, %3d, %4d, %4d },   // 0x%02X '%s'%s"
            % (off, g.width, g.height, g.x_advance, g.x_offset, g.y_offset, g.code,
               _printable(g.code), note))
    lines.append("};")
    lines.append("")

    lines.append("inline const ::GFXfont %sFont = {" % ident)
    lines.append("  (uint8_t  *)%sBitmaps," % ident)
    lines.append("  (::GFXglyph *)%sGlyphs, 0x%02X, 0x%02X, %d };"
                 % (ident, first, last, y_advance))
    lines.append("")
    lines.append('inline const ::enjin2::Face %s{"%s", &%sFont, %d, %d};'
                 % (ident, face_name, ident, ascent, cap_height))
    lines.append("")
    lines.append("}  // namespace enjin2::faces")
    lines.append("")
    return "\n".join(lines)


def main(argv=None):
    ap = argparse.ArgumentParser(description="Convert a TTF to an enjin GFXfont header.")
    ap.add_argument("font", help="input .ttf / .otf path")
    ap.add_argument("--size", type=int, required=True, help="pixel size (em height)")
    ap.add_argument("--name", help="C identifier for the GFXfont (the face id with "
                    "--face-name; defaults to it)")
    ap.add_argument("--first", type=lambda s: int(s, 0), default=0x20,
                    help="first code point (default 0x20)")
    ap.add_argument("--last", type=lambda s: int(s, 0), default=0x7E,
                    help="last code point (default 0x7E)")
    ap.add_argument("--glyphs", action="append", default=[], metavar="FILE",
                    help="merge/override glyphs from a text glyph file (repeatable)")
    ap.add_argument("--face-name", help="emit the enjin2::Face form, with this name")
    ap.add_argument("--ascent", type=int, help="face: line top to baseline, px")
    ap.add_argument("--cap-height", type=int, help="face: cap height, px")
    ap.add_argument("--describe", action="append", default=[], metavar="TEXT",
                    help="face: a comment line for the header (repeatable)")
    ap.add_argument("-o", "--output", help="output header path (default: stdout)")
    raw_argv = list(sys.argv[1:] if argv is None else argv)
    args = ap.parse_args(argv)
    if args.face_name is None and args.name is None:
        ap.error("--name is required")
    if args.face_name is not None and (args.ascent is None or args.cap_height is None):
        ap.error("--face-name needs --ascent and --cap-height")

    glyphs, y_advance = rasterize(args.font, args.size, args.first, args.last)
    overrides = {}
    for path in args.glyphs:
        overrides.update(parse_glyph_file(path))
    glyphs = merge_glyphs(glyphs, overrides)

    if args.face_name is not None:
        ident = args.name or args.face_name
        comments = list(args.describe)
        drawn = sorted(c for c in overrides if args.first <= c <= args.last)
        if drawn:
            comments.append("Hand-drawn (--glyphs): %d glyphs, U+%04X..U+%04X."
                            % (len(drawn), drawn[0], drawn[-1]))
        missing = [g.code for g in glyphs if g.x_advance == 0 and g.width == 0]
        comments.append("Missing (tofu): %d code points." % len(missing))
        comments.append("Regenerate (from the Tomodachi repo root):")
        comments.append("  python3 libs/enjin/tools/ttf2enjin.py " + shlex.join(raw_argv))
        header = emit_face_header(glyphs, y_advance, ident, args.face_name, args.ascent,
                                  args.cap_height, args.first, args.last, comments)
    else:
        header = emit_header(glyphs, y_advance, args.name, args.first, args.last)

    if args.output:
        with open(args.output, "w") as f:
            f.write(header)
        sys.stderr.write("ttf2enjin: wrote %s (%d glyphs)\n"
                         % (args.output, len(glyphs)))
    else:
        sys.stdout.write(header)
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
