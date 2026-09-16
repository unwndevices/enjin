"""Tests for the .njn v2 layered sprite codec (issue #93).

Mirrors ``njn2_layered_test.cpp``: the golden byte stream is asserted here and
in C++, so the two writers are proven byte-identical, and :func:`emit.parse_njn_layered`
re-implements ``njn2DecodeLayered``'s validation so a clean parse proves device
loadability.
"""

import os
import struct
import sys
import warnings

sys.path.insert(0, os.path.dirname(os.path.dirname(os.path.dirname(os.path.abspath(__file__)))))

from enjin_assets import emit  # noqa: E402

GOLDEN_HEX = (
    "4e4a020006000000c50000004c484452540000000b0000004c494d475f000000"
    "0d0000004c5052546c000000200000004c5245468c000000180000004c445552"
    "a400000004000000434c4950a80000001d000000010400040002000200020002"
    "000200000102030100010005626f647900000000000000000000000065796500"
    "00000000000000000000000000000000000001000100feff0000ffff0000ffff"
    "00000000640096000164656661756c7400000000000000000001020000640000"
    "0100960000"
)


def _fixture() -> emit.Layered:
    return emit.Layered(
        canvas_w=4,
        canvas_h=4,
        images=[
            emit.PartImage(2, 2, bytes([0, 1, 2, 3])),
            emit.PartImage(1, 1, bytes([5])),
        ],
        parts=["body", "eye"],
        refs=[(0, 0, 0), (1, 1, -2), (0, -1, 0), (emit.LAYERED_INVISIBLE, 0, 0)],
        durations=[100, 150],
        clips=[emit.Clip("default", emit.LOOP_LOOP, [(0, 100, 0), (1, 150, 0)])],
    )


# The base fixture with a single in-canvas static pivot (2, 1); its golden bytes
# are asserted byte-identical to the C++ mirror (LAY-22).
PIVOT_GOLDEN_HEX = (
    "4e4a020007000000d50000004c484452600000000b0000004c494d476b000000"
    "0d0000004c50525478000000200000004c52454698000000180000004c445552"
    "b0000000040000004c504956b400000004000000434c4950b80000001d000000"
    "010400040002000200020002000200000102030100010005626f647900000000"
    "0000000000000000657965000000000000000000000000000000000000000100"
    "0100feff0000ffff0000ffff0000000064009600020001000164656661756c74"
    "000000000000000000010200006400000100960000"
)


def _pivot_fixture() -> emit.Layered:
    asset = _fixture()
    asset.pivot_x = 2
    asset.pivot_y = 1
    return asset


def _dir_entries(buf: bytes):
    (num,) = struct.unpack_from("<I", buf, 4)
    out = []
    for i in range(num):
        base = 12 + i * 12
        tag, off, size = struct.unpack_from("<4sII", buf, base)
        out.append((tag, off, size, base))
    return out


def _set_size(buf: bytes, tag: bytes, new_size: int) -> bytes:
    b = bytearray(buf)
    for t, _off, _size, base in _dir_entries(buf):
        if t == tag:
            struct.pack_into("<I", b, base + 8, new_size)
            return bytes(b)
    raise KeyError(tag)


def _data_offset(buf: bytes, tag: bytes) -> int:
    for t, off, _size, _base in _dir_entries(buf):
        if t == tag:
            return off
    raise KeyError(tag)


