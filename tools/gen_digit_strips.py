#!/usr/bin/env python3
"""Generate the HUD digit-strip .njn v2 sheets (ADR-0003 §8, Tomodachi #83).

Two fixed-width sheets are emitted:
  * score.njn — 10 frames, glyphs 0-9.
  * timer.njn — 11 frames, glyphs 0-9 plus ':' at frame 10 (kNumeralColonFrame).

Each frame is a monospace CELL_W x CELL_H cell; the frame index *is* the glyph
(digit d -> frame d, colon -> frame 10). Pixels are 4bpp palette indices: the
foreground uses index 7 (WHITE in the engine palette) and the background uses
index 15 (the compile-time transparent index). The output is a plain #55 sheet
(META + PIXL, no CLIP) laid out frame-major, exactly what SpriteSheet::draw and
loadNjnAsset expect.

These are legible placeholders baked from a built-in 3x5 pixel font; the import
pipeline (#87) can replace them with authored art later ("format size freedom,
pre-users"). Re-run after editing the font:  python3 tools/gen_digit_strips.py
"""
import os
import struct

CELL_W = 6            # monospace advance in pixels
CELL_H = 8
GLYPH_X = 1           # top-left of the 3x5 glyph within the cell
GLYPH_Y = 2
FG = 7                # WHITE palette index
BG = 15               # transparent index

# 3x5 glyph bitmaps, top row first; '1' = foreground pixel.
FONT = {
    "0": ["111", "101", "101", "101", "111"],
    "1": ["010", "110", "010", "010", "111"],
    "2": ["111", "001", "111", "100", "111"],
    "3": ["111", "001", "111", "001", "111"],
    "4": ["101", "101", "111", "001", "001"],
    "5": ["111", "100", "111", "001", "111"],
    "6": ["111", "100", "111", "101", "111"],
    "7": ["111", "001", "010", "010", "010"],
    "8": ["111", "101", "111", "101", "111"],
    "9": ["111", "101", "111", "001", "111"],
    ":": ["000", "010", "000", "010", "000"],
}

# Chunk tags (must match njn2.hpp).
MAGIC = b"NJ"
VERSION = 2
TAG_META = b"META"
TAG_PIXL = b"PIXL"


def render_frame(glyph):
    """Return CELL_W*CELL_H bytes (frame-major) for one glyph."""
    px = bytearray([BG]) * (CELL_W * CELL_H)
    rows = FONT[glyph]
    for gy, row in enumerate(rows):
        for gx, bit in enumerate(row):
            if bit == "1":
                x = GLYPH_X + gx
                y = GLYPH_Y + gy
                px[y * CELL_W + x] = FG
    return bytes(px)


def build_njn(glyphs):
    """Serialise a frame-major sheet of `glyphs` into a .njn v2 byte buffer."""
    pixl = b"".join(render_frame(g) for g in glyphs)
    cols = len(glyphs)
    meta = bytes([CELL_W, CELL_H, cols, 1])

    chunks = [(TAG_META, meta), (TAG_PIXL, pixl)]
    header_size = 12
    dir_size = 12 * len(chunks)
    data_offset = header_size + dir_size

    # File size.
    file_size = data_offset + sum(len(d) for _, d in chunks)

    out = bytearray()
    out += MAGIC
    out += bytes([VERSION, 0])
    out += struct.pack("<II", len(chunks), file_size)

    # Directory.
    off = data_offset
    for tag, data in chunks:
        out += tag
        out += struct.pack("<II", off, len(data))
        off += len(data)

    # Chunk data.
    for _, data in chunks:
        out += data

    assert len(out) == file_size, (len(out), file_size)
    return bytes(out)


def main():
    here = os.path.dirname(os.path.abspath(__file__))
    out_dir = os.path.join(here, "..", "assets", "hud")
    os.makedirs(out_dir, exist_ok=True)

    score_glyphs = [str(d) for d in range(10)]
    timer_glyphs = score_glyphs + [":"]

    for name, glyphs in (("score", score_glyphs), ("timer", timer_glyphs)):
        path = os.path.join(out_dir, name + ".njn")
        with open(path, "wb") as f:
            f.write(build_njn(glyphs))
        print("wrote %s (%d frames, %dx%d cells)"
              % (os.path.normpath(path), len(glyphs), CELL_W, CELL_H))


if __name__ == "__main__":
    main()
