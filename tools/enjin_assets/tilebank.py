"""Flip-aware tile deduplication and tile-id assignment.

The map cell packs a 9-bit tile id plus ``hflip``/``vflip`` bits (see
``tilemap_asset.hpp``): a tile that is the horizontal/vertical mirror of another
must reuse that tile's pixels and set the flip bits rather than storing a second
copy. This bank canonicalises every tile over its four flip orientations
(identity, H, V, HV) so mirror-equivalent art collapses to one id (the #53
"flip-dedupe after quantise" decision). 90° rotations (Tiled's diagonal flag)
are *not* in that group and must be materialised into distinct appearance tiles
by the caller before adding — such a tile simply lands as its own canonical id.

Tile id 0 is reserved for the fully-transparent tile (a map cell of 0 is empty),
matching the wasted frame-0 convention of the sprite path. The 9-bit id field
caps the bank at :data:`MAX_TILES` (512) ids including id 0.
"""

from __future__ import annotations

import numpy as np

from .palette import TRANSPARENT_INDEX

#: Max tile ids addressable by the 9-bit cell field (id 0 = transparent).
MAX_TILES = 512


def _flip(tile: np.ndarray, hflip: bool, vflip: bool) -> np.ndarray:
    """Return ``tile`` mirrored per the flags (h then v)."""
    t = tile
    if hflip:
        t = np.fliplr(t)
    if vflip:
        t = np.flipud(t)
    return t


def _orientations(tile: np.ndarray):
    """Yield ``(flipped_bytes, hflip, vflip)`` for the tile's 4 flip orientations.

    Because each flip is its own inverse, if ``flip(P, h, v)`` equals a stored
    canonical ``C`` then ``flip(C, h, v)`` reproduces ``P`` — so a cell drawing
    ``C`` with flags ``(h, v)`` renders ``P``. The caller looks up each yielded
    ``flipped_bytes`` against the canonical table and, on a hit, adopts ``(h, v)``.
    """
    for hflip in (False, True):
        for vflip in (False, True):
            yield _flip(tile, hflip, vflip).tobytes(), hflip, vflip


class TileBank:
    """Accumulates deduplicated tiles and assigns 9-bit ids.

    Call :meth:`add` with each tile's final on-screen appearance; it returns the
    ``(tile_id, hflip, vflip)`` the map cell should carry. :meth:`pixels` and
    :meth:`attrs` serialise the bank for the ``PIXL`` and ``ATTR`` chunks.
    """

    def __init__(self, tile_w: int, tile_h: int):
        self.tile_w = tile_w
        self.tile_h = tile_h
        # id 0 = fully transparent tile, attr (0, 0).
        empty = np.full((tile_h, tile_w), TRANSPARENT_INDEX, dtype=np.uint8)
        self._tiles: list[np.ndarray] = [empty]
        self._attrs: list[tuple[int, int]] = [(0, 0)]
        # Map (canonical_bytes, attr) → id. Only the canonical (first-seen) form
        # of each tile is registered; lookups flip the *incoming* tile to match.
        self._key_to_id: dict[tuple[bytes, tuple[int, int]], int] = {
            (empty.tobytes(), (0, 0)): 0
        }
        #: Number of distinct appearance tiles seen (before dedupe), for reporting.
        self.seen = 0

    def add(self, tile: np.ndarray, attr: tuple[int, int] = (0, 0)) -> tuple[int, bool, bool]:
        """Add a tile appearance; return ``(tile_id, hflip, vflip)``.

        Args:
            tile: 2-D ``(h, w)`` uint8 index array — the tile as it should appear.
            attr: ``(flags, kind)`` attribute tuple; tiles that differ only in
                attributes are kept distinct (attributes are part of the key).

        Raises:
            ValueError: if the bank would exceed :data:`MAX_TILES` ids.
        """
        self.seen += 1
        t = np.asarray(tile, dtype=np.uint8)
        if t.shape != (self.tile_h, self.tile_w):
            raise ValueError(f"tile shape {t.shape} != ({self.tile_h}, {self.tile_w})")

        # If any orientation of this appearance is already stored, reuse it.
        for b, hflip, vflip in _orientations(t):
            key = (b, attr)
            existing = self._key_to_id.get(key)
            if existing is not None:
                # Stored canonical C satisfies flip(t, h, v) == C, and since each
                # flip is its own inverse, flip(C, h, v) == t: the cell carries (h, v).
                return existing, hflip, vflip

        # New tile: store t as its own canonical (register canonical bytes only).
        if len(self._tiles) >= MAX_TILES:
            raise ValueError(
                f"tileset exceeds the {MAX_TILES}-id budget (9-bit cell); reduce the map's used tiles"
            )
        tid = len(self._tiles)
        self._tiles.append(t.copy())
        self._attrs.append(attr)
        self._key_to_id[(t.tobytes(), attr)] = tid
        return tid, False, False

    def count(self) -> int:
        """Number of tile ids in the bank, including id 0."""
        return len(self._tiles)

    def dedupe_yield(self) -> tuple[int, int]:
        """Return ``(unique_non_empty_tiles, appearances_seen)`` for reporting."""
        return self.count() - 1, self.seen

    def pixels(self) -> bytes:
        """Frame-major pixel bytes for the ``PIXL`` chunk (1 byte/pixel, low nibble)."""
        out = bytearray()
        for t in self._tiles:
            out.extend(int(b) & 0x0F for b in t.reshape(-1))
        return bytes(out)

    def attrs(self) -> list[tuple[int, int]]:
        """Per-tile ``(flags, kind)`` records for the ``ATTR`` chunk, id-ordered."""
        return list(self._attrs)

    def has_attrs(self) -> bool:
        """True if any tile carries a non-zero attribute (else ATTR is omitted)."""
        return any(flags or kind for flags, kind in self._attrs)
