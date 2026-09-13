"""Tests for the .njn v2 / .njm emitters (issue #87).

Byte layouts are asserted against the C++ single source of truth in
``njn2.hpp`` / ``tilemap_asset.hpp``; :func:`emit.parse_njn` reproduces the
C++ reader's validation so a clean parse proves device/web loadability.
"""

import os
import struct
import sys

sys.path.insert(0, os.path.dirname(os.path.dirname(os.path.dirname(os.path.abspath(__file__)))))

from enjin_assets import emit  # noqa: E402


def _frames(n, cell=4):
    """n frames of a cell*cell tile, each filled with its index (low nibble)."""
    out = bytearray()
    for i in range(n):
        out += bytes([i & 0x0F]) * (cell * cell)
    return bytes(out)


# --- header + directory ---------------------------------------------------

def test_njn_header_and_roundtrip():
    data = emit.build_njn(4, 4, _frames(3), tile_count=3)
    parsed = emit.parse_njn(data)
    assert parsed.version == 2
    assert parsed.file_size == len(data)
    assert emit.CHUNK_META in parsed.chunks
    assert emit.CHUNK_PIXL in parsed.chunks
    cw, ch, cols, rows = struct.unpack("<BBBB", parsed.chunks[emit.CHUNK_META])
    assert (cw, ch, cols, rows) == (4, 4, 3, 1)
    assert len(parsed.chunks[emit.CHUNK_PIXL]) == 3 * 16


def test_meta_geometry_matches_grid_and_pixel_padding():
    # 300 tiles → grid 255x2 = 510 frames; pixels padded to 510 frames.
    data = emit.build_njn(4, 4, _frames(300), tile_count=300)
    parsed = emit.parse_njn(data)
    cw, ch, cols, rows = struct.unpack("<BBBB", parsed.chunks[emit.CHUNK_META])
    assert (cols, rows) == (255, 2)
    assert len(parsed.chunks[emit.CHUNK_PIXL]) == 255 * 2 * 16


def test_choose_grid():
    assert emit.choose_grid(1) == (1, 1)
    assert emit.choose_grid(132) == (132, 1)
    assert emit.choose_grid(255) == (255, 1)
    assert emit.choose_grid(256) == (255, 2)
    assert emit.choose_grid(511) == (255, 3)


# --- ATTR chunk -----------------------------------------------------------

def test_attr_chunk_layout():
    attrs = [(0, 0), (0x01, 7), (0x02, 0)]  # id0 empty, id1 SOLID kind7, id2 ONEWAY
    data = emit.build_njn(4, 4, _frames(3), tile_count=3, attrs=attrs)
    parsed = emit.parse_njn(data)
    body = parsed.chunks[emit.CHUNK_ATTR]
    (num,) = struct.unpack_from("<H", body, 0)
    assert num == 3
    decoded = [struct.unpack_from("<BB", body, 2 + i * 2) for i in range(num)]
    assert decoded == [(0, 0), (0x01, 7), (0x02, 0)]


def test_attr_chunk_omitted_when_all_zero():
    data = emit.build_njn(4, 4, _frames(2), tile_count=2, attrs=[(0, 0), (0, 0)])
    parsed = emit.parse_njn(data)
    assert emit.CHUNK_ATTR not in parsed.chunks


# --- PALB chunk -----------------------------------------------------------

def test_palb_chunk_layout():
    banks = list(range(16)) + list(range(16, 32))  # 2 banks of 16 RGB565
    data = emit.build_njn(4, 4, _frames(1), tile_count=1, palb=banks)
    parsed = emit.parse_njn(data)
    body = parsed.chunks[emit.CHUNK_PALB]
    (num_banks,) = struct.unpack_from("<B", body, 0)
    assert num_banks == 2
    vals = [struct.unpack_from("<H", body, 1 + i * 2)[0] for i in range(32)]
    assert vals == banks


# --- CLIP chunk -----------------------------------------------------------

def test_clip_chunk_layout():
    clips = [emit.Clip("walk", emit.LOOP_LOOP, [(0, 100, 0), (1, 100, 5)])]
    data = emit.build_njn(4, 4, _frames(2), tile_count=2, clips=clips)
    parsed = emit.parse_njn(data)
    body = parsed.chunks[emit.CHUNK_CLIP]
    (num_clips,) = struct.unpack_from("<B", body, 0)
    assert num_clips == 1
    name = body[1:17].rstrip(b"\x00")
    assert name == b"walk"
    loop_mode, num_frames = struct.unpack_from("<BB", body, 17)
    assert (loop_mode, num_frames) == (emit.LOOP_LOOP, 2)
    f0 = struct.unpack_from("<HHB", body, 19)
    assert f0 == (0, 100, 0)
    f1 = struct.unpack_from("<HHB", body, 19 + 5)
    assert f1 == (1, 100, 5)


# --- .njm -----------------------------------------------------------------

def test_njm_layout_and_roundtrip():
    cells = [0x0001, 0x8002, 0x0003, 0x2004]  # 2x2
    data = emit.emit_njm(cells, 2, 2)
    expected = struct.pack("<2sBBBBBB", b"NM", 1, 0, 2, 2, 0, 0) + struct.pack("<4H", *cells)
    assert data == expected


def test_njm_rejects_bad_dims_and_size():
    for bad in (lambda: emit.emit_njm([0] * 3, 2, 2), lambda: emit.emit_njm([0] * 256 * 256, 256, 256)):
        try:
            bad()
            assert False, "expected ValueError"
        except ValueError:
            pass


# --- reader rejects corruption --------------------------------------------

def test_parse_njn_rejects_corruption():
    good = emit.build_njn(4, 4, _frames(1), tile_count=1)
    for mutate in (
        lambda b: b"XX" + b[2:],          # bad magic
        lambda b: b[:2] + bytes([9]) + b[3:],  # bad version
        lambda b: b[:-1],                  # truncated → file_size mismatch
    ):
        try:
            emit.parse_njn(mutate(good))
            assert False, "expected ValueError"
        except ValueError:
            pass
