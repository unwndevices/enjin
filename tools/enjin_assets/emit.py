"""Writers for the ``.njn`` v2 typed-chunk container and the ``.njm`` map format.

The wire layouts are the Python mirror of the C++ single source of truth:
``enjin2/graphics/njn2.hpp`` (the v2 container: 12-byte header, 12-byte directory
entries, ``META``/``PIXL``/``ATTR``/``PALB``/``CLIP`` chunks) and
``enjin2/graphics/tilemap_asset.hpp`` (the ``.njm`` v1 map). A byte produced here
must parse cleanly under ``NjnV2Reader`` / ``parseNjmHeader``; :func:`parse_njn`
below re-implements the reader's validation so tests can assert that invariant
without a C++ build.
"""

from __future__ import annotations

import struct
from dataclasses import dataclass, field

# --- .njn v2 container constants (mirror njn2.hpp) ------------------------

NJN2_MAGIC = b"NJ"
NJN2_VERSION = 2
NJN2_FILE_HEADER_SIZE = 12
NJN2_DIR_ENTRY_SIZE = 12

CHUNK_META = b"META"
CHUNK_PIXL = b"PIXL"
CHUNK_CLIP = b"CLIP"
CHUNK_ATTR = b"ATTR"
CHUNK_PALB = b"PALB"

# --- .njm map constants + cell packing (mirror tilemap_asset.hpp) --------

NJM_MAGIC = b"NM"
NJM_VERSION = 1

# Cell bit layout [band:1 | vflip:1 | hflip:1 | palbank:4 | tileid:9] (tmPackCell).
TM_TILEID_MASK = 0x01FF
TM_PALBANK_SHIFT = 9
TM_PALBANK_MASK = 0x000F
TM_HFLIP_BIT = 1 << 13
TM_VFLIP_BIT = 1 << 14
TM_BAND_BIT = 1 << 15


def pack_cell(tile_id: int, band: int = 0, palbank: int = 0,
              hflip: bool = False, vflip: bool = False) -> int:
    """Pack a 16-bit tilemap cell (the Python mirror of ``tmPackCell``)."""
    return (
        (tile_id & TM_TILEID_MASK)
        | ((palbank & TM_PALBANK_MASK) << TM_PALBANK_SHIFT)
        | (TM_HFLIP_BIT if hflip else 0)
        | (TM_VFLIP_BIT if vflip else 0)
        | (TM_BAND_BIT if band else 0)
    ) & 0xFFFF

# Loop modes (mirror NjnLoopMode).
LOOP_ONCE = 0
LOOP_LOOP = 1
LOOP_PINGPONG = 2


@dataclass
class Clip:
    """One animation clip for the ``CLIP`` chunk.

    ``frames`` is a list of ``(frame_index, duration_ms, event_id)`` tuples.
    """

    name: str
    loop_mode: int = LOOP_ONCE
    frames: list[tuple[int, int, int]] = field(default_factory=list)


class NjnV2Writer:
    """Accumulative ``.njn`` v2 builder (mirrors the C++ ``NjnV2Writer``)."""

    def __init__(self):
        self._chunks: list[tuple[bytes, bytes]] = []

    def add_chunk(self, tag: bytes, data: bytes) -> None:
        """Append a raw chunk. ``tag`` must be exactly 4 bytes."""
        if len(tag) != 4:
            raise ValueError(f"chunk tag must be 4 bytes, got {tag!r}")
        self._chunks.append((tag, bytes(data)))

    def finalise(self) -> bytes:
        """Serialise header + directory + packed chunk data (little-endian)."""
        num = len(self._chunks)
        data_offset = NJN2_FILE_HEADER_SIZE + num * NJN2_DIR_ENTRY_SIZE
        file_size = data_offset + sum(len(d) for _, d in self._chunks)

        header = struct.pack(
            "<2sBBII", NJN2_MAGIC, NJN2_VERSION, 0, num, file_size
        )
        directory = bytearray()
        cur = data_offset
        for tag, data in self._chunks:
            directory += struct.pack("<4sII", tag, cur, len(data))
            cur += len(data)
        body = b"".join(data for _, data in self._chunks)
        return header + bytes(directory) + body


def choose_grid(count: int) -> tuple[int, int]:
    """Pick a ``(cols, rows)`` sheet grid for ``count`` frames, both ≤255.

    Frame addressing in the engine is frame-major (``frameIndex * frameSize``) so
    the grid is metadata; we still keep ``cols*rows >= count`` and both bytes in
    range. ``count ≤ 255`` stays a single row (no padding); larger counts (up to
    the 512-id budget) use a near-square grid, and the caller pads the pixel data
    to ``cols*rows`` frames.
    """
    if count <= 0:
        return (1, 1)
    if count <= 255:
        return (count, 1)
    cols = 255
    rows = (count + cols - 1) // cols
    if rows > 255:
        raise ValueError(f"{count} frames exceed the 255x255 sheet grid")
    return (cols, rows)


