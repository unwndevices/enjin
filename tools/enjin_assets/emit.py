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
import warnings
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

# --- Layered sprite chunks (schema version 1, mirror njn2.hpp, issue #93) ---

CHUNK_LHDR = b"LHDR"
CHUNK_LIMG = b"LIMG"
CHUNK_LPRT = b"LPRT"
CHUNK_LREF = b"LREF"
CHUNK_LDUR = b"LDUR"
CHUNK_LPIV = b"LPIV"

#: The only layered schema version understood (mirror NJN2_LAYERED_SCHEMA_VERSION).
LAYERED_SCHEMA_VERSION = 1

#: Frame-part reference image index meaning "invisible this frame"
#: (mirror NJN2_LAYERED_INVISIBLE).
LAYERED_INVISIBLE = 0xFFFF

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


@dataclass
class PartImage:
    """One cropped, immutable part image: dimensions + canonical 4bpp pixels.

    ``pixels`` is ``w*h`` bytes, one palette index per byte in the low nibble.
    """

    w: int
    h: int
    pixels: bytes


@dataclass
class Layered:
    """A layered sprite asset (the union of the layered chunks).

    ``refs`` is frame-major: ``num_frames * num_parts`` tuples of
    ``(image_index, offset_x, offset_y)``; ``image_index == LAYERED_INVISIBLE``
    marks the part invisible that frame.  ``parts`` lists names in bottom-to-top
    painter order.  ``clips`` is optional; its frame indices reference animation
    frames (0..num_frames-1), not sheet cells.

    ``pivot_x`` / ``pivot_y`` are the sprite's single static pivot point in the
    canvas coordinate space: top-left origin, +x right / +y down, a pixel-center
    coordinate in ``0..canvas_w-1`` / ``0..canvas_h-1``.  An outside-canvas value
    is allowed but warns on parse.  The default ``(0, 0)`` is written as an
    absent ``LPIV`` chunk (``build_njn_layered`` only emits ``LPIV`` for a
    non-zero pivot), and an absent chunk round-trips back to ``(0, 0)``.
    """

    canvas_w: int
    canvas_h: int
    images: list[PartImage]
    parts: list[str]
    refs: list[tuple[int, int, int]]
    durations: list[int]
    clips: list[Clip] | None = None
    schema_version: int = LAYERED_SCHEMA_VERSION
    pivot_x: int = 0
    pivot_y: int = 0


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


# --- layered sprite codec (schema version 1, mirror njn2DecodeLayered) ------

