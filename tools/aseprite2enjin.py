#!/usr/bin/env python3
"""aseprite2enjin.py — Convert indexed-color .aseprite files to enjin C headers.

Parses the Aseprite binary format (ASE file spec) using Python stdlib only.
Outputs a C header with a const uint8_t array compatible with enjin2::SpriteSheet.
"""

import struct
import zlib
import os
import sys
import argparse

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from enjin_assets import emit as _emit  # noqa: E402  (shared v2 .njn writer)

# ---------------------------------------------------------------------------
# ASE format constants
# ---------------------------------------------------------------------------
ASE_MAGIC        = 0xA5E0
FRAME_MAGIC      = 0xF1FA
CHUNK_LAYER      = 0x2004
CHUNK_CEL        = 0x2005
CHUNK_CEL_EXTRA  = 0x2006
CHUNK_FRAME_TAGS = 0x2018
CHUNK_PALETTE    = 0x2019
COLOR_DEPTH_INDEXED = 8
COLOR_DEPTH_RGBA = 32

LAYER_FLAG_VISIBLE = 0x01
LAYER_TYPE_IMAGE = 0
LAYER_TYPE_GROUP = 1
LAYER_TYPE_TILEMAP = 2
BLEND_MODE_NORMAL = 0

# Aseprite tag loop directions → enjin NjnLoopMode (reverse folds to Loop).
_ASE_DIR_TO_LOOP = {0: _emit.LOOP_LOOP, 1: _emit.LOOP_LOOP, 2: _emit.LOOP_PINGPONG, 3: _emit.LOOP_PINGPONG}

CEL_TYPE_RAW        = 0
CEL_TYPE_LINKED     = 1
CEL_TYPE_COMPRESSED = 2

TRANSPARENT_INDEX = 15

# Number of opaque Enjin palette indices (0..14); index 15 is transparency.
# Mirrors enjin_assets.palette.OPAQUE_COUNT without importing numpy into the
# converter's common stdlib-only path.
OPAQUE_COLOR_COUNT = 15

# ---------------------------------------------------------------------------
# Tilemap cell packing — the single source of truth mirrors
# enjin2/graphics/tilemap_asset.hpp: [band:1 | vflip:1 | hflip:1 | palbank:4 | tileid:9]
# ---------------------------------------------------------------------------
TM_TILEID_MASK   = 0x01FF
TM_PALBANK_SHIFT = 9
TM_PALBANK_MASK  = 0x000F
TM_HFLIP_BIT     = 1 << 13
TM_VFLIP_BIT     = 1 << 14
TM_BAND_BIT      = 1 << 15


def pack_cell(tile_id, band=0, palbank=0, hflip=False, vflip=False):
    """Pack a tilemap cell to a uint16 (matches tmPackCell in tilemap_asset.hpp)."""
    return (
        (tile_id & TM_TILEID_MASK)
        | ((palbank & TM_PALBANK_MASK) << TM_PALBANK_SHIFT)
        | (TM_HFLIP_BIT if hflip else 0)
        | (TM_VFLIP_BIT if vflip else 0)
        | (TM_BAND_BIT if band else 0)
    ) & 0xFFFF


# ---------------------------------------------------------------------------
# ASE parser
# ---------------------------------------------------------------------------

def _read_aseprite(path: str, want_palette: bool = False):
    """Parse an .aseprite into raw frame cels (the shared conversion front end).

    Returns a dict with canvas metadata, parsed layer metadata, the raw
    per-frame cel tables (``frame_cels``), per-frame durations, tags, the
    cumulative indexed palette, and narrow warnings.  Flattening (the flat
    exporter) and layered assembly are the callers' concern.
    """
    with open(path, 'rb') as f:
        data = f.read()

    # --- File header (128 bytes) ---
    if len(data) < 128:
        raise ValueError("File too small to be a valid .aseprite file")

    file_size, magic = struct.unpack_from('<IH', data, 0)
    if magic != ASE_MAGIC:
        raise ValueError(f"Not a valid .aseprite file (bad magic: 0x{magic:04X}, expected 0x{ASE_MAGIC:04X})")
    if file_size > len(data):
        raise ValueError(f"Truncated .aseprite file (header says {file_size} bytes, found {len(data)})")

    frame_count, width, height, color_depth = struct.unpack_from('<HHHH', data, 6)
    file_flags = struct.unpack_from('<I', data, 14)[0]
    # speed at offset+18 — skip
    transparent_index = data[28] if color_depth == COLOR_DEPTH_INDEXED else None
    # number of colors at offset+32 (2 bytes)

    if color_depth not in (COLOR_DEPTH_INDEXED, COLOR_DEPTH_RGBA):
        raise ValueError(
            f"Only indexed and RGBA sprites are supported (found {color_depth}-bit)"
        )

    offset = 128  # skip to first frame
    layers = []
    frame_cels = []
    durations = []   # per-frame hold time in ms
    tags = []        # list of (from_frame, to_frame, loop_dir, name)
    warnings = []
    palette = {}
    frame_palettes = []

    for frame_idx in range(frame_count):
        if offset + 16 > len(data):
            raise ValueError(f"Truncated frame header at frame {frame_idx}")

        # --- Frame header (16 bytes) ---
        frame_size, frame_magic, num_chunks_old, frame_duration = struct.unpack_from('<IHHH', data, offset)
        # new chunk count at offset+12 (4 bytes), replaces num_chunks_old when != 0xFFFF
        num_chunks_new = struct.unpack_from('<I', data, offset + 12)[0]

        if frame_magic != FRAME_MAGIC:
            raise ValueError(f"Bad frame magic at frame {frame_idx}: 0x{frame_magic:04X}")

        num_chunks = num_chunks_new if num_chunks_old == 0xFFFF else num_chunks_old

        frame_end = offset + frame_size
        if frame_size < 16 or frame_end > len(data):
            raise ValueError(f"Invalid or truncated frame {frame_idx}")
        chunk_offset = offset + 16  # first chunk starts after 16-byte frame header
        cels = {}

        for _ in range(num_chunks):
            if chunk_offset + 6 > frame_end:
                raise ValueError(f"Truncated chunk header in frame {frame_idx}")

            chunk_size, chunk_type = struct.unpack_from('<IH', data, chunk_offset)
            if chunk_size < 6 or chunk_offset + chunk_size > frame_end:
                raise ValueError(f"Invalid chunk size in frame {frame_idx}")

            chunk_data_offset = chunk_offset + 6
            chunk_body_size   = chunk_size - 6

            if chunk_type == CHUNK_LAYER:
                layer = _parse_layer(
                    data, chunk_data_offset, chunk_body_size,
                    layer_opacity_valid=bool(file_flags & 0x01)
                )
                layer['index'] = len(layers)
                layers.append(layer)
            elif chunk_type == CHUNK_CEL:
                cel = _parse_cel(
                    data, chunk_data_offset, chunk_body_size, color_depth,
                    frame_idx, warnings
                )
                if cel['layer_index'] in cels:
                    raise ValueError(
                        f"Multiple cels for layer {cel['layer_index']} in frame {frame_idx}"
                    )
                cels[cel['layer_index']] = cel
            elif chunk_type == CHUNK_FRAME_TAGS:
                tags.extend(_parse_frame_tags(data, chunk_data_offset, chunk_body_size))
            elif (chunk_type == CHUNK_PALETTE
                  and color_depth == COLOR_DEPTH_INDEXED and want_palette):
                _parse_palette_update(data, chunk_data_offset, chunk_body_size, palette)

            chunk_offset += chunk_size

        if chunk_offset != frame_end:
            raise ValueError(f"Frame {frame_idx} chunk data does not match its declared size")
        frame_cels.append(cels)
        frame_palettes.append(dict(palette))
        durations.append(frame_duration)
        offset = frame_end

    for frame_idx, cels in enumerate(frame_cels):
        unknown = set(cels) - set(range(len(layers)))
        if unknown:
            raise ValueError(f"Frame {frame_idx} references unknown layer {min(unknown)}")

    return {
        'width':       width,
        'height':      height,
        'color_depth': color_depth,
        'transparent_index': transparent_index,
        'frame_count': frame_count,
        'frame_cels':  frame_cels,
        'frame_palettes': frame_palettes,
        'durations':   durations,
        'tags':        tags,
        'layers':      layers,
        'warnings':    warnings,
    }