def build_njn(
    cell_w: int,
    cell_h: int,
    pixels: bytes,
    tile_count: int,
    *,
    attrs: list[tuple[int, int]] | None = None,
    palb: list[int] | None = None,
    clips: list[Clip] | None = None,
) -> bytes:
    """Build a ``.njn`` v2 file from a frame-major pixel blob and optional chunks.

    Args:
        cell_w, cell_h: Tile/frame dimensions in pixels.
        pixels: Frame-major pixel bytes (1 byte/pixel, low nibble); length must be
            a multiple of ``cell_w*cell_h``.
        tile_count: Number of real frames the caller cares about (used to pick the
            grid). ``pixels`` is padded with transparent frames to ``cols*rows``.
        attrs: Optional per-tile ``(flags, kind)`` list → ``ATTR`` chunk.
        palb: Optional flat RGB565 list (numBanks*16 entries) → ``PALB`` chunk.
        clips: Optional list of :class:`Clip` → ``CLIP`` chunk.
    """
    frame_size = cell_w * cell_h
    if frame_size == 0 or len(pixels) % frame_size != 0:
        raise ValueError("pixel length must be a positive multiple of cell_w*cell_h")

    cols, rows = choose_grid(tile_count)
    total_frames = cols * rows
    # Pad pixel data with transparent frames so cols*rows frames are backed.
    from .palette import TRANSPARENT_INDEX

    have_frames = len(pixels) // frame_size
    padded = bytearray(pixels)
    if have_frames < total_frames:
        padded += bytes([TRANSPARENT_INDEX]) * (frame_size * (total_frames - have_frames))

    if cols > 255 or rows > 255:
        raise ValueError(f"sheet grid {cols}x{rows} exceeds the 255x255 META limit")

    w = NjnV2Writer()
    w.add_chunk(CHUNK_META, struct.pack("<BBBB", cell_w & 0xFF, cell_h & 0xFF, cols, rows))
    w.add_chunk(CHUNK_PIXL, bytes(b & 0x0F for b in padded))

    if attrs is not None and any(f or k for f, k in attrs):
        body = bytearray(struct.pack("<H", len(attrs)))
        for flags, kind in attrs:
            body += struct.pack("<BB", flags & 0xFF, kind & 0xFF)
        w.add_chunk(CHUNK_ATTR, bytes(body))

    if palb:
        if len(palb) % 16 != 0:
            raise ValueError("palb must hold a whole number of 16-entry banks")
        num_banks = len(palb) // 16
        body = bytearray(struct.pack("<B", num_banks))
        for entry in palb:
            body += struct.pack("<H", entry & 0xFFFF)
        w.add_chunk(CHUNK_PALB, bytes(body))

    if clips:
        w.add_chunk(CHUNK_CLIP, _build_clip_chunk(clips))

    return w.finalise()


def _build_clip_chunk(clips: list[Clip]) -> bytes:
    """Serialise the ``CLIP`` chunk body (mirrors njn2WriteClip)."""
    if len(clips) > 255:
        raise ValueError("at most 255 clips per sheet")
    body = bytearray(struct.pack("<B", len(clips)))
    for c in clips:
        name = c.name.encode("utf-8")[:15]
        body += name + b"\x00" * (16 - len(name))
        if len(c.frames) > 255:
            raise ValueError(f"clip {c.name!r} has >255 frames")
        body += struct.pack("<BB", c.loop_mode & 0xFF, len(c.frames))
        for frame_index, duration_ms, event_id in c.frames:
            body += struct.pack("<HHB", frame_index & 0xFFFF, duration_ms & 0xFFFF, event_id & 0xFF)
    return bytes(body)


def emit_njm(cells: list[int], map_w: int, map_h: int) -> bytes:
    """Encode a map as ``.njm`` bytes: 8-byte 'NM' header + LE uint16 cells."""
    if map_w > 255 or map_h > 255:
        raise ValueError(f"map {map_w}x{map_h} exceeds the 255x255 .njm header limit")
    if len(cells) != map_w * map_h:
        raise ValueError(f"cell count {len(cells)} != {map_w}*{map_h}")
    header = struct.pack("<2sBBBBBB", NJM_MAGIC, NJM_VERSION, 0, map_w & 0xFF, map_h & 0xFF, 0, 0)
    body = b"".join(struct.pack("<H", c & 0xFFFF) for c in cells)
    return header + body


# --- reader (mirrors NjnV2Reader validation, for round-trip tests) --------

@dataclass
class ParsedNjn:
    version: int
    num_chunks: int
    file_size: int
    chunks: dict  # tag(bytes) -> data(bytes)


def parse_njn(buf: bytes) -> ParsedNjn:
    """Parse+validate a ``.njn`` v2 buffer exactly as ``NjnV2Reader::open`` does.

    Raises ``ValueError`` on any condition the C++ reader would reject (bad magic
    or version, file-size mismatch, out-of-bounds chunk offsets), so a passing
    parse proves the emitted bytes are loadable on device/web.
    """
    if len(buf) < NJN2_FILE_HEADER_SIZE:
        raise ValueError("buffer smaller than the file header")
    magic, version, _reserved, num_chunks, file_size = struct.unpack_from("<2sBBII", buf, 0)
    if magic != NJN2_MAGIC:
        raise ValueError(f"bad magic {magic!r}")
    if version != NJN2_VERSION:
        raise ValueError(f"bad version {version}")
    if file_size != len(buf):
        raise ValueError(f"file_size {file_size} != buffer length {len(buf)}")
    dir_bytes = num_chunks * NJN2_DIR_ENTRY_SIZE
    if len(buf) < NJN2_FILE_HEADER_SIZE + dir_bytes:
        raise ValueError("directory runs past the buffer")

    chunks: dict = {}
    for i in range(num_chunks):
        base = NJN2_FILE_HEADER_SIZE + i * NJN2_DIR_ENTRY_SIZE
        tag, offset, size = struct.unpack_from("<4sII", buf, base)
        if offset < NJN2_FILE_HEADER_SIZE:
            raise ValueError(f"chunk {tag!r} offset {offset} inside the header")
        if offset + size > file_size:
            raise ValueError(f"chunk {tag!r} runs past the file")
        chunks[tag] = buf[offset:offset + size]
    return ParsedNjn(version=version, num_chunks=num_chunks, file_size=file_size, chunks=chunks)
