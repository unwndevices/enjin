"""End-to-end Tiled→enjin conversion: the shared core behind ``tiled2enjin.py``.

Ties the modules together: parse the ``.tmx``, quantise the tileset PNG, orient
and flip-dedupe every used tile (materialising Tiled's 90° flag), pack ``.njm``
cells, build the ``.njn`` v2 tileset (``PIXL`` + ``ATTR``), and gather object-layer
colliders into a ColliderSet Lua sidecar. Kept import-friendly (no argv/exit) so
tests drive it directly on the committed Kenney sample.
"""

from __future__ import annotations

from dataclasses import dataclass, field

import numpy as np

from . import emit, png, quantize
from .palette import TRANSPARENT_INDEX, Palette
from .tilebank import TileBank
from .tiled import TiledMap
from .tilemap_attr import TileAttr


def orient(tile: np.ndarray, hflip: bool, vflip: bool, dflip: bool) -> np.ndarray:
    """Apply Tiled flip flags to a tile, yielding its on-screen appearance.

    Tiled's diagonal flag is an anti-diagonal reflection (transpose), applied
    before the horizontal/vertical mirrors; it needs a square tile. Together the
    three flags span the 8-orientation dihedral group — the 90° rotations that
    the 16-bit cell (H/V only) cannot store, so those tiles are materialised as
    distinct appearances here and land as fresh ids in the bank.
    """
    t = tile
    if dflip:
        if t.shape[0] != t.shape[1]:
            raise ValueError("diagonal (90°) flip needs a square tile to materialise")
        t = t.T
    if hflip:
        t = np.fliplr(t)
    if vflip:
        t = np.flipud(t)
    return np.ascontiguousarray(t)


@dataclass
class ConvertResult:
    njn: bytes
    njm: bytes
    colliders_lua: str | None
    map_w: int
    map_h: int
    unique_tiles: int
    seen_appearances: int
    materialised_d: int
    used_source_tiles: int
    delta_e: np.ndarray = field(default_factory=lambda: np.zeros(0))
    warnings: list[str] = field(default_factory=list)

    def report(self) -> str:
        """Human-readable summary for the CLI (dedupe yield + ΔEOK histogram)."""
        lines = [
            f"map           : {self.map_w}x{self.map_h} cells",
            f"tiles         : {self.unique_tiles} unique ids (+id0) from "
            f"{self.seen_appearances} placed appearances "
            f"({self.used_source_tiles} distinct source tiles used)",
            f"D-materialised: {self.materialised_d} rotated tiles baked "
            f"(Tiled 90° flag, inexpressible in the cell)",
            "ΔEOK quantise :",
            quantize.histogram(self.delta_e),
        ]
        for w in self.warnings:
            lines.append(f"warning: {w}")
        return "\n".join(lines)


def _flatten(tmap: TiledMap):
    """Choose one ``(GidCell, band)`` per map cell (top-most non-empty wins).

    Band 0 (under → L0) for the bottom layer, band 1 (over → L2) for any upper
    layer. Yields ``(index, cell_or_None, band)`` for every map cell, row-major.
    """
    n = tmap.width * tmap.height
    for i in range(n):
        chosen = None
        band = 0
        for li in range(len(tmap.layers) - 1, -1, -1):
            cell = tmap.layers[li].cells[i]
            if not cell.empty:
                chosen = cell
                band = 0 if li == 0 else 1
                break
        yield i, chosen, band


def convert_tiled(tmap: TiledMap, palette: Palette, *, strict: bool = False) -> ConvertResult:
    """Convert a parsed :class:`TiledMap` into ``.njn`` + ``.njm`` bytes + colliders."""
    if not tmap.tilesets:
        raise ValueError("map has no tileset")
    ts = tmap.tilesets[0]
    sheet = png.load_rgba(ts.image_source)
    src_tiles = png.slice_sheet(
        sheet, ts.tile_w, ts.tile_h, columns=ts.columns, count=ts.tilecount,
        margin=ts.margin, spacing=ts.spacing,
    )

    # Quantise each source tile once; cache the index grid and per-tile error.
    quant_cache: dict[int, np.ndarray] = {}
    de_by_tile: dict[int, np.ndarray] = {}

    def quantised(local_id: int) -> np.ndarray:
        if local_id not in quant_cache:
            res = quantize.quantize_rgba(src_tiles[local_id], palette)
            quant_cache[local_id] = res.indices
            de_by_tile[local_id] = res.delta_e
        return quant_cache[local_id]

    bank = TileBank(ts.tile_w, ts.tile_h)
    cells = [0] * (tmap.width * tmap.height)
    warnings: list[str] = []
    materialised_d = 0
    placed = 0

    for i, cell, band in _flatten(tmap):
        if cell is None:
            cells[i] = 0
            continue
        if cell.local_id < 0 or cell.local_id >= ts.tilecount:
            msg = f"cell {i}: local id {cell.local_id} out of range 0..{ts.tilecount - 1}"
            if strict:
                raise ValueError(msg)
            warnings.append(msg)
            cells[i] = 0
            continue

        idx_tile = quantised(cell.local_id)
        if cell.dflip:
            materialised_d += 1
            if strict:
                warnings.append(f"cell {i}: materialised a 90° (diagonal) tile")
        appearance = orient(idx_tile, cell.hflip, cell.vflip, cell.dflip)
        attr = ts.attrs.get(cell.local_id)
        if cell.dflip and attr is not None and (attr.flags & TileAttr.DIR_MASK):
            warnings.append(
                f"cell {i}: directional attr on a 90°-rotated tile is baked but its "
                f"DIR is left as authored (device tmResolveDir only flips H/V)"
            )
        attr_tuple = attr.as_tuple() if attr is not None else (0, 0)
        tid, hflip, vflip = bank.add(appearance, attr_tuple)
        cells[i] = emit.pack_cell(tid, band, 0, hflip, vflip)
        placed += 1

    used_ids = sorted(quant_cache.keys())
    delta_e = (
        np.concatenate([de_by_tile[k] for k in used_ids]) if used_ids else np.zeros(0)
    )

    attrs = bank.attrs() if bank.has_attrs() else None
    njn = emit.build_njn(ts.tile_w, ts.tile_h, bank.pixels(), bank.count(), attrs=attrs)
    njm = emit.emit_njm(cells, tmap.width, tmap.height)
    colliders_lua = _colliders_lua(tmap)

    unique, seen = bank.dedupe_yield()
    return ConvertResult(
        njn=njn, njm=njm, colliders_lua=colliders_lua,
        map_w=tmap.width, map_h=tmap.height,
        unique_tiles=unique, seen_appearances=placed,
        materialised_d=materialised_d, used_source_tiles=len(used_ids),
        delta_e=delta_e, warnings=warnings,
    )


