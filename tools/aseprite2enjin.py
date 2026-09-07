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

# ---------------------------------------------------------------------------
# ASE format constants
# ---------------------------------------------------------------------------
ASE_MAGIC        = 0xA5E0
FRAME_MAGIC      = 0xF1FA
CHUNK_LAYER      = 0x2004
CHUNK_CEL        = 0x2005
CHUNK_CEL_EXTRA  = 0x2006
COLOR_DEPTH_INDEXED = 8

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

def parse_aseprite(path: str):
    """Parse an .aseprite file.

    Returns a dict with:
        width, height         -- canvas size in pixels
        color_depth           -- must be 8 (indexed)
        frame_count           -- number of animation frames
        frames                -- list of (canvas_width * canvas_height) bytes arrays
    """
    with open(path, 'rb') as f:
        data = f.read()

    offset = 0

    # --- File header (128 bytes) ---
    if len(data) < 128:
        raise ValueError("File too small to be a valid .aseprite file")

    file_size, magic = struct.unpack_from('<IH', data, offset)
    if magic != ASE_MAGIC:
        raise ValueError(f"Not a valid .aseprite file (bad magic: 0x{magic:04X}, expected 0x{ASE_MAGIC:04X})")

    frame_count, width, height, color_depth = struct.unpack_from('<HHHH', data, offset + 6)
    # flags at offset+14, speed at offset+16 — skip
    # transparent color index for indexed mode is at offset+28 (1 byte)
    # number of colors at offset+32 (2 bytes)

    if color_depth != COLOR_DEPTH_INDEXED:
        raise ValueError(
            f"Only indexed-color sprites are supported (found {color_depth}-bit). "
            f"In Aseprite: Sprite > Color Mode > Indexed."
        )

    offset = 128  # skip to first frame

    frames = []

    for frame_idx in range(frame_count):
        if offset + 16 > len(data):
            break

        # --- Frame header (16 bytes) ---
        frame_size, frame_magic, num_chunks_old, frame_duration = struct.unpack_from('<IHHH', data, offset)
        # new chunk count at offset+12 (4 bytes), replaces num_chunks_old when != 0xFFFF
        num_chunks_new = struct.unpack_from('<I', data, offset + 12)[0]

        if frame_magic != FRAME_MAGIC:
            raise ValueError(f"Bad frame magic at frame {frame_idx}: 0x{frame_magic:04X}")

        num_chunks = num_chunks_new if num_chunks_old == 0xFFFF else num_chunks_old

        frame_end = offset + frame_size
        chunk_offset = offset + 16  # first chunk starts after 16-byte frame header

        # canvas buffer: fill with transparent index
        canvas = bytearray([TRANSPARENT_INDEX] * (width * height))

        for _ in range(num_chunks):
            if chunk_offset + 6 > frame_end:
                break

            chunk_size, chunk_type = struct.unpack_from('<IH', data, chunk_offset)
            if chunk_size < 6:
                break  # malformed

            chunk_data_offset = chunk_offset + 6
            chunk_body_size   = chunk_size - 6

            if chunk_type == CHUNK_CEL:
                _process_cel_chunk(
                    data, chunk_data_offset, chunk_body_size,
                    canvas, width, height
                )

            chunk_offset += chunk_size

        frames.append(bytes(canvas))
        offset = frame_end

    return {
        'width':       width,
        'height':      height,
        'color_depth': color_depth,
        'frame_count': len(frames),
        'frames':      frames,
    }


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
# Grid / layout helpers
# ---------------------------------------------------------------------------

def build_pixel_array(frames, canvas_w, canvas_h, grid_spec):
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
                            out.append(first_frame[src_y * canvas_w + src_x] & 0x0F)
                        else:
                            out.append(TRANSPARENT_INDEX)
        return bytes(out), gw, gh, cols, rows

    elif len(frames) == 1:
        # Single frame — emit as-is, mask to lower nibble
        pixels = bytes(b & 0x0F for b in frames[0])
        return pixels, canvas_w, canvas_h, 1, 1

    else:
        # Multiple Aseprite frames — each frame becomes a column
        out = bytearray()
        for frame in frames:
            out.extend(b & 0x0F for b in frame)
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

    args = parser.parse_args()

    input_path = args.input
    if not os.path.isfile(input_path):
        print(f"Error: file not found: {input_path}", file=sys.stderr)
        sys.exit(1)

    if args.tilemap:
        _run_tilemap(args, input_path)
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

    # Build pixel array
    pixel_data, cell_w, cell_h, cols, rows = build_pixel_array(
        ase['frames'], ase['width'], ase['height'], args.grid
    )

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