def _variant(*, dup_lhdr=False, omit_ldur=False, dup_clip=False) -> bytes:
    asset = _fixture()
    num_frames, num_parts, num_images = 2, 2, 2
    w = emit.NjnV2Writer()

    def lhdr():
        w.add_chunk(
            emit.CHUNK_LHDR,
            struct.pack("<BHHHHH", asset.schema_version, asset.canvas_w, asset.canvas_h,
                        num_frames, num_parts, num_images),
        )

    lhdr()
    if dup_lhdr:
        lhdr()

    limg = bytearray()
    for img in asset.images:
        limg += struct.pack("<HH", img.w, img.h) + img.pixels
    w.add_chunk(emit.CHUNK_LIMG, bytes(limg))

    lprt = bytearray()
    for name in asset.parts:
        lprt += name.encode("utf-8") + b"\x00" * (16 - len(name.encode("utf-8")))
    w.add_chunk(emit.CHUNK_LPRT, bytes(lprt))

    lref = bytearray()
    for idx, ox, oy in asset.refs:
        lref += struct.pack("<Hhh", idx, ox, oy)
    w.add_chunk(emit.CHUNK_LREF, bytes(lref))

    if not omit_ldur:
        w.add_chunk(emit.CHUNK_LDUR, b"".join(struct.pack("<H", d) for d in asset.durations))

    if asset.clips:
        w.add_chunk(emit.CHUNK_CLIP, emit._build_clip_chunk(asset.clips))
        if dup_clip:
            w.add_chunk(emit.CHUNK_CLIP, emit._build_clip_chunk(asset.clips))

    return w.finalise()


# --- golden bytes ----------------------------------------------------------

def test_layered_golden_bytes():
    data = emit.build_njn_layered(_fixture())
    assert data == bytes.fromhex(GOLDEN_HEX)


# --- round-trip ------------------------------------------------------------

def test_layered_roundtrip():
    data = emit.build_njn_layered(_fixture())
    out = emit.parse_njn_layered(data)
    assert out.canvas_w == 4 and out.canvas_h == 4
    assert [img.pixels for img in out.images] == [b"\x00\x01\x02\x03", b"\x05"]
    assert [(img.w, img.h) for img in out.images] == [(2, 2), (1, 1)]
    assert out.parts == ["body", "eye"]
    assert out.refs == [(0, 0, 0), (1, 1, -2), (0, -1, 0), (emit.LAYERED_INVISIBLE, 0, 0)]
    assert out.durations == [100, 150]
    assert [(c.name, c.loop_mode, c.frames) for c in out.clips] == [
        ("default", emit.LOOP_LOOP, [(0, 100, 0), (1, 150, 0)])
    ]


def test_layered_reserialise():
    data = emit.build_njn_layered(_fixture())
    out = emit.parse_njn_layered(data)
    assert emit.build_njn_layered(out) == data


# --- unknown chunk ---------------------------------------------------------

def test_layered_unknown_chunk_skipped():
    w = emit.NjnV2Writer()
    for tag, data in emit.parse_njn(emit.build_njn_layered(_fixture())).chunks.items():
        w.add_chunk(tag, data)
    w.add_chunk(b"ZZZZ", b"\xde\xad")
    buf = w.finalise()
    out = emit.parse_njn_layered(buf)
    assert out.canvas_w == 4


# --- required chunks -------------------------------------------------------

def test_layered_missing_required():
    buf = _variant(omit_ldur=True)
    try:
        emit.parse_njn_layered(buf)
        assert False, "expected ValueError"
    except ValueError as e:
        assert "missing" in str(e)


def test_layered_duplicate_required():
    buf = _variant(dup_lhdr=True)
    try:
        emit.parse_njn_layered(buf)
        assert False, "expected ValueError"
    except ValueError as e:
        assert "duplicate" in str(e)


# --- schema / counts -------------------------------------------------------

def test_layered_unsupported_schema():
    data = bytearray(emit.build_njn_layered(_fixture()))
    data[_data_offset(data, emit.CHUNK_LHDR)] = 2
    try:
        emit.parse_njn_layered(bytes(data))
        assert False, "expected ValueError"
    except ValueError as e:
        assert "schema" in str(e)


def test_layered_zero_canvas():
    data = bytearray(emit.build_njn_layered(_fixture()))
    data[_data_offset(data, emit.CHUNK_LHDR) + 1] = 0
    try:
        emit.parse_njn_layered(bytes(data))
        assert False, "expected ValueError"
    except ValueError as e:
        assert "canvas" in str(e)


def test_layered_zero_counts():
    data = bytearray(emit.build_njn_layered(_fixture()))
    data[_data_offset(data, emit.CHUNK_LHDR) + 5] = 0
    try:
        emit.parse_njn_layered(bytes(data))
        assert False, "expected ValueError"
    except ValueError as e:
        assert "frame" in str(e) or "count" in str(e)


