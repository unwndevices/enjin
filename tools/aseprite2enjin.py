#!/usr/bin/env python3
"""aseprite2enjin.py — Aseprite tilemap authoring and the .aseprite parser.

Parses the Aseprite binary format (ASE file spec) using Python stdlib only.
``--tilemap`` dices index-mode under/over layers into a .njn tileset + .njm map;
``parse_aseprite`` flattens frames for host tools (scripts/live_preview.py).

Sprites (sheet and layered .njn v2) are imported by the C++ CLI
``enjin_sprite_import`` (tools/sprite_import, ADR-0015).
"""

import struct
import zlib
import os
import sys
import argparse


# ---------------------------------------------------------------------------
# ASE format constants
# ---------------------------------------------------------------------------
ASE_MAGIC        = 0xA5E0
FRAME_MAGIC      = 0xF1FA
CHUNK_OLD_PALETTE = 0x0004
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

# Aseprite tag loop directions: the third field of each parse_aseprite() tag.
TAG_FORWARD          = 0
TAG_REVERSE          = 1
TAG_PINGPONG         = 2
TAG_PINGPONG_REVERSE = 3

# A frame whose duration and the header speed are both 0 holds this long.
FALLBACK_DURATION_MS = 100

CEL_TYPE_RAW        = 0
CEL_TYPE_LINKED     = 1
CEL_TYPE_COMPRESSED = 2

TRANSPARENT_INDEX = 15

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
    # Deprecated header speed: what a 0 ms frame holds for.
    speed = struct.unpack_from('<H', data, 18)[0]
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
    tags = []        # list of (from_frame, to_frame, loop_dir, repeat, name)
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
        old_palettes = []   # 0x0004 bodies, applied only if the frame has no 0x2019
        has_new_palette = False

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
                has_new_palette = True
            elif (chunk_type == CHUNK_OLD_PALETTE
                  and color_depth == COLOR_DEPTH_INDEXED and want_palette):
                old_palettes.append((chunk_data_offset, chunk_body_size))

            chunk_offset += chunk_size

        if chunk_offset != frame_end:
            raise ValueError(f"Frame {frame_idx} chunk data does not match its declared size")
        # The spec says to ignore 0x0004 when a 0x2019 is present; Aseprite
        # writes only 0x0004 for an opaque palette of <= 256 colours.
        if not has_new_palette:
            for old_offset, old_size in old_palettes:
                _parse_old_palette_update(data, old_offset, old_size, palette)
        frame_cels.append(cels)
        frame_palettes.append(dict(palette))
        if frame_duration == 0:
            frame_duration = speed or FALLBACK_DURATION_MS
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


def _parse_old_palette_update(data, offset, body_size, palette):
    """Apply one old PALETTE chunk (0x0004) to the current palette.

    Layout: WORD packets, then per packet BYTE skip, BYTE count (0 = 256),
    and count RGB triples. Entries are opaque.
    """
    end = offset + body_size
    if body_size < 2:
        raise ValueError("Truncated old palette chunk")
    packets = struct.unpack_from('<H', data, offset)[0]
    pos = offset + 2
    index = 0
    for _ in range(packets):
        if pos + 2 > end:
            raise ValueError("Truncated old palette packet")
        skip, count = data[pos], data[pos + 1]
        pos += 2
        index += skip
        count = count or 256
        if index + count > 256:
            raise ValueError("Old palette runs past 256 colours")
        if pos + 3 * count > end:
            raise ValueError("Truncated old palette colour")
        for _ in range(count):
            palette[index] = (data[pos], data[pos + 1], data[pos + 2], 255)
            pos += 3
            index += 1


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
            render_cels.append((layer['index'] + cel['z_index'], cel['z_index'], layer, cel))
    # Spec NOTE.5: order = layer index + z-index; ties paint the lower z-index
    # first. The sort is stable, so equal (order, z) keep layer order.
    render_cels.sort(key=lambda item: (item[0], item[1]))

    for _order, _z_index, layer, cel in render_cels:
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
    """Parse a FRAME_TAGS chunk (0x2018) → list of (from, to, loop_dir, repeat, name).

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
        repeat = struct.unpack_from('<H', data, pos + 5)[0]
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
        out.append((from_frame, to_frame, loop_dir, repeat, name))
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
# CLI
# ---------------------------------------------------------------------------

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
        description="Dice an indexed .aseprite into a .njn tileset + .njm map. "
                    "Sprites: use enjin_sprite_import (tools/sprite_import)."
    )
    parser.add_argument("input", help="Input .aseprite file")
    parser.add_argument("--output", default=None,
                        help="Output path; its extension is replaced by .njn and .njm "
                             "(default: next to the input)")
    parser.add_argument("--grid",   default=None, type=parse_grid, metavar="WxH",
                        help="Tile size (default: 16x16)")
    parser.add_argument("--tilemap", action="store_true",
                        help="Tilemap authoring mode: dice under/over layers into a "
                             ".njn tileset + .njm map (the only mode)")

    args = parser.parse_args()

    if not args.tilemap:
        parser.error("only --tilemap is supported; sprites are imported by "
                     "enjin_sprite_import (tools/sprite_import, ADR-0015)")

    input_path = args.input
    if not os.path.isfile(input_path):
        print(f"Error: file not found: {input_path}", file=sys.stderr)
        sys.exit(1)

    _run_tilemap(args, input_path)



if __name__ == "__main__":
    main()
