"""End-to-end pipeline tests (issue #87), incl. the real Kenney sample."""

import os
import sys

import numpy as np

sys.path.insert(0, os.path.dirname(os.path.dirname(os.path.dirname(os.path.abspath(__file__)))))

from enjin_assets import emit, palette, pipeline, tiled  # noqa: E402

_REPO = os.path.dirname(
    os.path.dirname(os.path.dirname(os.path.dirname(os.path.dirname(os.path.abspath(__file__)))))
)
SAMPLE_TMX = os.path.join(_REPO, "assets", "third-party", "kenney-tiny-dungeon", "Tiled", "sampleMap.tmx")
_have = os.path.isfile(SAMPLE_TMX)


# --- orient ---------------------------------------------------------------

def test_orient_transpose_needs_square():
    t = np.arange(6, dtype=np.uint8).reshape(2, 3)
    try:
        pipeline.orient(t, False, False, True)
        assert False
    except ValueError:
        pass
    sq = np.arange(4, dtype=np.uint8).reshape(2, 2)
    assert np.array_equal(pipeline.orient(sq, False, False, True), sq.T)


# --- Kenney round-trip ----------------------------------------------------

def test_kenney_sample_round_trips():
    if not _have:
        return
    pal = palette.load_default()
    m = tiled.parse_tmx(SAMPLE_TMX)
    res = pipeline.convert_tiled(m, pal)

    # .njn parses under the C++-mirror reader; ATTR absent (Kenney authors none).
    parsed = emit.parse_njn(res.njn)
    assert parsed.version == 2
    assert emit.CHUNK_PIXL in parsed.chunks
    assert emit.CHUNK_ATTR not in parsed.chunks

    # .njm decodes to the right dimensions.
    mw, mh, cells = pipeline.parse_njm(res.njm)
    assert (mw, mh) == (32, 20)
    assert len(cells) == 32 * 20

    # The sample uses Tiled 90° flags → some tiles were materialised.
    assert res.materialised_d > 0
    # Dedupe actually collapsed appearances into fewer ids.
    assert res.unique_tiles > 0
    assert res.unique_tiles < res.seen_appearances
    # Quantise produced error samples within the palette's reach.
    assert res.delta_e.size > 0
    assert res.delta_e.mean() < 0.12

    # Preview reconstructs an image of the map's pixel size.
    img = pipeline.render_preview(res.njn, res.njm, pal)
    assert img.shape == (20 * 16, 32 * 16, 3)
    assert img.max() > 0  # not all black

    # No freeform colliders in the Kenney sample (tile layers only).
    assert res.colliders_lua is None


def test_report_mentions_dedupe_and_deok():
    if not _have:
        return
    pal = palette.load_default()
    res = pipeline.convert_tiled(tiled.parse_tmx(SAMPLE_TMX), pal)
    text = res.report()
    assert "unique ids" in text and "ΔEOK" in text


# --- colliders via synthetic map ------------------------------------------

def _synthetic_map_with_objects(tmp_path):
    tsx = """<?xml version="1.0" encoding="UTF-8"?>
<tileset version="1.8" name="t" tilewidth="2" tileheight="2" columns="2" tilecount="4">
 <image source="sheet.png" width="4" height="4"/>
</tileset>
"""
    tmx = """<?xml version="1.0" encoding="UTF-8"?>
<map version="1.8" orientation="orthogonal" infinite="0" width="2" height="1" tilewidth="2" tileheight="2">
 <tileset firstgid="1" source="sheet.tsx"/>
 <layer id="1" name="base" width="2" height="1"><data encoding="csv">1,2</data></layer>
 <objectgroup name="collision">
  <object id="1" class="wall" x="0" y="0" width="4" height="1"/>
  <object id="2" class="bouncy" x="1" y="1"><ellipse width="2" height="2"/></object>
 </objectgroup>
</map>
"""
    (tmp_path / "sheet.tsx").write_text(tsx)
    # 4x4 RGBA sheet: 4 tiles of 2x2, distinct colours.
    from PIL import Image
    arr = np.zeros((4, 4, 4), dtype=np.uint8)
    arr[0:2, 0:2] = (239, 125, 87, 255)   # tile 0 orange
    arr[0:2, 2:4] = (65, 166, 246, 255)   # tile 1 blue
    Image.fromarray(arr, "RGBA").save(tmp_path / "sheet.png")
    (tmp_path / "map.tmx").write_text(tmx)
    return str(tmp_path / "map.tmx")


def test_synthetic_map_emits_colliders(tmp_path):
    pal = palette.load_default()
    path = _synthetic_map_with_objects(tmp_path)
    res = pipeline.convert_tiled(tiled.parse_tmx(path), pal)
    assert res.colliders_lua is not None
    assert "segments" in res.colliders_lua
    assert "aabbs" in res.colliders_lua
    assert "circles" in res.colliders_lua
    # The wall AABB spans 0,0..4,1; the bouncy circle has kind 1.
    assert "0, 0, 4, 1, 0, 0" in res.colliders_lua
    mw, mh, cells = pipeline.parse_njm(res.njm)
    assert (mw, mh) == (2, 1)