def test_layered_zero_images():
    data = bytearray(emit.build_njn_layered(_fixture()))
    data[_data_offset(data, emit.CHUNK_LHDR) + 9] = 0
    try:
        emit.parse_njn_layered(bytes(data))
        assert False, "expected ValueError"
    except ValueError as e:
        assert "image pool" in str(e)


# --- record truncation / sizes --------------------------------------------

def test_layered_truncated_lhdr():
    buf = _set_size(emit.build_njn_layered(_fixture()), emit.CHUNK_LHDR, 10)
    try:
        emit.parse_njn_layered(buf)
        assert False, "expected ValueError"
    except ValueError as e:
        assert "LHDR" in str(e)


def test_layered_truncated_limg():
    buf = _set_size(emit.build_njn_layered(_fixture()), emit.CHUNK_LIMG, 6)
    try:
        emit.parse_njn_layered(buf)
        assert False, "expected ValueError"
    except ValueError as e:
        assert "LIMG" in str(e)


def test_layered_zero_image():
    data = bytearray(emit.build_njn_layered(_fixture()))
    data[_data_offset(data, emit.CHUNK_LIMG)] = 0
    try:
        emit.parse_njn_layered(bytes(data))
        assert False, "expected ValueError"
    except ValueError as e:
        assert "zero-size" in str(e)


def test_layered_limg_trailing():
    buf = _set_size(emit.build_njn_layered(_fixture()), emit.CHUNK_LIMG, 14)
    try:
        emit.parse_njn_layered(buf)
        assert False, "expected ValueError"
    except ValueError as e:
        assert "trailing" in str(e)


def test_layered_lprt_size():
    buf = _set_size(emit.build_njn_layered(_fixture()), emit.CHUNK_LPRT, 16)
    try:
        emit.parse_njn_layered(buf)
        assert False, "expected ValueError"
    except ValueError as e:
        assert "LPRT" in str(e)


def test_layered_lref_overflow():
    data = bytearray(emit.build_njn_layered(_fixture()))
    off = _data_offset(data, emit.CHUNK_LHDR)
    data[off + 5:off + 7] = b"\xff\xff"  # numFrames = 0xFFFF, numParts stays 2
    try:
        emit.parse_njn_layered(bytes(data))
        assert False, "expected ValueError"
    except ValueError as e:
        assert "LREF" in str(e)


def test_layered_invalid_ref():
    data = bytearray(emit.build_njn_layered(_fixture()))
    off = _data_offset(data, emit.CHUNK_LREF)
    data[off:off + 2] = struct.pack("<H", 5)  # image index 5 >= numImages 2
    try:
        emit.parse_njn_layered(bytes(data))
        assert False, "expected ValueError"
    except ValueError as e:
        assert "reference" in str(e)


def test_layered_ldur_size():
    buf = _set_size(emit.build_njn_layered(_fixture()), emit.CHUNK_LDUR, 2)
    try:
        emit.parse_njn_layered(buf)
        assert False, "expected ValueError"
    except ValueError as e:
        assert "LDUR" in str(e)


# --- clips ---------------------------------------------------------------

def test_layered_clip_frame_oob():
    data = bytearray(emit.build_njn_layered(_fixture()))
    off = _data_offset(data, emit.CHUNK_CLIP)
    struct.pack_into("<H", data, off + 19, 10)  # frameIndex 10 >= numFrames 2
    try:
        emit.parse_njn_layered(bytes(data))
        assert False, "expected ValueError"
    except ValueError as e:
        assert "frame index" in str(e)


def test_layered_duplicate_clip():
    buf = _variant(dup_clip=True)
    try:
        emit.parse_njn_layered(buf)
        assert False, "expected ValueError"
    except ValueError as e:
        assert "duplicate" in str(e)


# --- directory ------------------------------------------------------------

def test_layered_bad_directory():
    data = bytearray(emit.build_njn_layered(_fixture()))
    struct.pack_into("<I", data, 12 + 4, 0x0FFFFFFF)  # first entry offset OOB
    try:
        emit.parse_njn_layered(bytes(data))
        assert False, "expected ValueError"
    except ValueError:
        pass