def parse_aseprite(path: str, rgba_output=False):
    """Parse an .aseprite file and flatten it to full-canvas frames.

    Returns a dict with:
        width, height         -- canvas size in pixels
        color_depth           -- 8 (indexed) or 32 (RGBA)
        frame_count           -- number of animation frames
        frames                -- indexed bytes by default; RGBA bytes for RGBA
                                 input or when rgba_output=True
        frame_format          -- "indexed" or "rgba"
        layers                -- parsed layer metadata in bottom-to-top order
        warnings              -- narrowly recovered input issues
    """
    raw = _read_aseprite(path, want_palette=rgba_output)
    color_depth = raw['color_depth']
    layers = raw['layers']
    frame_cels = raw['frame_cels']
    _validate_layers(layers)

    if color_depth == COLOR_DEPTH_INDEXED and not rgba_output:
        has_layer_opacity = any(
            layer['visible'] and layer['opacity'] != 255 for layer in layers
        )
        has_cel_opacity = any(
            cel['opacity'] != 255 for cels in frame_cels for cel in cels.values()
        )
        if has_layer_opacity or has_cel_opacity:
            raise ValueError(
                "Indexed output cannot preserve layer or cel opacity; "
                "parse with rgba_output=True and quantize the flattened result"
            )

    output_rgba = color_depth == COLOR_DEPTH_RGBA or rgba_output
    frames = []
    for frame_idx in range(raw['frame_count']):
        frames.append(_flatten_frame(
            frame_idx, frame_cels, layers, raw['width'], raw['height'], color_depth,
            raw['transparent_index'], output_rgba,
            raw['frame_palettes'][frame_idx] if color_depth == COLOR_DEPTH_INDEXED else None,
        ))

    return {
        'width':       raw['width'],
        'height':      raw['height'],
        'color_depth': color_depth,
        'frame_format': 'rgba' if output_rgba else 'indexed',
        'transparent_index': raw['transparent_index'],
        'frame_count': len(frames),
        'frames':      frames,
        'durations':   raw['durations'],
        'tags':        raw['tags'],
        'layers':      layers,
        'warnings':    raw['warnings'],
    }


def parse_aseprite_layered(path: str):
    """Parse an .aseprite preserving raw per-frame cels for ``--layered``.

    Unlike :func:`parse_aseprite`, this neither flattens nor rejects layer/cel
    opacity; :func:`build_layered_asset` performs the layered path's own precise
    validation.
    """
    return _read_aseprite(path)


def _parse_palette_update(data, offset, body_size, palette):
    """Apply one modern PALETTE chunk (0x2019) to the current palette."""
    end = offset + body_size
    if body_size < 20:
        raise ValueError("Truncated palette chunk")
    _palette_size, first, last = struct.unpack_from('<III', data, offset)
    if last < first or last > 255:
        raise ValueError(f"Invalid palette update range {first}..{last}")
    pos = offset + 20  # fixed fields followed by eight reserved bytes
    for index in range(first, last + 1):
        if pos + 6 > end:
            raise ValueError(f"Truncated palette entry {index}")
        flags = struct.unpack_from('<H', data, pos)[0]
        palette[index] = tuple(data[pos + 2:pos + 6])
        pos += 6
        if flags & 0x01:
            if pos + 2 > end:
                raise ValueError(f"Truncated palette entry name {index}")
            name_len = struct.unpack_from('<H', data, pos)[0]
            pos += 2
            if pos + name_len > end:
                raise ValueError(f"Truncated palette entry name {index}")
            pos += name_len
    if pos != end:
        raise ValueError("Palette chunk has trailing data")


def _parse_layer(data, offset, body_size, layer_opacity_valid):
    """Parse the fixed and common variable fields of a LAYER chunk."""
    if body_size < 18:
        raise ValueError("Truncated layer chunk")
    flags, layer_type, child_level, _w, _h, blend_mode, opacity = struct.unpack_from(
        '<HHHHHHB', data, offset
    )
    name_len = struct.unpack_from('<H', data, offset + 16)[0]
    if 18 + name_len > body_size:
        raise ValueError("Truncated layer name")
    raw_name = data[offset + 18:offset + 18 + name_len]
    try:
        name = raw_name.decode('utf-8')
    except UnicodeDecodeError:
        name = raw_name.decode('latin-1', 'replace')
    return {
        'name': name,
        'visible': bool(flags & LAYER_FLAG_VISIBLE),
        'flags': flags,
        'type': layer_type,
        'child_level': child_level,
        'blend_mode': blend_mode,
        'opacity': opacity if layer_opacity_valid else 255,
    }


def _validate_layers(layers):
    for layer in layers:
        label = layer['name'] or str(layer['index'])
        if layer['type'] == LAYER_TYPE_GROUP or layer['child_level']:
            raise ValueError(f"Group layer semantics are unsupported (layer {label!r})")
        if layer['type'] == LAYER_TYPE_TILEMAP:
            raise ValueError(f"Tilemap layers are unsupported (layer {label!r})")
        if layer['type'] != LAYER_TYPE_IMAGE:
            raise ValueError(f"Unsupported layer type {layer['type']} (layer {label!r})")
        if layer['blend_mode'] != BLEND_MODE_NORMAL:
            raise ValueError(
                f"Unsupported blend mode {layer['blend_mode']} on layer {label!r}; "
                "only normal blend mode is supported"
            )


def _strict_zlib_decompress(payload):
    decoder = zlib.decompressobj()
    pixels = decoder.decompress(payload) + decoder.flush()
    if not decoder.eof or decoder.unused_data or decoder.unconsumed_tail:
        raise zlib.error("incomplete stream or trailing compressed data")
    return pixels


def _decompress_cel(payload, expected_size, frame_idx, layer_index, warnings):
    try:
        pixels = _strict_zlib_decompress(payload)
    except zlib.error as original_error:
        # Pixquare writes a complete zlib deflate stream but omits only Adler-32.
        # Accept it only if decoding consumes all input, yields the exact payload,
        # and adding the computed trailer turns it into a strict zlib stream.
        try:
            decoder = zlib.decompressobj()
            candidate = decoder.decompress(payload) + decoder.flush()
            repairable = (
                not decoder.eof
                and not decoder.unused_data
                and not decoder.unconsumed_tail
                and len(candidate) == expected_size
            )
            if not repairable:
                raise zlib.error("not a missing-Adler-32 stream")
            trailer = struct.pack('>I', zlib.adler32(candidate) & 0xFFFFFFFF)
            pixels = _strict_zlib_decompress(payload + trailer)
            if pixels != candidate:
                raise zlib.error("repaired stream changed output")
        except zlib.error:
            raise ValueError(
                f"Corrupt compressed cel in frame {frame_idx}, layer {layer_index}: "
                f"{original_error}"
            ) from original_error
        warnings.append(
            f"Repaired missing Adler-32 in frame {frame_idx}, layer {layer_index}"
        )
    if len(pixels) != expected_size:
        raise ValueError(
            f"Cel in frame {frame_idx}, layer {layer_index} has {len(pixels)} pixel bytes; "
            f"expected {expected_size}"
        )
    return pixels


