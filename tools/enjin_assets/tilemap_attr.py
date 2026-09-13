"""Per-tile attribute encoding — the Python mirror of C++ ``TileAttr``.

Matches ``enjin2::TileAttr`` in ``graphics/tilemap_asset.hpp`` (ADR-0003 §3):
a 2-byte record ``{flags, kind}`` where ``flags`` packs SOLID (bit0), ONEWAY
(bit1) and a cardinal DIR (bits2-3), and ``kind`` is a full ``uint8``. DIR is
authored here and flip-resolved at query time on device, so we store the
authored value unchanged.
"""

from __future__ import annotations

from dataclasses import dataclass

# Cardinal directions (mirror TileDir): Up=0, Right=1, Down=2, Left=3.
DIR_UP, DIR_RIGHT, DIR_DOWN, DIR_LEFT = 0, 1, 2, 3
_DIR_NAMES = {"up": DIR_UP, "right": DIR_RIGHT, "down": DIR_DOWN, "left": DIR_LEFT}


def parse_dir(value: str) -> int:
    """Map a direction name (``up``/``right``/``down``/``left``) to 0-3 (default Up)."""
    return _DIR_NAMES.get((value or "").strip().lower(), DIR_UP)


@dataclass
class TileAttr:
    """Per-tile attribute record (``flags``, ``kind``)."""

    FLAG_SOLID = 0x01
    FLAG_ONEWAY = 0x02
    DIR_SHIFT = 2
    DIR_MASK = 0x0C

    flags: int = 0
    kind: int = 0

    def set_dir(self, direction: int) -> None:
        """Write the 2-bit cardinal direction into ``flags`` bits 2-3."""
        self.flags = (self.flags & ~self.DIR_MASK) | ((direction & 0x03) << self.DIR_SHIFT)

    @property
    def solid(self) -> bool:
        return bool(self.flags & self.FLAG_SOLID)

    @property
    def oneway(self) -> bool:
        return bool(self.flags & self.FLAG_ONEWAY)

    @property
    def dir(self) -> int:
        return (self.flags >> self.DIR_SHIFT) & 0x03

    def as_tuple(self) -> tuple[int, int]:
        """Return ``(flags, kind)`` for the ``ATTR`` chunk / tilebank key."""
        return (self.flags & 0xFF, self.kind & 0xFF)
