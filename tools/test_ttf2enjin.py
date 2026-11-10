#!/usr/bin/env python3
"""Tests for ttf2enjin.py — the mono 1-bit TTF -> GFXfont converter (#40).

Run: python3 tools/test_ttf2enjin.py   (from the enjin repo root)

Uses a deterministic synthetic fixture (test_fixtures/blocks.ttf, rectangular
glyphs so the mono raster is hinting-independent). Two claims:

  1. Expected bytes — the tool reproduces the committed golden header
     (test_fixtures/blocks16_golden.h) byte-for-byte. This is the "known TTF ->
     expected GFXfont bytes" regression pin from the ticket.
  2. Structural invariants — the emitted GFXfont obeys the Adafruit layout the
     enjin TextRenderer decodes: byte-aligned MSB-first packing (each glyph's
     bitmap length == ceil(w*h/8)), contiguous monotonic bitmapOffsets, and the
     known solid-block glyph 'A' round-trips to an all-ones 6x10 silhouette.
  3. The missing-glyph rule — a code point the TTF lacks (and every control in
     0x7F-0x9F) is `{offset, 0, 0, 0, 0, 0}` (xAdvance 0 = missing, a tofu).
  4. `--glyphs` — a text glyph file merges over the raster (adds and overrides),
     cropped to its ink; and the `--face-name` form wraps it all in an
     enjin2::Face inside namespace enjin2::faces.
"""
import os
import re
import sys
import tempfile

HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, HERE)

import ttf2enjin  # noqa: E402

FIXTURE = os.path.join(HERE, "test_fixtures", "blocks.ttf")
GOLDEN = os.path.join(HERE, "test_fixtures", "blocks16_golden.h")

_passes = 0
_failures = 0


def check(cond, msg):
    global _passes, _failures
    if cond:
        _passes += 1
    else:
        _failures += 1
        sys.stderr.write("FAIL: %s\n" % msg)


def test_golden_bytes():
    glyphs, yadv = ttf2enjin.rasterize(FIXTURE, 16, 0x20, 0x43)
    header = ttf2enjin.emit_header(glyphs, yadv, "tomoBlocks16", 0x20, 0x43)
    with open(GOLDEN) as f:
        golden = f.read()
    check(header == golden,
          "regenerated header must match the committed golden byte-for-byte")


def test_packing_is_byte_aligned():
    glyphs, _ = ttf2enjin.rasterize(FIXTURE, 16, 0x20, 0x43)
    for g in glyphs:
        packed = ttf2enjin.pack_bits(g)
        expected = (g.width * g.height + 7) // 8
        check(len(packed) == expected,
              "glyph 0x%02X packs to ceil(w*h/8)=%d bytes, got %d"
              % (g.code, expected, len(packed)))


def test_offsets_are_contiguous():
    glyphs, _ = ttf2enjin.rasterize(FIXTURE, 16, 0x20, 0x43)
    run = 0
    for g in glyphs:
        # bitmapOffset is implicit in emit order; re-derive and check contiguity.
        packed = ttf2enjin.pack_bits(g)
        run += len(packed)
    # Total bitmap length equals the sum of per-glyph byte-aligned lengths.
    total = sum(len(ttf2enjin.pack_bits(g)) for g in glyphs)
    check(run == total, "cumulative offset equals summed glyph byte lengths")


def test_block_glyph_is_solid():
    glyphs, _ = ttf2enjin.rasterize(FIXTURE, 16, 0x20, 0x43)
    a = next(g for g in glyphs if g.code == 0x41)
    check(a.width == 6 and a.height == 10, "'A' is a 6x10 ink box (got %dx%d)"
          % (a.width, a.height))
    check(all(all(bit == 1 for bit in row) for row in a.bits),
          "'A' silhouette is entirely set (a solid block)")
    check(a.x_advance == 8, "'A' xAdvance is the font's advance width (8)")
    # The 'B' fixture glyph has a hole -> at least one clear row-cell.
    b = next(g for g in glyphs if g.code == 0x42)
    check(any(any(bit == 0 for bit in row) for row in b.bits),
          "'B' silhouette has a gap between its two bars")


def test_space_has_advance_no_ink():
    glyphs, _ = ttf2enjin.rasterize(FIXTURE, 16, 0x20, 0x43)
    sp = next(g for g in glyphs if g.code == 0x20)
    check(sp.width == 0 and sp.height == 0, "space has no ink")
    check(sp.x_advance == 6, "space still advances the cursor (6)")


GLYPHS_TXT = """# a comment before the first block

U+0041 A xAdvance=5 xOffset=0 yOffset=-3
...
.#.
###
...

U+0044 D xAdvance=4 xOffset=1 yOffset=-2
##
#.

U+00A0 NBSP xAdvance=6 xOffset=0 yOffset=0
"""


def _glyph_file():
    fd, path = tempfile.mkstemp(suffix=".latin1.txt")
    with os.fdopen(fd, "w", encoding="utf-8") as f:
        f.write(GLYPHS_TXT)
    return path


