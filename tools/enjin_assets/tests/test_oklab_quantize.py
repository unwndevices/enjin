"""Tests for oklab, palette, and quantize (issue #87)."""

import os
import sys

import numpy as np

sys.path.insert(0, os.path.dirname(os.path.dirname(os.path.dirname(os.path.abspath(__file__)))))

from enjin_assets import oklab, palette, quantize  # noqa: E402


# --- oklab ----------------------------------------------------------------

def test_oklab_reference_points():
    # Ottosson's reference: white → L≈1, a≈0, b≈0; black → all 0.
    white = oklab.srgb_to_oklab(np.array([255, 255, 255]))
    assert np.allclose(white, [1.0, 0.0, 0.0], atol=1e-3)
    black = oklab.srgb_to_oklab(np.array([0, 0, 0]))
    assert np.allclose(black, [0.0, 0.0, 0.0], atol=1e-6)


def test_delta_e_is_zero_for_identical_and_symmetric():
    a = oklab.srgb_to_oklab(np.array([239, 125, 87]))
    b = oklab.srgb_to_oklab(np.array([65, 166, 246]))
    assert oklab.delta_e(a, a) == 0.0
    assert np.isclose(oklab.delta_e(a, b), oklab.delta_e(b, a))
    assert oklab.delta_e(a, b) > 0.1  # distinct colours are far apart


def test_delta_e_broadcasts_over_palette():
    pal = palette.load_default()
    px = oklab.srgb_to_oklab(np.array([[239, 125, 87], [26, 28, 44]]))  # (2,3)
    d = oklab.delta_e(px[:, None, :], pal.oklab[None, :, :])
    assert d.shape == (2, 15)


# --- palette --------------------------------------------------------------

def test_load_default_palette():
    pal = palette.load_default()
    assert pal.name == "house-default"
    assert pal.rgb.shape == (15, 3)
    assert pal.oklab.shape == (15, 3)
    # Index 0 is the bg role #081a1c, index 3 the text role #e8f4cc.
    assert tuple(pal.rgb[0]) == (0x08, 0x1A, 0x1C)
    assert tuple(pal.rgb[3]) == (0xE8, 0xF4, 0xCC)
    # Index 7 is the accent4 role, the house default's red #e84840.
    assert tuple(pal.rgb[7]) == (0xE8, 0x48, 0x40)


def test_load_gpl_rejects_short_palette(tmp_path):
    p = tmp_path / "short.gpl"
    p.write_text("GIMP Palette\nName: short\n#\n255 0 0\n0 255 0\n")
    try:
        palette.load_gpl(str(p))
        assert False, "expected ValueError"
    except ValueError:
        pass


# --- quantize -------------------------------------------------------------

def test_exact_palette_colours_map_to_their_index():
    pal = palette.load_default()
    # Feed the 8 role colours as opaque pixels; each must map to itself. (The
    # temporary entries 8-14 repeat some of them, so the first index wins.)
    rgba = np.concatenate([pal.rgb[:8], np.full((8, 1), 255, dtype=np.uint8)], axis=1)
    res = quantize.quantize_rgba(rgba, pal)
    assert list(res.indices) == list(range(8))
    assert np.allclose(res.delta_e, 0.0, atol=1e-9)


def test_transparent_pixels_map_to_index_15():
    pal = palette.load_default()
    rgba = np.array(
        [
            [232, 72, 64, 0],    # accent4 red but fully transparent → 15
            [232, 72, 64, 128],  # exactly at threshold → transparent
            [232, 72, 64, 129],  # just above threshold → accent4 (7)
        ],
        dtype=np.uint8,
    )
    res = quantize.quantize_rgba(rgba, pal)
    assert list(res.indices) == [15, 15, 7]
    # Only one opaque pixel contributed to the error list.
    assert res.delta_e.shape == (1,)


def test_near_colour_snaps_to_nearest_entry():
    pal = palette.load_default()
    # A slightly-off red should still land on index 7 (accent4).
    rgba = np.array([[228, 76, 70, 255]], dtype=np.uint8)
    res = quantize.quantize_rgba(rgba, pal)
    assert res.indices[0] == 7
    assert res.delta_e[0] < 0.05  # close


def test_quantize_preserves_leading_shape():
    pal = palette.load_default()
    rgba = np.zeros((4, 5, 4), dtype=np.uint8)
    rgba[..., 3] = 255  # opaque black
    res = quantize.quantize_rgba(rgba, pal)
    assert res.indices.shape == (4, 5)


def test_histogram_reports_stats():
    de = np.array([0.0, 0.01, 0.02, 0.03, 0.1])
    out = quantize.histogram(de)
    assert "ΔEOK" in out
    assert "n=5" in out
    assert quantize.histogram(np.zeros(0)).startswith("ΔEOK: (no opaque")
