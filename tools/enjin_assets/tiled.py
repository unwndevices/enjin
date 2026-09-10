"""Parse Tiled ``.tmx`` maps and ``.tsx`` tilesets (issue #87, decision #53).

Tiled is the map/attribute/collider authoring surface for purchased packs. This
module reads the XML into plain dataclasses; the CLI (``tiled2enjin.py``) turns
them into quantised tiles, a deduped tilebank, ``.njm`` cells, an ``ATTR`` chunk,
and a ColliderSet. We deliberately refuse the two forms the decision rules out —
**zstd-compressed layer data** and **infinite maps** — and materialise Tiled's
diagonal (90°) flip, which the 16-bit cell (H/V flips only) cannot express.

Cell GID flip flags (Tiled): H=0x80000000, V=0x40000000, D=0x20000000 (anti-
diagonal / transpose). The low 29 bits are the global tile id (0 = empty).
"""

from __future__ import annotations

import base64
import gzip
import os
import struct
import xml.etree.ElementTree as ET
import zlib
from dataclasses import dataclass, field

from .tilemap_attr import TileAttr, parse_dir  # local helper module

FLIP_H = 0x80000000
FLIP_V = 0x40000000
FLIP_D = 0x20000000
GID_MASK = 0x1FFFFFFF

# Collider kind vocabulary — mirrors ColliderKinds in core/colliders.hpp.
COLLIDER_KINDS = {"wall": 0, "bouncy": 1, "hazard": 2, "goal": 3, "flipper": 4}


@dataclass
class GidCell:
    """A decoded map cell: local tile id plus the three Tiled flip flags."""

    local_id: int  # -1 = empty
    hflip: bool = False
    vflip: bool = False
    dflip: bool = False

    @property
    def empty(self) -> bool:
        return self.local_id < 0


@dataclass
class TileLayer:
    name: str
    width: int
    height: int
    cells: list[GidCell]  # row-major, length width*height


@dataclass
class Collider:
    """A freeform collider extracted from a Tiled object (map pixel space)."""

    shape: str  # "aabb" | "circle" | "segments"
    kind: int = 0
    restitution: float = 0.0
    # aabb: (minx, miny, maxx, maxy); circle: (cx, cy, r); segments: flat [x0,y0,x1,y1,...]
    data: tuple = ()


@dataclass
class ObjectGroup:
    name: str
    colliders: list[Collider]


@dataclass
class Tileset:
    firstgid: int
    name: str
    tile_w: int
    tile_h: int
    columns: int
    spacing: int
    margin: int
    tilecount: int
    image_source: str  # absolute path to the sheet PNG
    image_w: int
    image_h: int
    attrs: dict  # local_id -> TileAttr (only non-default entries present)


@dataclass
class TiledMap:
    width: int
    height: int
    tile_w: int
    tile_h: int
    tilesets: list[Tileset]
    layers: list[TileLayer] = field(default_factory=list)
    object_groups: list[ObjectGroup] = field(default_factory=list)


# --- GID decode -----------------------------------------------------------

def _req_int(el, attr: str) -> int:
    """Read a required integer XML attribute, raising ValueError if it is missing."""
    raw = el.get(attr)
    if raw is None:
        raise ValueError(f"malformed Tiled file: <{el.tag}> is missing required attribute {attr!r}")
    try:
        return int(raw)
    except ValueError:
        raise ValueError(f"malformed Tiled file: {attr}={raw!r} is not an integer")


def decode_gid(raw: int, firstgid: int) -> GidCell:
    """Split a raw GID into a :class:`GidCell` (flags stripped, local id resolved)."""
    hflip = bool(raw & FLIP_H)
    vflip = bool(raw & FLIP_V)
    dflip = bool(raw & FLIP_D)
    gid = raw & GID_MASK
    if gid == 0:
        return GidCell(local_id=-1)
    return GidCell(local_id=gid - firstgid, hflip=hflip, vflip=vflip, dflip=dflip)


# --- layer data decode ----------------------------------------------------

