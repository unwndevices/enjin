"""The engine's target palette: the 8 Palette roles, the house default or a GIMP ``.gpl``.

The default is the engine's house default (``src/graphics/palette.cpp``): the
8 Palette roles (ADR-0022) at indices 0–7. Indices 8–14 are not roles: no tool
writes them and the engine refuses them. A ``.gpl`` lists the 8 roles first
(a 9th row, the transparent key magenta ``#FF00FF``, is ignored). Quantisation
only ever maps to the 8 roles; a transparent source pixel is assigned index 15
directly, never by colour distance.
"""

from __future__ import annotations

import os
from dataclasses import dataclass

import numpy as np

from . import oklab

#: Reserved transparent palette index (matches TRANSPARENT_INDEX across the tools).
TRANSPARENT_INDEX = 15

#: Number of opaque, quantisable colours: the Palette roles (indices 0..OPAQUE_COUNT-1).
OPAQUE_COUNT = 8


def is_palette_index(v: int) -> bool:
    """A stored index the engine accepts: a Palette role 0..7 or transparent 15."""
    return 0 <= v < OPAQUE_COUNT or v == TRANSPARENT_INDEX

#: The house default, as in the engine's ``src/graphics/palette.cpp``.
HOUSE_DEFAULT_RGB: tuple[tuple[int, int, int], ...] = (
    (0x08, 0x1A, 0x1C),  # 0  bg
    (0x16, 0x46, 0x32),  # 1  surface
    (0x52, 0xA8, 0x2E),  # 2  muted
    (0xE8, 0xF4, 0xCC),  # 3  text
    (0xB2, 0xDA, 0x63),  # 4  accent
    (0xF0, 0xA8, 0x30),  # 5  accent2
    (0x48, 0xC8, 0xD8),  # 6  accent3
    (0xE8, 0x48, 0x40),  # 7  accent4
)


@dataclass(frozen=True)
class Palette:
    """A loaded 8-role palette plus precomputed Oklab coordinates.

    Attributes:
        name: Human-readable palette name from the ``.gpl`` header.
        rgb:  ``(8, 3)`` uint8 array of the role colours (indices 0–7).
        oklab: ``(8, 3)`` float array — the Oklab coordinates of ``rgb``.
    """

    name: str
    rgb: np.ndarray
    oklab: np.ndarray


def load_gpl(path: str) -> Palette:
    """Parse a GIMP ``.gpl`` file into a :class:`Palette`.

    Reads the leading opaque entries (up to :data:`OPAQUE_COUNT`) and ignores the
    trailing transparent key. Raises ``ValueError`` if fewer than
    :data:`OPAQUE_COUNT` colour rows are present.
    """
    name = os.path.splitext(os.path.basename(path))[0]
    colours: list[tuple[int, int, int]] = []
    with open(path, "r", encoding="utf-8") as f:
        for raw in f:
            line = raw.strip()
            if not line or line.startswith("#"):
                continue
            if line.startswith("GIMP Palette"):
                continue
            if line.startswith("Name:"):
                name = line[len("Name:"):].strip()
                continue
            if line.startswith("Columns:"):
                continue
            parts = line.split()
            if len(parts) < 3:
                continue
            try:
                r, g, b = int(parts[0]), int(parts[1]), int(parts[2])
            except ValueError:
                continue
            colours.append((r, g, b))

    if len(colours) < OPAQUE_COUNT:
        raise ValueError(
            f"palette {path!r} has only {len(colours)} colours, need at least {OPAQUE_COUNT}"
        )

    rgb = np.array(colours[:OPAQUE_COUNT], dtype=np.uint8)
    return Palette(name=name, rgb=rgb, oklab=oklab.srgb_to_oklab(rgb))


def load_default() -> Palette:
    """The engine's house default palette (:data:`HOUSE_DEFAULT_RGB`)."""
    rgb = np.array(HOUSE_DEFAULT_RGB, dtype=np.uint8)
    return Palette(name="house-default", rgb=rgb, oklab=oklab.srgb_to_oklab(rgb))
