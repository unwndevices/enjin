"""Load a GIMP ``.gpl`` ramp palette as the engine's 15-index target palette.

The canonical palettes live in ``tools/palettes/`` (``enjin_default.gpl``,
``enjin_gameboy.gpl``). Each holds 16 entries: indices 0–14 are opaque colours
and index 15 is the reserved transparent key (magenta ``#FF00FF``). Quantisation
only ever maps to the 15 opaque colours; a transparent source pixel is assigned
index 15 directly, never by colour distance.
"""

from __future__ import annotations

import os
from dataclasses import dataclass

import numpy as np

from . import oklab

#: Reserved transparent palette index (matches TRANSPARENT_INDEX across the tools).
TRANSPARENT_INDEX = 15

#: Number of opaque, quantisable colours (indices 0..OPAQUE_COUNT-1).
OPAQUE_COUNT = 15

#: Directory holding the canonical ``.gpl`` palettes, next to this package.
PALETTES_DIR = os.path.join(os.path.dirname(os.path.dirname(os.path.abspath(__file__))), "palettes")


@dataclass(frozen=True)
class Palette:
    """A loaded 15-index palette plus precomputed Oklab coordinates.

    Attributes:
        name: Human-readable palette name from the ``.gpl`` header.
        rgb:  ``(15, 3)`` uint8 array of the opaque colours (indices 0–14).
        oklab: ``(15, 3)`` float array — the Oklab coordinates of ``rgb``.
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
    """Load the canonical ``enjin_default.gpl`` 15-colour palette."""
    return load_gpl(os.path.join(PALETTES_DIR, "enjin_default.gpl"))
