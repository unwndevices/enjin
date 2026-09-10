"""Decode PNGs and slice a packed tilesheet into individual tiles.

Purchased packs ship the tileset as one PNG (Tiled's ``<image>``), optionally
with ``margin`` around the sheet and ``spacing`` between tiles (Kenney's
``tilemap.png`` is 12×11 tiles at 16px with 1px spacing). This module returns
tiles as ``(tile_h, tile_w, 4)`` uint8 RGBA arrays in row-major order so the
quantiser and tilebank can treat every front-end identically.
"""

from __future__ import annotations

import numpy as np
from PIL import Image


def load_rgba(path: str) -> np.ndarray:
    """Load a PNG (any mode) as an ``(H, W, 4)`` uint8 RGBA array."""
    with Image.open(path) as im:
        return np.asarray(im.convert("RGBA"), dtype=np.uint8)


def slice_sheet(
    rgba: np.ndarray,
    tile_w: int,
    tile_h: int,
    columns: int,
    count: int,
    *,
    margin: int = 0,
    spacing: int = 0,
) -> list[np.ndarray]:
    """Slice a packed tilesheet into ``count`` tiles, row-major (Tiled order).

    Args:
        rgba: The full sheet as ``(H, W, 4)`` uint8.
        tile_w, tile_h: Tile dimensions in pixels.
        columns: Number of tile columns in the sheet.
        count: Total number of tiles to extract (Tiled's ``tilecount``).
        margin: Pixels of border around the whole sheet.
        spacing: Pixels between adjacent tiles.

    Returns:
        A list of ``count`` ``(tile_h, tile_w, 4)`` uint8 arrays. A tile that
        runs off the sheet edge is padded with transparent (0,0,0,0) pixels.

    Raises:
        ValueError: if ``columns`` or ``count`` is non-positive.
    """
    if columns <= 0 or count <= 0:
        raise ValueError("columns and count must be positive")

    arr = np.asarray(rgba, dtype=np.uint8)
    h, w = arr.shape[0], arr.shape[1]
    tiles: list[np.ndarray] = []
    for idx in range(count):
        row = idx // columns
        col = idx % columns
        x0 = margin + col * (tile_w + spacing)
        y0 = margin + row * (tile_h + spacing)
        tile = np.zeros((tile_h, tile_w, 4), dtype=np.uint8)
        # Copy the overlapping region; pad the rest transparent.
        sx1 = min(x0 + tile_w, w)
        sy1 = min(y0 + tile_h, h)
        if x0 < w and y0 < h and sx1 > x0 and sy1 > y0:
            tile[: sy1 - y0, : sx1 - x0] = arr[y0:sy1, x0:sx1]
        tiles.append(tile)
    return tiles