def build_njn_layered(asset: Layered) -> bytes:
    """Serialise a :class:`Layered` asset into ``.njn`` v2 bytes.

    Counts are derived from the asset: ``num_frames = len(durations)``,
    ``num_parts = len(parts)``, ``num_images = len(images)``.  Raises
    ``ValueError`` on an inconsistent asset (wrong ``refs`` length, an image
    whose ``pixels`` length disagrees with ``w*h``, or a count exceeding 16 bits).

    The ``CLIP`` chunk is reused unchanged; its frame indices reference animation
    frames, not sheet cells.  No ``META``/``PIXL`` fallback is written.  A
    non-zero pivot is written as a 4-byte ``LPIV`` chunk (s16 pivotX, s16 pivotY);
    the default ``(0, 0)`` pivot is left absent.
    """
    num_frames = len(asset.durations)
    num_parts = len(asset.parts)
    num_images = len(asset.images)

    for name, count in (("frames", num_frames), ("parts", num_parts), ("images", num_images)):
        if count > 0xFFFF:
            raise ValueError(f"{count} {name} exceed the 16-bit layered count limit")
    for name, value in (("canvas_w", asset.canvas_w), ("canvas_h", asset.canvas_h)):
        if not 0 < value <= 0xFFFF:
            raise ValueError(f"{name} {value} outside the 1..65535 canvas extent")
    if num_images == 0:
        raise ValueError("a layered asset needs at least one part image")

    if len(asset.refs) != num_frames * num_parts:
        raise ValueError(
            f"refs has {len(asset.refs)} entries, expected {num_frames}*{num_parts}"
        )

    w = NjnV2Writer()
    w.add_chunk(
        CHUNK_LHDR,
        struct.pack(
            "<BHHHHH",
            asset.schema_version & 0xFF,
            asset.canvas_w,
            asset.canvas_h,
            num_frames,
            num_parts,
            num_images,
        ),
    )

    limg = bytearray()
    for img in asset.images:
        if len(img.pixels) != img.w * img.h:
            raise ValueError(
                f"image {img.w}x{img.h} has {len(img.pixels)} pixels, expected {img.w * img.h}"
            )
        limg += struct.pack("<HH", img.w & 0xFFFF, img.h & 0xFFFF)
        limg += bytes(b & 0x0F for b in img.pixels)
    w.add_chunk(CHUNK_LIMG, bytes(limg))

    lprt = bytearray()
    for name in asset.parts:
        encoded = name.encode("utf-8")[:15]
        lprt += encoded + b"\x00" * (16 - len(encoded))
    w.add_chunk(CHUNK_LPRT, bytes(lprt))

    lref = bytearray()
    for image_index, offset_x, offset_y in asset.refs:
        if not -0x8000 <= offset_x <= 0x7FFF or not -0x8000 <= offset_y <= 0x7FFF:
            raise ValueError(f"reference offset ({offset_x},{offset_y}) outside s16 range")
        lref += struct.pack("<Hhh", image_index & 0xFFFF, offset_x, offset_y)
    w.add_chunk(CHUNK_LREF, bytes(lref))

    w.add_chunk(
        CHUNK_LDUR,
        b"".join(struct.pack("<H", d & 0xFFFF) for d in asset.durations),
    )

    if asset.pivot_x or asset.pivot_y:
        if not -0x8000 <= asset.pivot_x <= 0x7FFF or not -0x8000 <= asset.pivot_y <= 0x7FFF:
            raise ValueError(
                f"pivot ({asset.pivot_x},{asset.pivot_y}) outside s16 range"
            )
        w.add_chunk(CHUNK_LPIV, struct.pack("<hh", asset.pivot_x, asset.pivot_y))

    if asset.clips:
        w.add_chunk(CHUNK_CLIP, _build_clip_chunk(asset.clips))

    return w.finalise()