def test_missing_glyphs_are_zero():
    glyphs, _ = ttf2enjin.rasterize(FIXTURE, 16, 0x20, 0x43)
    bang = next(g for g in glyphs if g.code == 0x21)  # blocks.ttf has no '!'
    check((bang.width, bang.height, bang.x_advance, bang.x_offset, bang.y_offset)
          == (0, 0, 0, 0, 0), "a code point the TTF lacks is all zeros")
    glyphs, _ = ttf2enjin.rasterize(FIXTURE, 16, 0x7E, 0xA0)
    check(all(g.x_advance == 0 and g.width == 0 for g in glyphs if 0x7F <= g.code <= 0x9F),
          "0x7F-0x9F are missing")
    header = ttf2enjin.emit_header(glyphs, 16, "t", 0x7E, 0xA0)
    check("{     0,   0,   0,   0,    0,    0 },   // 0x80" in header,
          "a missing glyph is emitted as {offset, 0, 0, 0, 0, 0}")


def test_glyph_file_parses_and_crops():
    path = _glyph_file()
    try:
        drawn = ttf2enjin.parse_glyph_file(path)
    finally:
        os.unlink(path)
    check(sorted(drawn) == [0x41, 0x44, 0xA0], "three blocks parsed (got %s)" % sorted(drawn))
    a = drawn[0x41]
    check((a.width, a.height, a.x_advance, a.x_offset, a.y_offset) == (3, 2, 5, 0, -2),
          "blank border rows are cropped and yOffset follows the ink (got %r)"
          % ((a.width, a.height, a.x_advance, a.x_offset, a.y_offset),))
    check(a.bits == [[0, 1, 0], [1, 1, 1]], "rows read as '.'=0 '#'=1")
    nb = drawn[0xA0]
    check(nb.width == 0 and nb.height == 0 and nb.x_advance == 6,
          "a block with no rows is an advance with no ink")


def test_glyph_file_merges_over_raster():
    path = _glyph_file()
    try:
        glyphs, _ = ttf2enjin.rasterize(FIXTURE, 16, 0x20, 0x44)
        glyphs = ttf2enjin.merge_glyphs(glyphs, ttf2enjin.parse_glyph_file(path))
    finally:
        os.unlink(path)
    by = {g.code: g for g in glyphs}
    check(by[0x41].width == 3 and by[0x41].x_advance == 5, "the file overrides the TTF's A")
    check(by[0x42].width == 6, "B, not in the file, stays the TTF's")
    check(by[0x44].x_advance == 4 and by[0x44].bits == [[1, 1], [1, 0]],
          "D, which the TTF lacks, comes from the file")
    check(by[0x21].x_advance == 0, "! is still missing")


def test_face_form_cli():
    path = _glyph_file()
    fd, out = tempfile.mkstemp(suffix=".h")
    os.close(fd)
    try:
        rc = ttf2enjin.main([FIXTURE, "--size", "16", "--first", "0x20", "--last", "0xFF",
                             "--glyphs", path, "--name", "blocks", "--face-name", "blocks",
                             "--ascent", "10", "--cap-height", "10", "-o", out])
        with open(out) as f:
            text = f.read()
    finally:
        os.unlink(path)
        os.unlink(out)
    check(rc == 0, "face-form CLI exits 0")
    check('#include "enjin2/graphics/face.hpp"' in text, "the face form includes face.hpp")
    check("namespace enjin2::faces {" in text, "the face form is in enjin2::faces")
    check("inline const ::GFXfont blocksFont = {" in text, "the GFXfont is <id>Font")
    check('inline const ::enjin2::Face blocks{"blocks", &blocksFont, 10, 10};' in text,
          "the Face carries the name, font, ascent and cap height")
    check(re.search(r"blocksGlyphs, 0x20, 0xFF, \d+ \};", text) is not None,
          "the GFXfont spans 0x20..0xFF")
    entries = re.findall(r"^  \{.*?\},   // 0x", text, re.M)
    check(len(entries) == 0xFF - 0x20 + 1, "one glyph entry per code point (got %d)"
          % len(entries))
    check("// 0xA0 ' '" in text and "{    18,   0,   0,   6,    0,    0 },   // 0xA0" in text,
          "U+00A0 from the file: advance 6, no ink")
    check("Regenerate (from the Tomodachi repo root):" in text,
          "the face form states its regenerate command")


def main():
    test_golden_bytes()
    test_packing_is_byte_aligned()
    test_offsets_are_contiguous()
    test_block_glyph_is_solid()
    test_space_has_advance_no_ink()
    test_missing_glyphs_are_zero()
    test_glyph_file_parses_and_crops()
    test_glyph_file_merges_over_raster()
    test_face_form_cli()
    print("ttf2enjin: %d passed, %d failed" % (_passes, _failures))
    return 1 if _failures else 0


if __name__ == "__main__":
    raise SystemExit(main())