# --- writer consistency ----------------------------------------------------

def test_layered_writer_rejects_inconsistent_refs():
    asset = _fixture()
    asset.refs = [(0, 0, 0)]
    try:
        emit.build_njn_layered(asset)
        assert False, "expected ValueError"
    except ValueError:
        pass


def test_layered_writer_rejects_bad_image_size():
    asset = _fixture()
    asset.images[0].pixels = b"\x00\x01"  # 2 pixels for a 2x2 image
    try:
        emit.build_njn_layered(asset)
        assert False, "expected ValueError"
    except ValueError:
        pass


def test_layered_writer_rejects_empty_images():
    asset = _fixture()
    asset.images = []
    try:
        emit.build_njn_layered(asset)
        assert False, "expected ValueError"
    except ValueError:
        pass


# --- static pivot (LPIV, issue #133) ---------------------------------------

def test_layered_pivot_golden_bytes():
    data = emit.build_njn_layered(_pivot_fixture())
    assert data == bytes.fromhex(PIVOT_GOLDEN_HEX)


def test_layered_pivot_roundtrip():
    data = emit.build_njn_layered(_pivot_fixture())
    out = emit.parse_njn_layered(data)
    assert (out.pivot_x, out.pivot_y) == (2, 1)


def test_layered_pivot_reserialise():
    data = emit.build_njn_layered(_pivot_fixture())
    out = emit.parse_njn_layered(data)
    assert emit.build_njn_layered(out) == data


def test_layered_pivot_absent_defaults_zero():
    # The base fixture leaves pivot at (0, 0), so no LPIV chunk is written and
    # the pivot round-trips back to (0, 0).
    data = emit.build_njn_layered(_fixture())
    assert emit.CHUNK_LPIV not in emit.parse_njn(data).chunks
    out = emit.parse_njn_layered(data)
    assert (out.pivot_x, out.pivot_y) == (0, 0)


def test_layered_pivot_negative_roundtrip_and_warns():
    asset = _fixture()
    asset.pivot_x, asset.pivot_y = -3, 1  # negative x is outside the canvas
    data = emit.build_njn_layered(asset)
    with warnings.catch_warnings(record=True) as caught:
        warnings.simplefilter("always")
        out = emit.parse_njn_layered(data)
    assert (out.pivot_x, out.pivot_y) == (-3, 1)
    assert any("outside canvas" in str(w.message) for w in caught)


def test_layered_pivot_outside_canvas_warns():
    asset = _fixture()
    asset.pivot_x, asset.pivot_y = 10, 10  # both beyond the 4x4 canvas
    data = emit.build_njn_layered(asset)
    with warnings.catch_warnings(record=True) as caught:
        warnings.simplefilter("always")
        out = emit.parse_njn_layered(data)
    assert (out.pivot_x, out.pivot_y) == (10, 10)
    assert any("outside canvas" in str(w.message) for w in caught)


def test_layered_pivot_inside_canvas_no_warning():
    data = emit.build_njn_layered(_pivot_fixture())
    with warnings.catch_warnings(record=True) as caught:
        warnings.simplefilter("always")
        emit.parse_njn_layered(data)
    assert not caught


def test_layered_duplicate_pivot():
    w = emit.NjnV2Writer()
    for tag, data in emit.parse_njn(emit.build_njn_layered(_pivot_fixture())).chunks.items():
        w.add_chunk(tag, data)
    w.add_chunk(emit.CHUNK_LPIV, struct.pack("<hh", 2, 1))
    try:
        emit.parse_njn_layered(w.finalise())
        assert False, "expected ValueError"
    except ValueError as e:
        assert "duplicate" in str(e)


def test_layered_truncated_pivot():
    buf = _set_size(emit.build_njn_layered(_pivot_fixture()), emit.CHUNK_LPIV, 3)
    try:
        emit.parse_njn_layered(buf)
        assert False, "expected ValueError"
    except ValueError as e:
        assert "LPIV" in str(e)
