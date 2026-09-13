"""Tests for png slicing and tilebank flip-dedupe (issue #87)."""

import os
import sys

import numpy as np

sys.path.insert(0, os.path.dirname(os.path.dirname(os.path.dirname(os.path.abspath(__file__)))))

from enjin_assets import png, tilebank  # noqa: E402
from enjin_assets.palette import TRANSPARENT_INDEX  # noqa: E402
from enjin_assets.tilebank import _flip  # noqa: E402


# --- png.slice_sheet ------------------------------------------------------

def _sheet_with_spacing(tile_w, tile_h, cols, rows, spacing, margin):
    """Build an RGBA sheet where tile (r,c) is filled with red value = index."""
    w = margin * 2 + cols * tile_w + (cols - 1) * spacing
    h = margin * 2 + rows * tile_h + (rows - 1) * spacing
    sheet = np.zeros((h, w, 4), dtype=np.uint8)
    for r in range(rows):
        for c in range(cols):
            idx = r * cols + c
            x0 = margin + c * (tile_w + spacing)
            y0 = margin + r * (tile_h + spacing)
            sheet[y0:y0 + tile_h, x0:x0 + tile_w] = (idx + 1, 0, 0, 255)
    return sheet


def test_slice_sheet_respects_spacing_and_margin():
    sheet = _sheet_with_spacing(4, 4, 3, 2, spacing=1, margin=2)
    tiles = png.slice_sheet(sheet, 4, 4, columns=3, count=6, margin=2, spacing=1)
    assert len(tiles) == 6
    for i, t in enumerate(tiles):
        assert t.shape == (4, 4, 4)
        assert (t[..., 0] == i + 1).all(), f"tile {i} picked up the wrong region"
        assert (t[..., 3] == 255).all()


def test_slice_sheet_matches_kenney_geometry():
    # Kenney tilemap.png: 203x186, 16px tiles, spacing 1, 12 cols → 132 tiles.
    sheet = np.zeros((186, 203, 4), dtype=np.uint8)
    tiles = png.slice_sheet(sheet, 16, 16, columns=12, count=132, spacing=1)
    assert len(tiles) == 132
    assert all(t.shape == (16, 16, 4) for t in tiles)


def test_slice_sheet_pads_off_edge_tiles_transparent():
    sheet = np.full((16, 16, 4), 255, dtype=np.uint8)
    # Ask for 4 tiles from a 1-tile sheet; tiles 1..3 fall off → transparent.
    tiles = png.slice_sheet(sheet, 16, 16, columns=2, count=4)
    assert (tiles[0] == 255).all()
    assert (tiles[1] == 0).all()  # column 1 is off the 16px-wide sheet


# --- tilebank -------------------------------------------------------------

def _tile(vals):
    return np.array(vals, dtype=np.uint8)


def test_id0_is_transparent():
    bank = tilebank.TileBank(2, 2)
    assert bank.count() == 1
    empty = _tile([[TRANSPARENT_INDEX, TRANSPARENT_INDEX], [TRANSPARENT_INDEX, TRANSPARENT_INDEX]])
    tid, h, v = bank.add(empty)
    assert (tid, h, v) == (0, False, False)
    assert bank.count() == 1  # no new id


def test_distinct_tiles_get_distinct_ids():
    bank = tilebank.TileBank(2, 2)
    a = _tile([[1, 2], [3, 4]])
    b = _tile([[5, 6], [7, 8]])
    assert bank.add(a) == (1, False, False)
    assert bank.add(b) == (2, False, False)
    assert bank.count() == 3


def test_hflip_dedupes_and_returns_flags():
    bank = tilebank.TileBank(2, 2)
    a = _tile([[1, 2], [3, 4]])
    assert bank.add(a) == (1, False, False)
    # The horizontal mirror must reuse id 1 with hflip set.
    tid, h, v = bank.add(_flip(a, True, False))
    assert (tid, h, v) == (1, True, False)
    assert bank.count() == 2  # no new tile stored


def test_vflip_and_hvflip_dedupe():
    bank = tilebank.TileBank(2, 2)
    a = _tile([[1, 2], [3, 4]])
    bank.add(a)
    assert bank.add(_flip(a, False, True)) == (1, False, True)
    assert bank.add(_flip(a, True, True)) == (1, True, True)
    assert bank.count() == 2


def test_flip_flags_actually_reproduce_appearance():
    # The whole point: drawing the canonical with the returned flags == request.
    bank = tilebank.TileBank(2, 3)
    a = _tile([[1, 2], [3, 4], [5, 6]])
    bank.add(a)
    want = _flip(a, True, False)
    tid, h, v = bank.add(want)
    canonical = np.frombuffer(bank.pixels(), dtype=np.uint8)[tid * 6:(tid + 1) * 6].reshape(3, 2)
    assert np.array_equal(_flip(canonical, h, v), want)


def test_attributes_keep_flip_equivalent_tiles_distinct():
    bank = tilebank.TileBank(2, 2)
    a = _tile([[1, 2], [3, 4]])
    assert bank.add(a, attr=(0, 0)) == (1, False, False)
    # Same pixels, different attribute → a separate id (attrs are part of the key).
    tid, _, _ = bank.add(a, attr=(1, 5))
    assert tid == 2
    assert bank.attrs()[2] == (1, 5)
    assert bank.has_attrs()


def test_dedupe_yield_and_pixels_length():
    bank = tilebank.TileBank(2, 2)
    a = _tile([[1, 2], [3, 4]])
    bank.add(a)
    bank.add(_flip(a, True, False))  # dedupes
    bank.add(_tile([[9, 9], [9, 9]]))
    unique, seen = bank.dedupe_yield()
    assert (unique, seen) == (2, 3)
    assert len(bank.pixels()) == bank.count() * 4  # 2x2 tiles


def test_exceeding_tile_budget_raises():
    bank = tilebank.TileBank(1, 1)
    # Force >511 distinct ids by varying the attribute tuple (both bytes) so each
    # add is a new key; the 512-id (9-bit) budget must raise.
    try:
        for k in range(600):
            bank.add(_tile([[0]]), attr=((k >> 8) & 0xFF, k & 0xFF))
    except ValueError as e:
        assert "budget" in str(e)
        return
    assert False, "expected the 512-id budget to raise"