def _parse_cel(data, offset, body_size, color_depth, frame_idx, warnings):
    """Parse the spec's 16-byte cel header and its type-specific payload."""
    if body_size < 16:
        raise ValueError(f"Truncated cel header in frame {frame_idx}")
    layer_index, x, y, opacity, cel_type, z_index = struct.unpack_from(
        '<HhhBHh', data, offset
    )
    pos = offset + 16  # z-index is followed by five reserved bytes
    end = offset + body_size
    cel = {
        'layer_index': layer_index,
        'x': x,
        'y': y,
        'opacity': opacity,
        'type': cel_type,
        'z_index': z_index,
    }
    if cel_type == CEL_TYPE_LINKED:
        if pos + 2 != end:
            raise ValueError(f"Invalid linked cel payload in frame {frame_idx}")
        cel['linked_frame'] = struct.unpack_from('<H', data, pos)[0]
        return cel
    if cel_type not in (CEL_TYPE_RAW, CEL_TYPE_COMPRESSED):
        raise ValueError(f"Unsupported cel type {cel_type} in frame {frame_idx}")
    if pos + 4 > end:
        raise ValueError(f"Truncated cel dimensions in frame {frame_idx}")
    cel_w, cel_h = struct.unpack_from('<HH', data, pos)
    payload = data[pos + 4:end]
    bytes_per_pixel = 4 if color_depth == COLOR_DEPTH_RGBA else 1
    expected_size = cel_w * cel_h * bytes_per_pixel
    if cel_type == CEL_TYPE_COMPRESSED:
        pixels = _decompress_cel(
            payload, expected_size, frame_idx, layer_index, warnings
        )
    else:
        if len(payload) != expected_size:
            raise ValueError(
                f"Raw cel in frame {frame_idx}, layer {layer_index} has "
                f"{len(payload)} pixel bytes; expected {expected_size}"
            )
        pixels = payload
    cel.update({'width': cel_w, 'height': cel_h, 'pixels': pixels})
    return cel


def _resolve_cel(frame_idx, layer_index, frame_cels, seen=None):
    cel = frame_cels[frame_idx].get(layer_index)
    if cel is None or cel['type'] != CEL_TYPE_LINKED:
        return cel
    source_frame = cel['linked_frame']
    if source_frame >= len(frame_cels):
        raise ValueError(f"Linked cel in frame {frame_idx} references frame {source_frame}")
    key = (frame_idx, layer_index)
    seen = set() if seen is None else seen
    if key in seen:
        raise ValueError(f"Linked cel cycle at frame {frame_idx}, layer {layer_index}")
    seen.add(key)
    source = _resolve_cel(source_frame, layer_index, frame_cels, seen)
    if source is None:
        raise ValueError(
            f"Linked cel in frame {frame_idx}, layer {layer_index} has no source cel"
        )
    resolved = dict(source)
    resolved.update({
        'x': cel['x'], 'y': cel['y'], 'opacity': cel['opacity'],
        'z_index': cel['z_index'],
    })
    return resolved


def _flatten_frame(frame_idx, frame_cels, layers, width, height, color_depth,
                   transparent_index=None, output_rgba=False, palette=None):
    if output_rgba:
        canvas = bytearray(width * height * 4)
    else:
        canvas = bytearray([transparent_index] * (width * height))

    render_cels = []
    for layer in layers:
        if not layer['visible']:
            continue
        cel = _resolve_cel(frame_idx, layer['index'], frame_cels)
        if cel is not None:
            render_cels.append((layer['index'] + cel['z_index'], layer['index'], layer, cel))
    render_cels.sort(key=lambda item: (item[0], item[1]))

    for _order, _index, layer, cel in render_cels:
        if color_depth == COLOR_DEPTH_RGBA:
            _composite_rgba(canvas, width, height, cel, layer['opacity'])
        elif output_rgba:
            _composite_indexed_rgba(
                canvas, width, height, cel, layer['opacity'], palette,
                transparent_index, frame_idx,
            )
        else:
            _composite_indexed(canvas, width, height, cel, transparent_index)
    return bytes(canvas)


def _composite_indexed(canvas, canvas_w, canvas_h, cel, transparent_index):
    for row in range(cel['height']):
        for col in range(cel['width']):
            cx, cy = cel['x'] + col, cel['y'] + row
            pixel = cel['pixels'][row * cel['width'] + col]
            if 0 <= cx < canvas_w and 0 <= cy < canvas_h and pixel != transparent_index:
                canvas[cy * canvas_w + cx] = pixel


def _composite_indexed_rgba(canvas, canvas_w, canvas_h, cel, layer_opacity,
                            palette, transparent_index, frame_idx):
    rgba_pixels = bytearray(cel['width'] * cel['height'] * 4)
    for row in range(cel['height']):
        for col in range(cel['width']):
            cx, cy = cel['x'] + col, cel['y'] + row
            if not (0 <= cx < canvas_w and 0 <= cy < canvas_h):
                continue
            src_pos = row * cel['width'] + col
            index = cel['pixels'][src_pos]
            if index == transparent_index:
                continue
            color = palette.get(index) if palette is not None else None
            if color is None:
                raise ValueError(
                    f"Missing palette color for index {index} used in frame {frame_idx}"
                )
            rgba_pos = src_pos * 4
            rgba_pixels[rgba_pos:rgba_pos + 4] = bytes(color)
    rgba_cel = dict(cel)
    rgba_cel['pixels'] = rgba_pixels
    _composite_rgba(canvas, canvas_w, canvas_h, rgba_cel, layer_opacity)