# --- ColliderSet Lua sidecar ----------------------------------------------

def _colliders_lua(tmap: TiledMap) -> str | None:
    """Serialise object-layer colliders as a Lua module, or None if there are none.

    The returned table mirrors the ``engine.scene.colliders()`` API (segments,
    circles, aabbs); an applet ``require``s it and feeds ``addSeg/addCircle/
    addAabb``. This is the only runtime-consumable form today — no binary
    collider chunk/loader exists yet (a future ``.njm`` container can add one).
    """
    segs: list[tuple] = []
    circles: list[tuple] = []
    aabbs: list[tuple] = []
    for group in tmap.object_groups:
        for c in group.colliders:
            if c.shape == "aabb":
                aabbs.append(c.data + (c.restitution, c.kind))
            elif c.shape == "circle":
                circles.append(c.data + (c.restitution, c.kind))
            elif c.shape == "segments":
                pts = c.data
                for k in range(0, len(pts) - 2, 2):
                    segs.append((pts[k], pts[k + 1], pts[k + 2], pts[k + 3], c.restitution, c.kind))
    if not (segs or circles or aabbs):
        return None

    def fmt_rows(rows):
        return "\n".join(
            "    { " + ", ".join(f"{v:g}" for v in row) + " }," for row in rows
        )

    return (
        "-- Generated by tiled2enjin.py — ColliderSet for engine.scene.colliders().\n"
        "-- segments: {ax,ay,bx,by,restitution,kind}; circles: {cx,cy,r,restitution,kind};\n"
        "-- aabbs: {minx,miny,maxx,maxy,restitution,kind}. kind: 0 wall 1 bouncy 2 hazard 3 goal 4 flipper.\n"
        "return {\n"
        "  segments = {\n" + fmt_rows(segs) + "\n  },\n"
        "  circles = {\n" + fmt_rows(circles) + "\n  },\n"
        "  aabbs = {\n" + fmt_rows(aabbs) + "\n  },\n"
        "}\n"
    )


# --- preview rendering ----------------------------------------------------

def render_preview(njn: bytes, njm: bytes, palette: Palette) -> np.ndarray:
    """Reconstruct an RGB image from the *emitted* ``.njn`` + ``.njm`` (round-trip proof).

    Decodes the tileset pixels and map cells back and composes the map, applying
    each cell's flip flags — so a correct-looking preview confirms the binaries
    are internally consistent without the (deferred) runtime loader.
    """
    parsed = emit.parse_njn(njn)
    cw, ch, cols, rows = struct_meta(parsed.chunks[emit.CHUNK_META])
    pix = np.frombuffer(parsed.chunks[emit.CHUNK_PIXL], dtype=np.uint8)
    frame_size = cw * ch
    n_frames = len(pix) // frame_size
    frames = pix[: n_frames * frame_size].reshape(n_frames, ch, cw)

    map_w, map_h, cells = parse_njm(njm)
    # Background = palette index 0 (kept opaque); transparent pixels show it.
    rgb = np.zeros((map_h * ch, map_w * cw, 3), dtype=np.uint8)
    lut = np.vstack([palette.rgb, np.array([[0, 0, 0]], dtype=np.uint8)])  # 15 → black bg

    for ty in range(map_h):
        for tx in range(map_w):
            cell = cells[ty * map_w + tx]
            tid = cell & emit.TM_TILEID_MASK
            if tid == 0 or tid >= n_frames:
                continue
            tile = frames[tid]
            if cell & emit.TM_HFLIP_BIT:
                tile = np.fliplr(tile)
            if cell & emit.TM_VFLIP_BIT:
                tile = np.flipud(tile)
            colour = lut[np.clip(tile, 0, 15)]
            mask = tile != TRANSPARENT_INDEX
            dst = rgb[ty * ch:(ty + 1) * ch, tx * cw:(tx + 1) * cw]
            dst[mask] = colour[mask]
    return rgb


def struct_meta(meta: bytes):
    import struct as _s
    return _s.unpack("<BBBB", meta[:4])


def parse_njm(njm: bytes):
    import struct as _s
    magic, ver, _r0, mw, mh, _r1, _r2 = _s.unpack_from("<2sBBBBBB", njm, 0)
    if magic != emit.NJM_MAGIC:
        raise ValueError("bad .njm magic")
    n = mw * mh
    cells = list(_s.unpack_from("<%dH" % n, njm, 8))
    return mw, mh, cells
