"""Tests for the Tiled .tmx/.tsx parser (issue #87)."""

import os
import sys

sys.path.insert(0, os.path.dirname(os.path.dirname(os.path.dirname(os.path.abspath(__file__)))))

from enjin_assets import tiled  # noqa: E402
from enjin_assets.tilemap_attr import TileAttr  # noqa: E402

# The committed Kenney pack (assets/third-party/kenney-tiny-dungeon).
_REPO = os.path.dirname(  # .../libs/enjin/tools -> repo? compute from this file
    os.path.dirname(os.path.dirname(os.path.dirname(os.path.dirname(os.path.abspath(__file__)))))
)
KENNEY = os.path.join(_REPO, "assets", "third-party", "kenney-tiny-dungeon")
SAMPLE_TMX = os.path.join(KENNEY, "Tiled", "sampleMap.tmx")
SAMPLE_TSX = os.path.join(KENNEY, "Tiled", "sampleSheet.tsx")

_have_kenney = os.path.isfile(SAMPLE_TMX)


# --- GID decode -----------------------------------------------------------

def test_decode_gid_strips_flags():
    # 1610612787 = 0x60000033 = V|D flags on global gid 51; firstgid 1 → local 50.
    c = tiled.decode_gid(1610612787, firstgid=1)
    assert (c.local_id, c.hflip, c.vflip, c.dflip) == (50, False, True, True)


def test_decode_gid_empty_and_plain():
    assert tiled.decode_gid(0, 1).empty
    c = tiled.decode_gid(14, 1)
    assert (c.local_id, c.hflip, c.vflip, c.dflip) == (13, False, False, False)


# --- real Kenney sample ---------------------------------------------------

def test_parse_kenney_sample_tmx():
    if not _have_kenney:
        return
    m = tiled.parse_tmx(SAMPLE_TMX)
    assert (m.width, m.height) == (32, 20)
    assert (m.tile_w, m.tile_h) == (16, 16)
    assert [layer.name for layer in m.layers] == ["Dungeon", "Objects", "Carts"]
    assert all(len(layer.cells) == 32 * 20 for layer in m.layers)
    assert len(m.tilesets) == 1
    ts = m.tilesets[0]
    assert ts.tilecount == 132 and ts.columns == 12 and ts.spacing == 1
    assert ts.tile_w == 16 and ts.tile_h == 16
    assert os.path.basename(ts.image_source) == "tilemap.png"
    # Kenney sample authors no per-tile attributes.
    assert ts.attrs == {}


def test_parse_kenney_tsx_directly():
    if not _have_kenney:
        return
    ts = tiled.parse_tsx(SAMPLE_TSX, firstgid=1)
    assert ts.tilecount == 132
    assert os.path.isfile(ts.image_source)


# --- synthetic TMX: attributes + colliders + refusals ---------------------

def _write(tmp_path, name, text):
    p = tmp_path / name
    p.write_text(text)
    return str(p)


def _tsx_with_attrs():
    return """<?xml version="1.0" encoding="UTF-8"?>
<tileset version="1.8" name="t" tilewidth="16" tileheight="16" spacing="0" columns="4" tilecount="8">
 <image source="sheet.png" width="64" height="32"/>
 <tile id="2" class="wall"/>
 <tile id="3">
  <properties>
   <property name="oneway" type="bool" value="true"/>
   <property name="dir" value="down"/>
   <property name="kind" type="int" value="9"/>
  </properties>
 </tile>
</tileset>
"""


def test_tsx_attributes(tmp_path):
    path = _write(tmp_path, "sheet.tsx", _tsx_with_attrs())
    ts = tiled.parse_tsx(path, firstgid=1)
    # id2: class "wall" → SOLID + kind Wall(0).
    a2 = ts.attrs[2]
    assert a2.solid and a2.kind == 0
    # id3: ONEWAY + dir=down(2) + kind 9.
    a3 = ts.attrs[3]
    assert a3.oneway and a3.dir == 2 and a3.kind == 9
    # Tiles without attrs are absent.
    assert 0 not in ts.attrs and 1 not in ts.attrs


def _tmx_with_objects():
    return """<?xml version="1.0" encoding="UTF-8"?>
<map version="1.8" orientation="orthogonal" infinite="0" width="2" height="2" tilewidth="16" tileheight="16">
 <tileset firstgid="1" source="sheet.tsx"/>
 <layer id="1" name="base" width="2" height="2">
  <data encoding="csv">1,2,0,3</data>
 </layer>
 <objectgroup name="collision">
  <object id="1" x="10" y="20" width="30" height="40"/>
  <object id="2" class="bouncy" x="5" y="5" width="10" height="10"><ellipse/></object>
  <object id="3" class="hazard" x="0" y="0"><polygon points="0,0 8,0 8,8"/>
   <properties><property name="restitution" value="0.5"/></properties>
  </object>
 </objectgroup>
</map>
"""