def parse_njn_layered(buf: bytes) -> Layered:
    """Parse+validate a layered ``.njn`` v2 buffer as ``njn2DecodeLayered`` does.

    Rejects the same conditions the C++ decoder rejects: bad magic/version, a
    directory entry out of bounds, missing or duplicate required chunks, an
    unsupported layered schema version, zero/overflowing counts, truncated
    records, zero-size images, out-of-range part-image references, and CLIP frame
    indices outside 0..num_frames-1.  Raises ``ValueError`` on any of these.
    A duplicate or truncated ``LPIV`` is rejected; an out-of-canvas pivot is
    allowed but warns via :mod:`warnings`.  An absent ``LPIV`` decodes to
    ``pivot_x == pivot_y == 0``.
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

    chunks: dict[bytes, bytes] = {}
    counts: dict[bytes, int] = {}
    for i in range(num_chunks):
        base = NJN2_FILE_HEADER_SIZE + i * NJN2_DIR_ENTRY_SIZE
        tag, offset, size = struct.unpack_from("<4sII", buf, base)
        if offset < NJN2_FILE_HEADER_SIZE:
            raise ValueError(f"chunk {tag!r} offset {offset} inside the header")
        if offset + size > file_size:
            raise ValueError(f"chunk {tag!r} runs past the file")
        chunks[tag] = buf[offset:offset + size]
        counts[tag] = counts.get(tag, 0) + 1

    for required in (CHUNK_LHDR, CHUNK_LIMG, CHUNK_LPRT, CHUNK_LREF, CHUNK_LDUR):
        n = counts.get(required, 0)
        if n == 0:
            raise ValueError(f"missing required layered chunk {required!r}")
        if n > 1:
            raise ValueError(f"duplicate required layered chunk {required!r}")
    if counts.get(CHUNK_CLIP, 0) > 1:
        raise ValueError("duplicate CLIP chunk")
    if counts.get(CHUNK_LPIV, 0) > 1:
        raise ValueError("duplicate LPIV chunk")

    hdr = chunks[CHUNK_LHDR]
    if len(hdr) < 11:
        raise ValueError("truncated LHDR")
    schema, canvas_w, canvas_h, num_frames, num_parts, num_images = struct.unpack_from(
        "<BHHHHH", hdr, 0
    )
    if schema != LAYERED_SCHEMA_VERSION:
        raise ValueError(f"unsupported layered schema version {schema}")
    if canvas_w == 0 or canvas_h == 0:
        raise ValueError("zero canvas extent")
    if num_frames == 0 or num_parts == 0:
        raise ValueError("zero frame/part count")
    if num_images == 0:
        raise ValueError("empty part-image pool")

    images: list[PartImage] = []
    limg = chunks[CHUNK_LIMG]
    pos = 0
    for _ in range(num_images):
        if pos + 4 > len(limg):
            raise ValueError("truncated LIMG image header")
        w, h = struct.unpack_from("<HH", limg, pos)
        pos += 4
        if w == 0 or h == 0:
            raise ValueError("zero-size part image")
        if pos + w * h > len(limg):
            raise ValueError("truncated LIMG pixels")
        images.append(PartImage(w=w, h=h, pixels=bytes(limg[pos:pos + w * h])))
        pos += w * h
    if pos != len(limg):
        raise ValueError("LIMG trailing bytes")

    lprt = chunks[CHUNK_LPRT]
    if len(lprt) != num_parts * 16:
        raise ValueError("LPRT size mismatch")
    parts = [
        lprt[i * 16:(i + 1) * 16].rstrip(b"\x00").decode("utf-8", "replace")
        for i in range(num_parts)
    ]

    lref = chunks[CHUNK_LREF]
    if len(lref) != num_frames * num_parts * 6:
        raise ValueError("LREF size mismatch")
    refs: list[tuple[int, int, int]] = []
    for i in range(num_frames * num_parts):
        image_index, offset_x, offset_y = struct.unpack_from("<Hhh", lref, i * 6)
        if image_index != LAYERED_INVISIBLE and image_index >= num_images:
            raise ValueError("invalid part-image reference")
        refs.append((image_index, offset_x, offset_y))

    ldur = chunks[CHUNK_LDUR]
    if len(ldur) != num_frames * 2:
        raise ValueError("LDUR size mismatch")
    durations = [
        struct.unpack_from("<H", ldur, i * 2)[0] for i in range(num_frames)
    ]

    clips: list[Clip] | None = None
    if CHUNK_CLIP in chunks:
        clips = _parse_clip_chunk(chunks[CHUNK_CLIP])
        for clip in clips:
            for frame_index, _dur, _event in clip.frames:
                if frame_index >= num_frames:
                    raise ValueError("CLIP frame index out of range")

    pivot_x, pivot_y = 0, 0
    if CHUNK_LPIV in chunks:
        lpiv = chunks[CHUNK_LPIV]
        if len(lpiv) < 4:
            raise ValueError("truncated LPIV")
        pivot_x, pivot_y = struct.unpack_from("<hh", lpiv, 0)
        if not (0 <= pivot_x < canvas_w and 0 <= pivot_y < canvas_h):
            warnings.warn(
                f"pivot ({pivot_x},{pivot_y}) outside canvas {canvas_w}x{canvas_h}",
                stacklevel=2,
            )

    return Layered(
        canvas_w=canvas_w,
        canvas_h=canvas_h,
        images=images,
        parts=parts,
        refs=refs,
        durations=durations,
        clips=clips,
        schema_version=schema,
        pivot_x=pivot_x,
        pivot_y=pivot_y,
    )


def _parse_clip_chunk(body: bytes) -> list[Clip]:
    """Decode a ``CLIP`` chunk body (mirror njn2DecodeClip)."""
    if len(body) < 1:
        raise ValueError("truncated CLIP chunk")
    num_clips = body[0]
    pos = 1
    clips: list[Clip] = []
    for _ in range(num_clips):
        if pos + 18 > len(body):
            raise ValueError("truncated CLIP chunk")
        name = body[pos:pos + 16].rstrip(b"\x00").decode("utf-8", "replace")
        loop_mode = body[pos + 16]
        num_frames = body[pos + 17]
        pos += 18
        if pos + num_frames * 5 > len(body):
            raise ValueError("truncated CLIP chunk")
        frames: list[tuple[int, int, int]] = []
        for _ in range(num_frames):
            frames.append(struct.unpack_from("<HHB", body, pos))
            pos += 5
        clips.append(Clip(name=name, loop_mode=loop_mode, frames=frames))
    return clips
