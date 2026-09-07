#!/usr/bin/env python3
"""Fixture test for aseprite2enjin.py tilemap authoring (issue #41).

Synthesises a known 2-layer indexed .aseprite file (an under "floor" band and an
over "canopy" band), runs the extended converter, and asserts the exact .njn
tileset + .njm map bytes, including the uint16 cell packing.

Run directly (`python test_aseprite2enjin.py`) or under pytest.
"""

import io
import struct
import zlib
import os
import sys
import tempfile

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import aseprite2enjin as a2e  # noqa: E402


# ---------------------------------------------------------------------------
# Minimal .aseprite writer (indexed, single frame, N image layers)
# ---------------------------------------------------------------------------

def _cel_chunk(layer_index, x, y, cel_w, cel_h, pixels):
    """Build a compressed CEL chunk (real-file layout with 7 reserved bytes)."""
    body = struct.pack('<HhhBH', layer_index, x, y, 255, a2e.CEL_TYPE_COMPRESSED)
    body += b'\x00' * 7                          # 7 reserved bytes (real layout)
    body += struct.pack('<HH', cel_w, cel_h)
    body += zlib.compress(bytes(pixels))
    return struct.pack('<IH', len(body) + 6, a2e.CHUNK_CEL) + body


def _layer_chunk(name):
    """Build a LAYER chunk (0x2004) for a normal, visible image layer."""
    name_bytes = name.encode('utf-8')
    body = struct.pack('<HHHHHHB', 0x01, 0, 0, 0, 0, 0, 255)  # flags..opacity
    body += b'\x00' * 3                                       # reserved
    body += struct.pack('<H', len(name_bytes)) + name_bytes   # name STRING
    return struct.pack('<IH', len(body) + 6, a2e.CHUNK_LAYER) + body


def make_aseprite(width, height, layers):
    """Serialise a minimal indexed .aseprite. `layers` = [(name, pixels)] bottom→top."""
    chunks = b''
    for name, _px in layers:
        chunks += _layer_chunk(name)
    for idx, (_name, px) in enumerate(layers):
        chunks += _cel_chunk(idx, 0, 0, width, height, px)

    num_chunks = len(layers) * 2
    frame_body = chunks
    frame_size = 16 + len(frame_body)
    frame = struct.pack('<IHHH', frame_size, a2e.FRAME_MAGIC, 0xFFFF, 100)
    frame += struct.pack('<HI', 0, num_chunks)  # reserved(2) + new chunk count(4)
    frame += frame_body

    header = bytearray(128)
    struct.pack_into('<IH', header, 0, 128 + len(frame), a2e.ASE_MAGIC)
    struct.pack_into('<HHHH', header, 6, 1, width, height, a2e.COLOR_DEPTH_INDEXED)
    return bytes(header) + frame


# ---------------------------------------------------------------------------
# The fixture: 2 tiles wide, 1 tall. Under floor (index 1) spans both cells;
# over canopy (index 2) covers only the right cell.
# ---------------------------------------------------------------------------

def _build_fixture():
    W, H, T = 32, 16, 16
    TRANSP = a2e.TRANSPARENT_INDEX

    def cell_fill(idx, cols):
        # a full-canvas layer where each listed column is filled with `idx`
        px = bytearray([TRANSP] * (W * H))
        for ty in range(H):
            for cx in cols:
                for xx in range(T):
                    px[ty * W + cx * T + xx] = idx
        return bytes(px)

    under = cell_fill(1, cols=[0, 1])   # floor under both cells
    over = cell_fill(2, cols=[1])       # canopy over the right cell only
    return make_aseprite(W, H, [("under", under), ("over", over)])


def _run_tool(ase_bytes):
    with tempfile.TemporaryDirectory() as d:
        parsed = a2e.parse_aseprite_layers(io_write(d, "room.aseprite", ase_bytes))
    tileset_pixels, frame_count, cols, rows, cells = a2e.build_tilemap(
        parsed['layers'], parsed['width'], parsed['height'], 16, 16
    )
    njn = a2e.emit_njn_bytes(tileset_pixels, 16, 16, frame_count, 1)
    njm = a2e.emit_njm_bytes(cells, cols, rows)
    return njn, njm, frame_count, cols, rows, cells


def io_write(d, name, data):
    path = os.path.join(d, name)
    with open(path, 'wb') as f:
        f.write(data)
    return path


# ---------------------------------------------------------------------------
# Tests
# ---------------------------------------------------------------------------

def test_pack_cell_matches_bit_layout():
    # [band:1 | vflip:1 | hflip:1 | palbank:4 | tileid:9] → 0xF7A5
    assert a2e.pack_cell(0x1A5, band=1, palbank=0xB, hflip=True, vflip=True) == 0xF7A5
    assert a2e.pack_cell(511) == 0x01FF
    assert a2e.pack_cell(2, band=1) == 0x8002


def test_tilemap_njn_and_njm_bytes():
    njn, njm, frame_count, cols, rows, cells = _run_tool(_build_fixture())

    # --- map cells: under floor (id1, band0) then over canopy (id2, band1) ---
    assert (cols, rows) == (2, 1)
    assert cells == [0x0001, 0x8002]

    # --- .njm: 'NM' header + little-endian uint16 cells ---
    expected_njm = struct.pack('<2sBBBBBB', b'NM', 1, 0, 2, 1, 0, 0) \
        + struct.pack('<HH', 0x0001, 0x8002)
    assert njm == expected_njm

    # --- .njn: 'NJ' header (3 frames: transparent, floor, canopy) + pixels ---
    assert frame_count == 3
    expected_header = struct.pack('<2sBBBBBB', b'NJ', 1, 16, 16, 3, 1, 0)
    frame0 = bytes([0x0F]) * 256   # transparent (wasted) frame
    frame1 = bytes([0x01]) * 256   # floor
    frame2 = bytes([0x02]) * 256   # canopy
    assert njn == expected_header + frame0 + frame1 + frame2


def test_cli_writes_both_files(tmp_path=None):
    import subprocess
    d = tempfile.mkdtemp()
    ase_path = io_write(d, "room.aseprite", _build_fixture())
    out_base = os.path.join(d, "room")
    tool = os.path.join(os.path.dirname(os.path.abspath(__file__)), "aseprite2enjin.py")
    subprocess.check_call([sys.executable, tool, ase_path, "--tilemap",
                           "--output", out_base + ".njn"])
    assert os.path.isfile(out_base + ".njn")
    assert os.path.isfile(out_base + ".njm")
    with open(out_base + ".njm", 'rb') as f:
        assert f.read(2) == b'NM'
    with open(out_base + ".njn", 'rb') as f:
        assert f.read(2) == b'NJ'


def _main():
    tests = [test_pack_cell_matches_bit_layout,
             test_tilemap_njn_and_njm_bytes,
             test_cli_writes_both_files]
    failures = 0
    for t in tests:
        try:
            t()
            print(f"PASS: {t.__name__}")
        except AssertionError as e:
            failures += 1
            print(f"FAIL: {t.__name__}: {e}")
        except Exception as e:  # noqa: BLE001
            failures += 1
            print(f"ERROR: {t.__name__}: {e!r}")
    print(f"\n{len(tests) - failures} passed, {failures} failed")
    return 1 if failures else 0


if __name__ == "__main__":
    sys.exit(_main())