def test_tmx_objects_to_colliders(tmp_path):
    _write(tmp_path, "sheet.tsx", _tsx_with_attrs())
    path = _write(tmp_path, "map.tmx", _tmx_with_objects())
    m = tiled.parse_tmx(path)
    assert len(m.layers) == 1
    assert len(m.object_groups) == 1
    cols = m.object_groups[0].colliders
    kinds = {c.shape: c for c in cols}
    assert kinds["aabb"].data == (10.0, 20.0, 40.0, 60.0) and kinds["aabb"].kind == 0
    assert kinds["circle"].kind == 1  # bouncy
    cx, cy, r = kinds["circle"].data
    assert (cx, cy, r) == (10.0, 10.0, 5.0)
    seg = kinds["segments"]
    assert seg.kind == 2 and seg.restitution == 0.5  # hazard
    # polygon is closed: last point == first point.
    assert seg.data[:2] == seg.data[-2:]


def test_layer_cell_flags_from_csv(tmp_path):
    _write(tmp_path, "sheet.tsx", _tsx_with_attrs())
    path = _write(tmp_path, "map.tmx", _tmx_with_objects())
    m = tiled.parse_tmx(path)
    cells = m.layers[0].cells
    assert cells[0].local_id == 0  # gid 1 - firstgid 1
    assert cells[1].local_id == 1  # gid 2
    assert cells[2].empty          # gid 0
    assert cells[3].local_id == 2  # gid 3


def test_refuse_infinite_map(tmp_path):
    _write(tmp_path, "sheet.tsx", _tsx_with_attrs())
    text = _tmx_with_objects().replace('infinite="0"', 'infinite="1"')
    path = _write(tmp_path, "inf.tmx", text)
    try:
        tiled.parse_tmx(path)
        assert False, "expected refusal"
    except ValueError as e:
        assert "infinite" in str(e).lower()


def test_refuse_multi_tileset(tmp_path):
    _write(tmp_path, "sheet.tsx", _tsx_with_attrs())
    text = _tmx_with_objects().replace(
        '<tileset firstgid="1" source="sheet.tsx"/>',
        '<tileset firstgid="1" source="sheet.tsx"/>\n <tileset firstgid="50" source="sheet.tsx"/>',
    )
    path = _write(tmp_path, "multi.tmx", text)
    try:
        tiled.parse_tmx(path)
        assert False, "expected refusal"
    except ValueError as e:
        assert "tileset" in str(e).lower()


def test_embedded_tileset_derives_columns(tmp_path):
    # Embedded tileset with no `columns`; derive from image width 64 / 16 = 4.
    tmx = """<?xml version="1.0" encoding="UTF-8"?>
<map version="1.8" orientation="orthogonal" infinite="0" width="1" height="1" tilewidth="16" tileheight="16">
 <tileset firstgid="1" name="t" tilewidth="16" tileheight="16" tilecount="8">
  <image source="sheet.png" width="64" height="32"/>
 </tileset>
 <layer id="1" name="base" width="1" height="1"><data encoding="csv">1</data></layer>
</map>
"""
    path = _write(tmp_path, "embed.tmx", tmx)
    m = tiled.parse_tmx(path)
    assert m.tilesets[0].columns == 4


def test_malformed_map_raises_valueerror(tmp_path):
    # Missing required `width` → a clean ValueError (not a bare TypeError).
    _write(tmp_path, "sheet.tsx", _tsx_with_attrs())
    text = _tmx_with_objects().replace('width="2" height="2"', 'height="2"', 1)
    path = _write(tmp_path, "bad.tmx", text)
    try:
        tiled.parse_tmx(path)
        assert False, "expected ValueError"
    except ValueError as e:
        assert "missing required attribute" in str(e)


def test_refuse_zstd(tmp_path):
    data_el = '<data encoding="base64" compression="zstd">AAAA</data>'
    text = _tmx_with_objects().replace(
        '<data encoding="csv">1,2,0,3</data>', data_el
    )
    _write(tmp_path, "sheet.tsx", _tsx_with_attrs())
    path = _write(tmp_path, "zstd.tmx", text)
    try:
        tiled.parse_tmx(path)
        assert False, "expected refusal"
    except ValueError as e:
        assert "zstd" in str(e).lower()