def _decode_layer_data(data_el, width: int, height: int) -> list[int]:
    """Return the flat list of raw GIDs for a ``<data>`` element.

    Supports CSV and base64 (uncompressed / gzip / zlib). Refuses zstd.
    """
    encoding = data_el.get("encoding")
    compression = data_el.get("compression")
    if encoding == "csv":
        text = data_el.text or ""
        gids = [int(tok) for tok in text.replace("\n", "").split(",") if tok.strip() != ""]
    elif encoding == "base64":
        if compression == "zstd":
            raise ValueError("zstd-compressed layer data is not supported (decision #53)")
        raw = base64.b64decode((data_el.text or "").strip())
        if compression == "gzip":
            raw = gzip.decompress(raw)
        elif compression == "zlib":
            raw = zlib.decompress(raw)
        elif compression:
            raise ValueError(f"unsupported layer compression {compression!r}")
        gids = list(struct.unpack("<%dI" % (len(raw) // 4), raw))
    else:
        raise ValueError(f"unsupported layer encoding {encoding!r} (use csv or base64)")

    if len(gids) != width * height:
        raise ValueError(f"layer has {len(gids)} cells, expected {width * height}")
    return gids


# --- tileset (.tsx) -------------------------------------------------------

def _parse_tile_attr(tile_el) -> TileAttr:
    """Build a :class:`TileAttr` from a ``<tile>``'s class/type + properties."""
    attr = TileAttr()
    cls = tile_el.get("class") or tile_el.get("type")
    if cls:
        key = cls.strip().lower()
        # A recognised collider-kind name doubles as the tile's kind byte.
        if key in COLLIDER_KINDS:
            attr.kind = COLLIDER_KINDS[key]
        if key in ("wall", "solid"):
            attr.flags |= TileAttr.FLAG_SOLID
    props = tile_el.find("properties")
    if props is not None:
        for p in props.findall("property"):
            name = (p.get("name") or "").strip().lower()
            value = (p.get("value") or "").strip()
            if name == "solid":
                if value.lower() == "true":
                    attr.flags |= TileAttr.FLAG_SOLID
            elif name == "oneway":
                if value.lower() == "true":
                    attr.flags |= TileAttr.FLAG_ONEWAY
            elif name == "dir":
                attr.set_dir(parse_dir(value))
            elif name == "kind":
                try:
                    attr.kind = int(value) & 0xFF
                except ValueError:
                    if value.lower() in COLLIDER_KINDS:
                        attr.kind = COLLIDER_KINDS[value.lower()]
    return attr


def parse_tsx(path: str, firstgid: int = 1) -> Tileset:
    """Parse a standalone ``.tsx`` tileset file."""
    root = ET.parse(path).getroot()
    return _parse_tileset_element(root, firstgid, base_dir=os.path.dirname(os.path.abspath(path)))


def _parse_tileset_element(root, firstgid: int, base_dir: str) -> Tileset:
    tile_w = _req_int(root, "tilewidth")
    tile_h = _req_int(root, "tileheight")
    columns = int(root.get("columns", "0"))
    spacing = int(root.get("spacing", "0"))
    margin = int(root.get("margin", "0"))
    tilecount = int(root.get("tilecount", "0"))
    name = root.get("name", "tileset")

    image_el = root.find("image")
    if image_el is None:
        raise ValueError("tileset has no <image> (image-collection tilesets are unsupported)")
    image_source = os.path.normpath(os.path.join(base_dir, image_el.get("source")))
    image_w = int(image_el.get("width", "0"))
    image_h = int(image_el.get("height", "0"))

    # Embedded tilesets may omit `columns`; derive it from the sheet width.
    if columns <= 0 and image_w > 0 and tile_w > 0:
        columns = (image_w - 2 * margin + spacing) // (tile_w + spacing)

    attrs: dict = {}
    for tile_el in root.findall("tile"):
        local_id = int(tile_el.get("id"))
        attr = _parse_tile_attr(tile_el)
        if attr.flags or attr.kind:
            attrs[local_id] = attr

    return Tileset(
        firstgid=firstgid, name=name, tile_w=tile_w, tile_h=tile_h, columns=columns,
        spacing=spacing, margin=margin, tilecount=tilecount, image_source=image_source,
        image_w=image_w, image_h=image_h, attrs=attrs,
    )


# --- object groups → colliders --------------------------------------------

def _object_kind(obj_el) -> int:
    cls = (obj_el.get("class") or obj_el.get("type") or "").strip().lower()
    return COLLIDER_KINDS.get(cls, 0)


def _object_restitution(obj_el) -> float:
    props = obj_el.find("properties")
    if props is not None:
        for p in props.findall("property"):
            if (p.get("name") or "").strip().lower() == "restitution":
                try:
                    return float(p.get("value"))
                except (TypeError, ValueError):
                    return 0.0
    return 0.0


def _parse_object(obj_el) -> Collider | None:
    """Convert one Tiled ``<object>`` into a :class:`Collider` (or None to skip)."""
    x = float(obj_el.get("x", "0"))
    y = float(obj_el.get("y", "0"))
    kind = _object_kind(obj_el)
    rest = _object_restitution(obj_el)

    if obj_el.find("ellipse") is not None:
        w = float(obj_el.get("width", "0"))
        h = float(obj_el.get("height", "0"))
        r = (w + h) / 4.0  # mean radius
        return Collider("circle", kind, rest, (x + w / 2.0, y + h / 2.0, r))

    poly = obj_el.find("polygon")
    line = obj_el.find("polyline")
    if poly is not None or line is not None:
        pts_el = poly if poly is not None else line
        pts = _parse_points(pts_el.get("points", ""), x, y)
        if len(pts) < 4:
            return None
        if poly is not None and (pts[0], pts[1]) != (pts[-2], pts[-1]):
            pts = pts + [pts[0], pts[1]]  # close the polygon
        return Collider("segments", kind, rest, tuple(pts))

    if obj_el.find("point") is not None:
        return None  # points carry no geometry for the ColliderSet

    # Default: a rectangle object → AABB.
    w = float(obj_el.get("width", "0"))
    h = float(obj_el.get("height", "0"))
    if w <= 0 or h <= 0:
        return None
    return Collider("aabb", kind, rest, (x, y, x + w, y + h))


def _parse_points(points: str, ox: float, oy: float) -> list[float]:
    flat: list[float] = []
    for pair in points.split():
        if "," not in pair:
            continue
        px, py = pair.split(",")
        flat.append(ox + float(px))
        flat.append(oy + float(py))
    return flat


def _parse_object_group(group_el) -> ObjectGroup:
    colliders = []
    for obj_el in group_el.findall("object"):
        c = _parse_object(obj_el)
        if c is not None:
            colliders.append(c)
    return ObjectGroup(name=group_el.get("name", "objects"), colliders=colliders)


# --- map (.tmx) -----------------------------------------------------------

def parse_tmx(path: str) -> TiledMap:
    """Parse a Tiled ``.tmx`` map into a :class:`TiledMap`.

    Refuses non-orthogonal orientation and infinite maps.
    """
    base_dir = os.path.dirname(os.path.abspath(path))
    root = ET.parse(path).getroot()

    if root.get("orientation", "orthogonal") != "orthogonal":
        raise ValueError("only orthogonal maps are supported")
    if root.get("infinite", "0") == "1":
        raise ValueError("infinite maps are not supported (decision #53)")

    width = _req_int(root, "width")
    height = _req_int(root, "height")
    tile_w = _req_int(root, "tilewidth")
    tile_h = _req_int(root, "tileheight")

    tilesets: list[Tileset] = []
    for ts_el in root.findall("tileset"):
        firstgid = _req_int(ts_el, "firstgid")
        source = ts_el.get("source")
        if source:
            ts_path = os.path.normpath(os.path.join(base_dir, source))
            tilesets.append(parse_tsx(ts_path, firstgid))
        else:
            tilesets.append(_parse_tileset_element(ts_el, firstgid, base_dir))

    # We localise every gid against a single tileset's firstgid (our packs ship
    # one tileset per map). A second tileset would make gids above its firstgid
    # decode to the wrong local id — often silently in-range — so refuse it
    # rather than emit a corrupt map.
    if len(tilesets) > 1:
        raise ValueError(
            f"map references {len(tilesets)} tilesets; only single-tileset maps are supported"
        )
    firstgid0 = tilesets[0].firstgid if tilesets else 1

    layers: list[TileLayer] = []
    object_groups: list[ObjectGroup] = []
    # Preserve document order for correct bottom→top layer stacking.
    for el in root:
        if el.tag == "layer":
            lw = _req_int(el, "width")
            lh = _req_int(el, "height")
            data_el = el.find("data")
            if data_el is None:
                raise ValueError(f"tile layer {el.get('name')!r} has no <data>")
            gids = _decode_layer_data(data_el, lw, lh)
            cells = [decode_gid(g, firstgid0) for g in gids]
            layers.append(TileLayer(name=el.get("name", ""), width=lw, height=lh, cells=cells))
        elif el.tag == "objectgroup":
            object_groups.append(_parse_object_group(el))

    return TiledMap(
        width=width, height=height, tile_w=tile_w, tile_h=tile_h,
        tilesets=tilesets, layers=layers, object_groups=object_groups,
    )