def _composite_rgba(canvas, canvas_w, canvas_h, cel, layer_opacity):
    opacity = (cel['opacity'] * layer_opacity + 127) // 255
    for row in range(cel['height']):
        for col in range(cel['width']):
            cx, cy = cel['x'] + col, cel['y'] + row
            if not (0 <= cx < canvas_w and 0 <= cy < canvas_h):
                continue
            src_pos = (row * cel['width'] + col) * 4
            dst_pos = (cy * canvas_w + cx) * 4
            sr, sg, sb, src_alpha = cel['pixels'][src_pos:src_pos + 4]
            sa = (src_alpha * opacity + 127) // 255
            if sa == 0:
                continue
            dr, dg, db, da = canvas[dst_pos:dst_pos + 4]
            alpha_numerator = sa * 255 + da * (255 - sa)
            out_alpha = (alpha_numerator + 127) // 255
            for channel, src, dst in ((0, sr, dr), (1, sg, dg), (2, sb, db)):
                numerator = src * sa * 255 + dst * da * (255 - sa)
                canvas[dst_pos + channel] = (numerator + alpha_numerator // 2) // alpha_numerator
            canvas[dst_pos + 3] = out_alpha


def _parse_frame_tags(data, offset, body_size):
    """Parse a FRAME_TAGS chunk (0x2018) → list of (from, to, loop_dir, name).

    Layout: WORD numTags, BYTE[8] reserved, then per tag: WORD from, WORD to,
    BYTE loopDir, WORD repeat, BYTE[6] reserved, BYTE[3] colour, BYTE extra,
    STRING name (WORD length + utf-8 bytes).
    """
    end = offset + body_size
    if offset + 10 > end:
        return []
    num_tags = struct.unpack_from('<H', data, offset)[0]
    pos = offset + 2 + 8  # skip numTags + 8 reserved bytes
    out = []
    for _ in range(num_tags):
        if pos + 17 > end:
            break
        from_frame, to_frame = struct.unpack_from('<HH', data, pos)
        loop_dir = data[pos + 4]
        pos += 4 + 1 + 2 + 6 + 3 + 1  # from,to + dir + repeat + reserved + colour + extra
        if pos + 2 > end:
            break
        name_len = struct.unpack_from('<H', data, pos)[0]
        pos += 2
        raw = data[pos:pos + name_len]
        pos += name_len
        try:
            name = raw.decode('utf-8')
        except UnicodeDecodeError:
            name = raw.decode('latin-1', 'replace')
        out.append((from_frame, to_frame, loop_dir, name))
    return out


def _process_cel_chunk(data, offset, body_size, canvas, canvas_w, canvas_h):
    """Read a CEL chunk and composite pixel data onto the canvas buffer.

    ASE spec cel header layout:
      layer_index (WORD=2) + x (SHORT=2) + y (SHORT=2) + opacity (BYTE=1) + cel_type (WORD=2) = 9 bytes
      reserved (7 bytes) — present in files produced by Aseprite, omitted in minimal/test files

    We support both variants by computing the offset from chunk_body_size: if body_size >= 16+4
    we assume the 7 reserved bytes are present; otherwise we assume they are absent.
    """
    if body_size < 9:
        return  # too small to be a valid cel

    layer_index, x, y, opacity, cel_type = struct.unpack_from('<HhhBH', data, offset)

    # Detect whether 7 reserved bytes are present.
    # ASE spec: header=9 bytes + 7 reserved bytes = 16 bytes, then type-specific data.
    # Real Aseprite files: reserved bytes are all 0x00.
    # Minimal/test files may omit the reserved bytes entirely.
    # Heuristic: if the 7 bytes at offset+9 are all zero, treat as real file (offset+16);
    # otherwise assume the reserved bytes are absent (offset+9).
    reserved_region = data[offset + 9: offset + 16]
    if len(reserved_region) == 7 and all(b == 0 for b in reserved_region):
        cel_data_offset = offset + 16  # real Aseprite file with 7 reserved bytes
    else:
        cel_data_offset = offset + 9   # minimal file without reserved bytes

    if cel_type in (CEL_TYPE_RAW, CEL_TYPE_COMPRESSED):
        # Both types: width(WORD) + height(WORD) + pixel data (raw or zlib-compressed)
        if cel_data_offset + 4 > offset + body_size:
            return
        cel_w, cel_h = struct.unpack_from('<HH', data, cel_data_offset)
        pixel_offset = cel_data_offset + 4
        available    = (offset + body_size) - pixel_offset
        if available <= 0:
            return
        raw_data = data[pixel_offset: pixel_offset + available]

        if cel_type == CEL_TYPE_COMPRESSED:
            try:
                pixels = zlib.decompress(raw_data)
            except zlib.error:
                return  # skip corrupt cel
        else:
            # CEL_TYPE_RAW: try zlib first (handles files that mis-label cel type),
            # fall back to treating as raw bytes.
            try:
                pixels = zlib.decompress(raw_data)
            except zlib.error:
                pixel_count = cel_w * cel_h
                pixels = raw_data[:pixel_count]

        _composite(canvas, canvas_w, canvas_h, pixels, cel_w, cel_h, x, y)

    elif cel_type == CEL_TYPE_LINKED:
        # Linked cels point to another frame; skip for conversion purposes.
        pass


def _composite(canvas, canvas_w, canvas_h, pixels, cel_w, cel_h, x, y):
    """Blit cel pixels onto the canvas buffer at position (x, y)."""
    for row in range(cel_h):
        for col in range(cel_w):
            cx = x + col
            cy = y + row
            if 0 <= cx < canvas_w and 0 <= cy < canvas_h:
                px_idx = row * cel_w + col
                if px_idx < len(pixels):
                    canvas[cy * canvas_w + cx] = pixels[px_idx]


# ---------------------------------------------------------------------------
# Layered parse (tilemap authoring: one canvas per Aseprite layer, frame 0)
# ---------------------------------------------------------------------------

def _read_layer_name(data, offset, body_size):
    """Read a LAYER chunk (0x2004) name. Returns the layer name string.

    Layout: flags(2) type(2) child(2) w(2) h(2) blend(2) opacity(1) reserved(3)
            name(STRING = len WORD + utf8 bytes).
    """
    if body_size < 18:
        return ""
    name_len = struct.unpack_from('<H', data, offset + 16)[0]
    start = offset + 18
    raw = data[start:start + name_len]
    try:
        return raw.decode('utf-8')
    except UnicodeDecodeError:
        return raw.decode('latin-1', 'replace')


def parse_aseprite_layers(path):
    """Parse frame 0 of an .aseprite file into one canvas per layer.

    Returns a dict:
        width, height  -- canvas size in pixels
        layers         -- list of {'name': str, 'pixels': bytes} in file order
                          (bottom layer first, top layer last)
    """
    with open(path, 'rb') as f:
        data = f.read()

    if len(data) < 128:
        raise ValueError("File too small to be a valid .aseprite file")

    _, magic = struct.unpack_from('<IH', data, 0)
    if magic != ASE_MAGIC:
        raise ValueError(f"Not a valid .aseprite file (bad magic: 0x{magic:04X})")

    _frame_count, width, height, color_depth = struct.unpack_from('<HHHH', data, 6)
    if color_depth != COLOR_DEPTH_INDEXED:
        raise ValueError(
            f"Only indexed-color sprites are supported (found {color_depth}-bit). "
            f"In Aseprite: Sprite > Color Mode > Indexed."
        )

    # --- Frame 0 only ---
    offset = 128
    if offset + 16 > len(data):
        raise ValueError("Missing first frame")

    frame_size, frame_magic, num_chunks_old, _dur = struct.unpack_from('<IHHH', data, offset)
    num_chunks_new = struct.unpack_from('<I', data, offset + 12)[0]
    if frame_magic != FRAME_MAGIC:
        raise ValueError(f"Bad frame magic: 0x{frame_magic:04X}")
    num_chunks = num_chunks_new if num_chunks_old == 0xFFFF else num_chunks_old

    frame_end = offset + frame_size
    chunk_offset = offset + 16

    layer_names = []           # index -> name, in LAYER-chunk order
    layer_pixels = {}          # layer_index -> bytearray canvas

    for _ in range(num_chunks):
        if chunk_offset + 6 > frame_end:
            break
        chunk_size, chunk_type = struct.unpack_from('<IH', data, chunk_offset)
        if chunk_size < 6:
            break
        body_off = chunk_offset + 6
        body_size = chunk_size - 6

        if chunk_type == CHUNK_LAYER:
            layer_names.append(_read_layer_name(data, body_off, body_size))
        elif chunk_type == CHUNK_CEL:
            li = struct.unpack_from('<H', data, body_off)[0]
            canvas = layer_pixels.get(li)
            if canvas is None:
                canvas = bytearray([TRANSPARENT_INDEX] * (width * height))
                layer_pixels[li] = canvas
            _process_cel_chunk(data, body_off, body_size, canvas, width, height)

        chunk_offset += chunk_size

    layers = []
    for idx, name in enumerate(layer_names):
        px = layer_pixels.get(idx)
        if px is None:
            px = bytearray([TRANSPARENT_INDEX] * (width * height))
        layers.append({'name': name, 'pixels': bytes(px)})

    return {'width': width, 'height': height, 'layers': layers}


# ---------------------------------------------------------------------------
# Tilemap builder: dice under/over layers into a deduplicated tileset + .njm map
# ---------------------------------------------------------------------------

def _extract_tile(pixels, canvas_w, canvas_h, tx, ty, tile_w, tile_h):
    """Return the tile_w*tile_h nibbles of grid cell (tx,ty), row-major."""
    out = bytearray()
    for py in range(tile_h):
        for px in range(tile_w):
            sx = tx * tile_w + px
            sy = ty * tile_h + py
            if sx < canvas_w and sy < canvas_h:
                out.append(pixels[sy * canvas_w + sx] & 0x0F)
            else:
                out.append(TRANSPARENT_INDEX)
    return bytes(out)


def _tile_is_empty(tile):
    """A tile is empty when every pixel is the transparent index."""
    return all(b == TRANSPARENT_INDEX for b in tile)


def _classify_layers(layers):
    """Split layers into (under_layer, over_layer) by name, else by z-order.

    A layer whose name contains 'over' is the over band; 'under' is the under
    band. Absent names fall back to z-order: bottom layer = under, top = over.
    """
    under = over = None
    for layer in layers:
        name = layer['name'].lower()
        if 'over' in name:
            over = layer
        elif 'under' in name or 'floor' in name or 'base' in name:
            under = layer
    if under is None or over is None:
        # z-order fallback: first (bottom) = under, last (top) = over
        if under is None:
            under = layers[0] if layers else None
        if over is None and len(layers) >= 2:
            over = layers[-1]
    return under, over


def build_tilemap(layers, canvas_w, canvas_h, tile_w, tile_h):
    """Dice under/over layers into a deduplicated tileset + packed .njm cells.

    Returns (tileset_pixels, cell_count, cols, rows, map_cells) where:
      tileset_pixels -- bytes, frame-major (frame 0 = transparent), 1 byte/px
      cell_count     -- number of tileset frames (incl. the transparent frame 0)
      cols, rows     -- grid tiles across / down (map dimensions)
      map_cells      -- list of packed uint16 cells, row-major

    A grid cell takes the OVER-band tile when the over layer has art there,
    else the UNDER-band tile, else it is empty (cell 0). Only referenced tiles
    enter the tileset; ids are assigned in row-major first-appearance order.
    """
    cols = canvas_w // tile_w
    rows = canvas_h // tile_h
    under, over = _classify_layers(layers)

    frame_size = tile_w * tile_h
    empty_tile = bytes([TRANSPARENT_INDEX] * frame_size)

    tileset = [empty_tile]          # frame 0 = transparent (the "wasted" frame)
    tile_ids = {empty_tile: 0}      # dedup: tile bytes -> id
    map_cells = []

    for ty in range(rows):
        for tx in range(cols):
            over_tile = (_extract_tile(over['pixels'], canvas_w, canvas_h,
                                       tx, ty, tile_w, tile_h) if over else empty_tile)
            under_tile = (_extract_tile(under['pixels'], canvas_w, canvas_h,
                                        tx, ty, tile_w, tile_h) if under else empty_tile)

            if not _tile_is_empty(over_tile):
                band, tile = 1, over_tile
            elif not _tile_is_empty(under_tile):
                band, tile = 0, under_tile
            else:
                map_cells.append(0)   # empty cell
                continue

            tid = tile_ids.get(tile)
            if tid is None:
                tid = len(tileset)
                tileset.append(tile)
                tile_ids[tile] = tid
            map_cells.append(pack_cell(tid, band=band))

    tileset_pixels = b"".join(tileset)
    return tileset_pixels, len(tileset), cols, rows, map_cells


# ---------------------------------------------------------------------------
# .njn tileset + .njm map binary emitters (see sprite_asset.hpp / tilemap_asset.hpp)
# ---------------------------------------------------------------------------

def emit_njn_bytes(pixel_data, cell_w, cell_h, cols, rows):
    """Encode a tileset as .njn bytes: 8-byte 'NJ' header + 1 byte/pixel."""
    if cols > 255 or rows > 255:
        raise ValueError(f"tileset grid {cols}x{rows} exceeds the 255x255 .njn header limit")
    header = struct.pack('<2sBBBBBB', b'NJ', 1, cell_w & 0xFF, cell_h & 0xFF,
                         cols & 0xFF, rows & 0xFF, 0)
    return header + bytes(b & 0x0F for b in pixel_data)


def emit_njm_bytes(cells, map_w, map_h):
    """Encode a map as .njm bytes: 8-byte 'NM' header + little-endian uint16 cells."""
    if map_w > 255 or map_h > 255:
        raise ValueError(f"map {map_w}x{map_h} exceeds the 255x255 .njm header limit")
    header = struct.pack('<2sBBBBBB', b'NM', 1, 0, map_w & 0xFF, map_h & 0xFF, 0, 0)
    body = b"".join(struct.pack('<H', c & 0xFFFF) for c in cells)
    return header + body


# ---------------------------------------------------------------------------
# .njn v2 sheet emitter — animation clips from Aseprite tags (issue #87)
# ---------------------------------------------------------------------------

def build_clips_from_tags(tags, durations, frame_count):
    """Turn Aseprite frame tags into enjin CLIP records (per-frame durations)."""
    clips = []
    for from_frame, to_frame, loop_dir, name in tags:
        lo = max(0, from_frame)
        hi = min(frame_count - 1, to_frame)
        if hi < lo:
            continue
        frames = [
            (fi, durations[fi] if fi < len(durations) else 100, 0)
            for fi in range(lo, hi + 1)
        ]
        clips.append(_emit.Clip(
            name=name or f"clip{len(clips)}",
            loop_mode=_ASE_DIR_TO_LOOP.get(loop_dir, _emit.LOOP_LOOP),
            frames=frames,
        ))
    return clips


def emit_njn_v2_sheet(ase, grid_spec):
    """Build a .njn v2 sheet (META + PIXL + optional CLIP) from a parsed ASE file.

    Returns ``(bytes, frame_count, clips)``. Frames become sheet cells in order;
    CLIP frame indices reference those cells, so a clip is just a run of cells.
    """
    pixel_data, cell_w, cell_h, cols, rows = build_pixel_array(
        ase['frames'], ase['width'], ase['height'], grid_spec,
        ase.get('transparent_index', TRANSPARENT_INDEX),
    )
    frame_count = cols * rows
    # --grid slices one Aseprite frame into sheet cells (a static spritesheet), so
    # frame-tag clips — which index the animation frames that grid mode discards —
    # do not apply. Only build clips when the frames themselves are the cells.
    if grid_spec is not None:
        if ase.get('tags'):
            print("Warning: --grid ignores Aseprite frame tags (no CLIP chunk emitted)",
                  file=sys.stderr)
        clips = None
    else:
        clips = build_clips_from_tags(ase.get('tags', []), ase.get('durations', []), frame_count) or None
    data = _emit.build_njn(cell_w, cell_h, pixel_data, frame_count, clips=clips)
    return data, frame_count, clips


def _run_sprite_v2(args, input_path):
    """v2 sheet authoring path: emit a .njn v2 container with animation clips."""
    try:
        ase = parse_aseprite(input_path)
    except ValueError as e:
        print(f"Error: {e}", file=sys.stderr)
        sys.exit(1)
    if ase['color_depth'] != COLOR_DEPTH_INDEXED:
        print("Error: .njn conversion requires indexed input; RGBA parsing does not quantize", file=sys.stderr)
        sys.exit(1)

    try:
        data, frame_count, clips = emit_njn_v2_sheet(ase, args.grid)
    except ValueError as e:
        print(f"Error: {e}", file=sys.stderr)
        sys.exit(1)
    out_path = args.output or (os.path.splitext(input_path)[0] + ".njn")
    os.makedirs(os.path.dirname(os.path.abspath(out_path)), exist_ok=True)
    with open(out_path, 'wb') as f:
        f.write(data)
    n_clips = len(clips) if clips else 0
    print(f"Written: {out_path}  ({len(data)} bytes, .njn v2, {frame_count} frames, {n_clips} clips)")
    if clips:
        for c in clips:
            print(f"  clip {c.name!r}: {len(c.frames)} frames, loop={c.loop_mode}")


# ---------------------------------------------------------------------------
# Layered sprite (.njn v2 layered chunks, issue #94)
# ---------------------------------------------------------------------------

def _clip_cel_pixels(pixels, cel_x, cel_y, cel_w, cel_h,
                     canvas_w, canvas_h, bytes_per_pixel):
    """Clip a cel to the authored canvas, returning the in-canvas rectangle.

    Returns ``(w, h, pixels, origin_x, origin_y)`` with the origin in canvas
    coordinates, or ``None`` when the cel lies wholly outside the canvas.
    Validation and bounds operate on this region, so off-canvas artwork never
    inflates storage, rejects a colour, or moves a part.
    """
    x0 = max(0, cel_x)
    y0 = max(0, cel_y)
    x1 = min(canvas_w, cel_x + cel_w)
    y1 = min(canvas_h, cel_y + cel_h)
    if x1 <= x0 or y1 <= y0:
        return None

    w = x1 - x0
    h = y1 - y0
    src_x = x0 - cel_x
    src_y = y0 - cel_y
    out = bytearray(w * h * bytes_per_pixel)
    for row in range(h):
        src = ((src_y + row) * cel_w + src_x) * bytes_per_pixel
        dst = row * w * bytes_per_pixel
        out[dst:dst + w * bytes_per_pixel] = pixels[src:src + w * bytes_per_pixel]
    return w, h, bytes(out), x0, y0


def _rgba_palette_lookup(parsed, visible, target_palette, canvas_w, canvas_h):
    """Validate in-canvas RGBA pixels against a target palette.

    Every painted pixel must be binary-alpha and exactly match one of the 15
    opaque target colours.  Absent colours are collected and reported together
    so an author can fix the palette in one pass; no quantisation is performed.
    Off-canvas pixels are clipped away first and never influence validation.
    """
    palette = [tuple(int(channel) & 0xFF for channel in entry[:3])
               for entry in target_palette]
    if len(palette) != OPAQUE_COLOR_COUNT:
        raise ValueError(
            f"target palette must have {OPAQUE_COLOR_COUNT} opaque colours, "
            f"got {len(palette)}"
        )
    lookup = {}
    for index, rgb in enumerate(palette):
        lookup.setdefault(rgb, index)

    missing = set()
    for frame_idx, _cels in enumerate(parsed['frame_cels']):
        for layer in visible:
            cel = _resolve_cel(frame_idx, layer['index'], parsed['frame_cels'])
            if cel is None:
                continue
            clipped = _clip_cel_pixels(
                cel['pixels'], cel['x'], cel['y'], cel['width'], cel['height'],
                canvas_w, canvas_h, 4,
            )
            if clipped is None:
                continue
            pixels = clipped[2]
            for pos in range(0, len(pixels), 4):
                r, g, b, alpha = pixels[pos:pos + 4]
                if alpha == 0:
                    continue
                if alpha != 255:
                    raise ValueError(
                        f"Partially transparent pixel (alpha {alpha}) on layer "
                        f"{layer['name']!r} in frame {frame_idx}; only binary "
                        "alpha is supported"
                    )
                if (r, g, b) not in lookup:
                    missing.add((r, g, b))
    if missing:
        listing = ", ".join(
            "#%02X%02X%02X" % color for color in sorted(missing)
        )
        raise ValueError(
            f"RGBA source colours absent from target palette: {listing}; "
            "provide a palette containing every opaque source colour"
        )
    return lookup


def _cel_to_indices(pixels, color_depth, source_transparent_index, palette_lookup):
    """Map clipped cel pixels to canonical enjin indices (15 = transparent)."""
    if color_depth == COLOR_DEPTH_INDEXED:
        return bytes(
            _remap_indexed_pixel(pixel, source_transparent_index)
            for pixel in pixels
        )
    out = bytearray(len(pixels) // 4)
    for pos in range(0, len(pixels), 4):
        if pixels[pos + 3] == 0:
            out[pos // 4] = TRANSPARENT_INDEX
        else:
            out[pos // 4] = palette_lookup[
                (pixels[pos], pixels[pos + 1], pixels[pos + 2])
            ]
    return bytes(out)


def _crop_indices(indices, w, h):
    """Tight-crop an already-clipped index image to its non-transparent bounds.

    Returns ``(min_x, min_y, w, h, pixels)`` or ``None`` when fully transparent.
    """
    min_x = min_y = None
    max_x = max_y = -1
    for row in range(h):
        row_base = row * w
        for col in range(w):
            if indices[row_base + col] == TRANSPARENT_INDEX:
                continue
            if min_x is None:
                min_x, min_y = col, row
            else:
                min_x = min(min_x, col)
                min_y = min(min_y, row)
            max_x = max(max_x, col)
            max_y = max(max_y, row)
    if min_x is None:
        return None

    crop_w = max_x - min_x + 1
    crop_h = max_y - min_y + 1
    cropped = bytearray(crop_w * crop_h)
    for row in range(crop_h):
        src = (min_y + row) * w + min_x
        cropped[row * crop_w:(row + 1) * crop_w] = indices[src:src + crop_w]
    return min_x, min_y, crop_w, crop_h, bytes(cropped)


def _layered_clips(tags, durations, num_frames):
    """Authored tag clips, or a looping ``default`` clip over every frame.

    A tagged document whose tags all clamp to nothing is treated as untagged,
    so every layered asset remains immediately playable.
    """
    if tags:
        clips = build_clips_from_tags(tags, durations, num_frames)
        if clips:
            return clips
    return [_emit.Clip(
        name="default",
        loop_mode=_emit.LOOP_LOOP,
        frames=[
            (frame_idx, durations[frame_idx] if frame_idx < len(durations) else 100, 0)
            for frame_idx in range(num_frames)
        ],
    )]


def build_layered_asset(parsed, target_palette=None):
    """Assemble an ``emit.Layered`` asset and inspection summary from raw cels.

    ``parsed`` is the output of :func:`parse_aseprite_layered`.  ``target_palette``
    is the ``OPAQUE_COLOR_COUNT``-entry ``(r, g, b)`` list required for RGBA
    sources and ignored for indexed ones.  Raises ``ValueError`` for any source
    feature the layered authoring contract rejects.
    """
    color_depth = parsed['color_depth']
    layers = parsed['layers']
    frame_cels = parsed['frame_cels']
    num_frames = parsed['frame_count']
    canvas_w = parsed['width']
    canvas_h = parsed['height']

    visible = [layer for layer in layers if layer['visible']]
    ignored_layers = [
        layer['name'] or f"layer{layer['index']}"
        for layer in layers if not layer['visible']
    ]
    if not visible:
        raise ValueError("no visible layers to export as sprite parts")

    _validate_layers(visible)
    for layer in visible:
        if layer['opacity'] != 255:
            raise ValueError(
                f"Unsupported layer opacity {layer['opacity']} on layer "
                f"{layer['name']!r}; only 255 is supported"
            )

    palette_lookup = None
    if color_depth == COLOR_DEPTH_RGBA:
        if target_palette is None:
            raise ValueError(
                "RGBA sources require an explicit target palette (--palette)"
            )
        palette_lookup = _rgba_palette_lookup(
            parsed, visible, target_palette, canvas_w, canvas_h
        )

    resolved = [[None] * len(visible) for _ in range(num_frames)]
    linked_refs = 0
    for frame_idx in range(num_frames):
        raw_cels = frame_cels[frame_idx]
        for part_index, layer in enumerate(visible):
            layer_index = layer['index']
            raw_cel = raw_cels.get(layer_index)
            if raw_cel is not None and raw_cel['type'] == CEL_TYPE_LINKED:
                linked_refs += 1
            cel = _resolve_cel(frame_idx, layer_index, frame_cels)
            if cel is None:
                continue
            if cel['z_index'] != 0:
                raise ValueError(
                    f"Unsupported nonzero cel z-index {cel['z_index']} on layer "
                    f"{layer['name']!r} in frame {frame_idx}"
                )
            if cel['opacity'] != 255:
                raise ValueError(
                    f"Unsupported cel opacity {cel['opacity']} on layer "
                    f"{layer['name']!r} in frame {frame_idx}; only 255 is supported"
                )
            resolved[frame_idx][part_index] = cel

    images = []
    refs = []
    image_keys = {}
    reused_refs = 0
    for frame_idx in range(num_frames):
        for part_index, layer in enumerate(visible):
            cel = resolved[frame_idx][part_index]
            if cel is None:
                refs.append((_emit.LAYERED_INVISIBLE, 0, 0))
                continue
            bytes_per_pixel = 1 if color_depth == COLOR_DEPTH_INDEXED else 4
            clipped = _clip_cel_pixels(
                cel['pixels'], cel['x'], cel['y'], cel['width'], cel['height'],
                canvas_w, canvas_h, bytes_per_pixel,
            )
            if clipped is None:
                refs.append((_emit.LAYERED_INVISIBLE, 0, 0))
                continue
            clip_w, clip_h, clipped_pixels, origin_x, origin_y = clipped
            indices = _cel_to_indices(
                clipped_pixels, color_depth, parsed['transparent_index'],
                palette_lookup,
            )
            crop = _crop_indices(indices, clip_w, clip_h)
            if crop is None:
                refs.append((_emit.LAYERED_INVISIBLE, 0, 0))
                continue
            min_x, min_y, w, h, cropped = crop
            by_layer = image_keys.setdefault(part_index, {})
            image_index = by_layer.get((w, h, cropped))
            if image_index is None:
                image_index = len(images)
                images.append(_emit.PartImage(w, h, cropped))
                by_layer[(w, h, cropped)] = image_index
            else:
                reused_refs += 1
            refs.append((image_index, origin_x + min_x, origin_y + min_y))

    if not images:
        raise ValueError("no visible artwork to export: every cel is empty")

    parts = [layer['name'] or f"part{layer['index']}" for layer in visible]
    durations = list(parsed['durations'])
    clips = _layered_clips(parsed['tags'], durations, num_frames)

    asset = _emit.Layered(
        canvas_w=canvas_w,
        canvas_h=canvas_h,
        images=images,
        parts=parts,
        refs=refs,
        durations=durations,
        clips=clips,
    )
    summary = {
        'canvas_w': canvas_w,
        'canvas_h': canvas_h,
        'parts': parts,
        'num_frames': num_frames,
        'durations': durations,
        'clips': [(clip.name, len(clip.frames), clip.loop_mode) for clip in clips],
        'ignored_layers': ignored_layers,
        'num_images': len(images),
        'pool_pixels': sum(image.w * image.h for image in images),
        'linked_refs': linked_refs,
        'reused_refs': reused_refs,
    }
    return asset, summary


def format_layered_summary(summary):
    """Render the ``--layered`` inspection summary for the CLI."""
    clip_text = ', '.join(
        f"{name} ({count} frames)" for name, count, _loop in summary['clips']
    ) or 'none'
    ignored = summary['ignored_layers']
    lines = [
        f"Layered export: {summary['canvas_w']}x{summary['canvas_h']} canvas, "
        f"{len(summary['parts'])} parts, {summary['num_frames']} frames, "
        f"{summary['num_images']} images",
        f"  parts   : {', '.join(summary['parts']) or 'none'}",
        f"  clips   : {clip_text}",
        f"  ignored : {', '.join(ignored) if ignored else 'none'}",
        f"  reuse   : {summary['linked_refs']} linked refs, "
        f"{summary['reused_refs']} duplicate refs, {summary['num_images']} images "
        f"({summary['pool_pixels']} px)",
        f"  storage : {summary.get('storage_bytes', 0)} bytes (.njn)",
    ]
    return "\n".join(lines)


def _load_target_palette(spec):
    """Load a 15-colour target palette from a .gpl path or tools/palettes name."""
    from enjin_assets import palette as palette_mod
    if os.path.isfile(spec):
        path = spec
    else:
        path = os.path.join(
            palette_mod.PALETTES_DIR,
            spec if spec.endswith('.gpl') else spec + '.gpl',
        )
        if not os.path.isfile(path):
            raise ValueError(f"target palette not found: {spec!r}")
    loaded = palette_mod.load_gpl(path)
    return [tuple(int(channel) for channel in rgb) for rgb in loaded.rgb.tolist()]


def _run_layered(args, input_path):
    """Layered authoring path: emit a .njn v2 layered asset + inspection summary."""
    try:
        parsed = parse_aseprite_layered(input_path)
        target_palette = _load_target_palette(args.palette) if args.palette else None
        asset, summary = build_layered_asset(parsed, target_palette)
        data = _emit.build_njn_layered(asset)
    except ValueError as e:
        print(f"Error: {e}", file=sys.stderr)
        sys.exit(1)
    summary['storage_bytes'] = len(data)

    out_path = args.output or (os.path.splitext(input_path)[0] + ".njn")
    os.makedirs(os.path.dirname(os.path.abspath(out_path)), exist_ok=True)
    with open(out_path, 'wb') as f:
        f.write(data)
    print(f"Written: {out_path}  ({len(data)} bytes, .njn v2 layered)")
    print(format_layered_summary(summary))


# ---------------------------------------------------------------------------
# Grid / layout helpers
# ---------------------------------------------------------------------------

def _remap_indexed_pixel(index, source_transparent_index):
    if index == source_transparent_index:
        return TRANSPARENT_INDEX
    if index > 14:
        raise ValueError(
            f"Opaque source palette index {index} cannot be represented in enjin's 0..14 range"
        )
    return index


def build_pixel_array(frames, canvas_w, canvas_h, grid_spec,
                      source_transparent_index=TRANSPARENT_INDEX):
    """Return (pixel_bytes, cell_w, cell_h, cols, rows).

    grid_spec is None, or (gw, gh) from --grid WxH.
    """
    if grid_spec is not None:
        gw, gh = grid_spec
        # Treat the FIRST frame's canvas as a spritesheet grid.
        cols = canvas_w // gw
        rows = canvas_h // gh
        if cols == 0 or rows == 0:
            print(f"Warning: --grid {gw}x{gh} does not fit within canvas {canvas_w}x{canvas_h}; using 1x1")
            cols = max(1, cols)
            rows = max(1, rows)

        if canvas_w % gw != 0 or canvas_h % gh != 0:
            print(f"Warning: grid {gw}x{gh} does not evenly divide canvas {canvas_w}x{canvas_h}; cells will be truncated")

        first_frame = frames[0]
        out = bytearray()
        for row in range(rows):
            for col in range(cols):
                for py in range(gh):
                    for px in range(gw):
                        src_x = col * gw + px
                        src_y = row * gh + py
                        if src_x < canvas_w and src_y < canvas_h:
                            out.append(_remap_indexed_pixel(
                                first_frame[src_y * canvas_w + src_x],
                                source_transparent_index,
                            ))
                        else:
                            out.append(TRANSPARENT_INDEX)
        return bytes(out), gw, gh, cols, rows

    elif len(frames) == 1:
        pixels = bytes(_remap_indexed_pixel(b, source_transparent_index)
                       for b in frames[0])
        return pixels, canvas_w, canvas_h, 1, 1

    else:
        # Multiple Aseprite frames — each frame becomes a column
        out = bytearray()
        for frame in frames:
            out.extend(_remap_indexed_pixel(b, source_transparent_index)
                       for b in frame)
        return bytes(out), canvas_w, canvas_h, len(frames), 1


# ---------------------------------------------------------------------------
# C header emitter
# ---------------------------------------------------------------------------

def emit_header(pixel_data, name, cell_w, cell_h, cols, rows, source_filename):
    """Return the C header string."""
    total_frames = cols * rows
    lines = []
    lines.append(f"// Generated by aseprite2enjin.py from {source_filename}")
    lines.append(f"// Cell: {cell_w}x{cell_h}, Grid: {cols}x{rows}, Frames: {total_frames}")
    lines.append("#pragma once")
    lines.append("#include <cstdint>")
    lines.append("")
    lines.append(f"const uint8_t {name}_data[] = {{")

    frame_size = cell_w * cell_h
    for frame_idx in range(total_frames):
        lines.append(f"    // Frame {frame_idx}")
        start = frame_idx * frame_size
        end   = start + frame_size
        chunk = pixel_data[start:end]
        # emit 16 values per line
        for i in range(0, len(chunk), 16):
            segment = chunk[i:i + 16]
            hex_vals = ", ".join(f"0x{b:02X}" for b in segment)
            comma = "," if (i + 16 < len(chunk) or frame_idx + 1 < total_frames) else ""
            lines.append(f"    {hex_vals}{comma}")

    lines.append("};")
    lines.append("")
    lines.append("// Usage:")
    lines.append("// #include \"enjin2/graphics/sprite.hpp\"")
    lines.append(f"// enjin2::SpriteSheet {name}({name}_data, {cell_w}, {cell_h}, {cols}, {rows});")
    lines.append("")
    return "\n".join(lines)


# ---------------------------------------------------------------------------
# CLI
# ---------------------------------------------------------------------------

def derive_name(path: str) -> str:
    """Derive a C identifier from a file path."""
    base = os.path.splitext(os.path.basename(path))[0]
    # Replace non-identifier characters with underscores
    ident = "".join(c if c.isalnum() or c == '_' else '_' for c in base)
    if ident and ident[0].isdigit():
        ident = "_" + ident
    return ident or "sprite"


def parse_grid(value: str):
    """Parse a WxH grid string. Returns (w, h) or raises."""
    parts = value.lower().split('x')
    if len(parts) != 2:
        raise argparse.ArgumentTypeError(f"Grid must be WxH (e.g. 8x8), got: {value!r}")
    try:
        w, h = int(parts[0]), int(parts[1])
    except ValueError:
        raise argparse.ArgumentTypeError(f"Grid dimensions must be integers, got: {value!r}")
    if w <= 0 or h <= 0:
        raise argparse.ArgumentTypeError(f"Grid dimensions must be positive, got: {value!r}")
    return (w, h)


def _run_tilemap(args, input_path):
    """Tilemap authoring path: emit a .njn tileset + a .njm map (issue #41)."""
    tile_w, tile_h = args.grid if args.grid else (16, 16)

    try:
        parsed = parse_aseprite_layers(input_path)
    except ValueError as e:
        print(f"Error: {e}", file=sys.stderr)
        sys.exit(1)

    if not parsed['layers']:
        print("Error: no layers found in file", file=sys.stderr)
        sys.exit(1)

    tileset_pixels, frame_count, cols, rows, cells = build_tilemap(
        parsed['layers'], parsed['width'], parsed['height'], tile_w, tile_h
    )

    njn = emit_njn_bytes(tileset_pixels, tile_w, tile_h, frame_count, 1)
    njm = emit_njm_bytes(cells, cols, rows)

    base = os.path.splitext(args.output)[0] if args.output else os.path.splitext(input_path)[0]
    njn_path = base + ".njn"
    njm_path = base + ".njm"

    out_dir = os.path.dirname(os.path.abspath(njn_path))
    os.makedirs(out_dir, exist_ok=True)
    with open(njn_path, 'wb') as f:
        f.write(njn)
    with open(njm_path, 'wb') as f:
        f.write(njm)

    print(f"Written: {njn_path}  ({len(njn)} bytes, {frame_count} tiles, {tile_w}x{tile_h})")
    print(f"Written: {njm_path}  ({len(njm)} bytes, {cols}x{rows} map)")


def main():
    parser = argparse.ArgumentParser(
        description="Convert indexed-color .aseprite files to enjin C headers."
    )
    parser.add_argument("input", help="Input .aseprite file")
    parser.add_argument("--name",   default=None,
                        help="C identifier for the array (default: derived from filename)")
    parser.add_argument("--output", default=None,
                        help="Output .h path (default: same directory as input, .h extension)")
    parser.add_argument("--grid",   default=None, type=parse_grid, metavar="WxH",
                        help="Cell size for spritesheet-in-single-image mode (e.g. 8x8)")
    parser.add_argument("--tilemap", action="store_true",
                        help="Tilemap authoring mode: dice under/over layers into a "
                             ".njn tileset + .njm map (16x16 tiles unless --grid given)")
    parser.add_argument("--v2", action="store_true",
                        help="Emit a .njn v2 container sheet (META+PIXL, plus a CLIP "
                             "chunk built from Aseprite frame tags) instead of a C header")
    parser.add_argument("--layered", action="store_true",
                        help="Emit a .njn v2 layered sprite: cropped/deduplicated "
                             "source-layer parts, frame-part references, durations, "
                             "and clips, plus an inspection summary")
    parser.add_argument("--palette", default=None,
                        help="Target Enjin palette (.gpl path or tools/palettes name) "
                             "required for RGBA layered sources")

    args = parser.parse_args()

    input_path = args.input
    if not os.path.isfile(input_path):
        print(f"Error: file not found: {input_path}", file=sys.stderr)
        sys.exit(1)

    if args.layered:
        _run_layered(args, input_path)
        return

    if args.tilemap:
        _run_tilemap(args, input_path)
        return

    if args.v2:
        _run_sprite_v2(args, input_path)
        return

    # Derive defaults
    name = args.name or derive_name(input_path)
    if args.output:
        output_path = args.output
    else:
        base = os.path.splitext(input_path)[0]
        output_path = base + ".h"

    # Parse
    try:
        ase = parse_aseprite(input_path)
    except ValueError as e:
        print(f"Error: {e}", file=sys.stderr)
        sys.exit(1)
    if ase['color_depth'] != COLOR_DEPTH_INDEXED:
        print("Error: C header conversion requires indexed input; RGBA parsing does not quantize", file=sys.stderr)
        sys.exit(1)

    # Build pixel array
    try:
        pixel_data, cell_w, cell_h, cols, rows = build_pixel_array(
            ase['frames'], ase['width'], ase['height'], args.grid,
            ase['transparent_index'],
        )
    except ValueError as e:
        print(f"Error: {e}", file=sys.stderr)
        sys.exit(1)

    # Emit header
    header = emit_header(
        pixel_data, name, cell_w, cell_h, cols, rows,
        os.path.basename(input_path)
    )

    # Write output
    os.makedirs(os.path.dirname(os.path.abspath(output_path)), exist_ok=True)
    with open(output_path, 'w') as f:
        f.write(header)

    print(f"Written: {output_path}")
    print(f"  Array: {name}_data  ({len(pixel_data)} bytes)")
    print(f"  Cell:  {cell_w}x{cell_h}  Grid: {cols}x{rows}  Frames: {cols * rows}")


if __name__ == "__main__":
    main()
