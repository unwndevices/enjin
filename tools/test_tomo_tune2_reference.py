#!/usr/bin/env python3
"""Reference-asset end-to-end test (Tomodachi #99).

Exercises the committed ``tomo_tune2.aseprite`` reference through the whole
layered path: parse the RGBA document, convert it with the explicit
``tomo_tune2.gpl`` target palette, encode the ``.njn`` v2 asset, decode it with
the shared codec, reconstruct every animation frame in source-layer painter
order, and compare the result pixel-for-pixel against frames exported by
Aseprite itself. This is the visual-parity half of the #99 integration test;
the C++ half loads the same committed ``.njn`` through the bounded per-applet
arena and the retained sprite runtime.

The golden PNGs under ``testdata/tomo_tune2_expected/`` were produced once from
the committed document with:

    aseprite -b tools/testdata/tomo_tune2.aseprite \\
        --save-as tools/testdata/tomo_tune2_expected/frame_{frame}.png

so they are Aseprite's own rendering, not this exporter's.
"""

import functools
import os
import sys

import pytest

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import aseprite2enjin as a2e  # noqa: E402
from enjin_assets import emit, palette as palette_mod  # noqa: E402

try:
    from PIL import Image
except ImportError:  # pragma: no cover - Pillow is a hard dev dependency
    Image = None

TESTDATA = os.path.join(os.path.dirname(os.path.abspath(__file__)), 'testdata')
ASEPRITE = os.path.join(TESTDATA, 'tomo_tune2.aseprite')
PALETTE = os.path.join(TESTDATA, 'tomo_tune2.gpl')
GOLDEN_DIR = os.path.join(TESTDATA, 'tomo_tune2_expected')
COMMITTED_NJN = os.path.join(TESTDATA, 'tomo_tune2.njn')

CANVAS_W, CANVAS_H = 96, 110
NUM_FRAMES = 23
PARTS = ['Layer 1', 'Detail', 'Vaso', 'Bunny', 'Gambo', 'Fiore (3)', 'Sparkles']
SPARKLE_PART = 6
# ``Sparkles`` is absent from the document in these frames and painted in the
# rest: the intermittent detail the reference asset is chosen to cover.
SPARKLE_INVISIBLE = [0, 1, 2, 3, 4, 5, 6, 16, 17, 18, 19, 20, 21, 22]
TRANSPARENT = 15


@functools.lru_cache(maxsize=None)
def _palette_rgb():
    loaded = palette_mod.load_gpl(PALETTE)
    return [tuple(int(channel) for channel in rgb) for rgb in loaded.rgb.tolist()]


@functools.lru_cache(maxsize=None)
def _convert():
    """Convert the committed fixture and round-trip it through the codec."""
    parsed = a2e.parse_aseprite_layered(ASEPRITE)
    rgb = _palette_rgb()
    asset, summary = a2e.build_layered_asset(parsed, rgb)
    data = emit.build_njn_layered(asset)
    return summary, data, emit.parse_njn_layered(data)


def _reconstruct(asset, frame, rgb):
    """Rebuild one frame with the runtime's painter rule (index 15 skipped)."""
    width, height = asset.canvas_w, asset.canvas_h
    rgba = [(r, g, b, 255) for (r, g, b) in rgb] + [(0, 0, 0, 0)]
    canvas = [[(0, 0, 0, 0)] * width for _ in range(height)]
    num_parts = len(asset.parts)
    for part in range(num_parts):
        image_index, origin_x, origin_y = asset.refs[frame * num_parts + part]
        if image_index == emit.LAYERED_INVISIBLE:
            continue
        image = asset.images[image_index]
        for row in range(image.h):
            canvas_y = origin_y + row
            if not 0 <= canvas_y < height:
                continue
            base = row * image.w
            for col in range(image.w):
                value = image.pixels[base + col]
                if value == TRANSPARENT:
                    continue
                canvas_x = origin_x + col
                if 0 <= canvas_x < width:
                    canvas[canvas_y][canvas_x] = rgba[value]
    return [pixel for row in canvas for pixel in row]


def _aseprite_frame(frame):
    if Image is None:  # pragma: no cover
        pytest.skip('Pillow is required for the Aseprite golden comparison')
    path = os.path.join(GOLDEN_DIR, f'frame_{frame}.png')
    image = Image.open(path).convert('RGBA')
    assert image.size == (CANVAS_W, CANVAS_H), f'golden {path} has wrong extent'
    raw = image.tobytes()
    return [tuple(raw[i:i + 4]) for i in range(0, len(raw), 4)]


# ---------------------------------------------------------------------------
# Conversion shape + reproducibility
# ---------------------------------------------------------------------------

def test_reference_conversion_reproduces_the_committed_asset():
    _summary, data, _decoded = _convert()
    with open(COMMITTED_NJN, 'rb') as handle:
        committed = handle.read()
    assert data == committed, (
        'committed tomo_tune2.njn is stale: re-run '
        'aseprite2enjin.py --layered --palette tools/testdata/tomo_tune2.gpl'
    )


def test_reference_structure():
    summary, _data, out = _convert()

    assert (out.canvas_w, out.canvas_h) == (CANVAS_W, CANVAS_H)
    assert out.parts == PARTS
    assert len(out.durations) == NUM_FRAMES
    assert set(out.durations) == {100}
    assert len(out.images) == 29
    assert len(out.refs) == NUM_FRAMES * len(PARTS)

    assert len(out.clips) == 1
    clip = out.clips[0]
    assert clip.name == 'default'
    assert clip.loop_mode == emit.LOOP_LOOP
    assert [entry[0] for entry in clip.frames] == list(range(NUM_FRAMES))

    # ADR-0004/0005 reference measurement: 72,044 unique part pixels.
    assert summary['num_images'] == 29
    assert summary['pool_pixels'] == 72044
    assert summary['ignored_layers'] == []


def test_sparkle_part_is_intermittent():
    _summary, _data, out = _convert()

    for frame in range(NUM_FRAMES):
        ref = out.refs[frame * len(PARTS) + SPARKLE_PART]
        invisible = ref[0] == emit.LAYERED_INVISIBLE
        assert invisible == (frame in SPARKLE_INVISIBLE), f'frame {frame}'


# ---------------------------------------------------------------------------
# Pixel parity against Aseprite
# ---------------------------------------------------------------------------

@pytest.mark.parametrize('frame', range(NUM_FRAMES))
def test_every_frame_matches_aseprite(frame):
    _summary, _data, out = _convert()
    rgb = _palette_rgb()

    assert _reconstruct(out, frame, rgb) == _aseprite_frame(frame), (
        f'frame {frame} differs from the Aseprite export '
        '(painter order, offsets, clipping or sparkle visibility drifted)'
    )


if __name__ == '__main__':
    sys.exit(pytest.main([__file__, '-v']))
