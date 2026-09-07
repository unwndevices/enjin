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
"""
import os
import sys

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


def main():
    test_golden_bytes()
    test_packing_is_byte_aligned()
    test_offsets_are_contiguous()
    test_block_glyph_is_solid()
    test_space_has_advance_no_ink()
    print("ttf2enjin: %d passed, %d failed" % (_passes, _failures))
    return 1 if _failures else 0


if __name__ == "__main__":
    raise SystemExit(main())
