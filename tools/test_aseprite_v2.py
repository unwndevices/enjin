#!/usr/bin/env python3
"""Test the aseprite2enjin.py .njn v2 sheet + clip path (issue #87).

Synthesises a 3-frame indexed .aseprite with a FRAME_TAGS chunk, runs the v2
emitter, and asserts the .njn v2 container decodes with a CLIP chunk carrying the
tag's per-frame durations. Runnable under pytest or directly.
"""

import os
import struct
import sys
import tempfile
import zlib

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import aseprite2enjin as a2e  # noqa: E402
from enjin_assets import emit  # noqa: E402


def _cel_chunk(layer_index, cel_w, cel_h, pixels):
    body = struct.pack('<HhhBH', layer_index, 0, 0, 255, a2e.CEL_TYPE_COMPRESSED)
    body += b'\x00' * 7
    body += struct.pack('<HH', cel_w, cel_h)
    body += zlib.compress(bytes(pixels))
    return struct.pack('<IH', len(body) + 6, a2e.CHUNK_CEL) + body


def _frame_tags_chunk(tags):
    body = struct.pack('<H', len(tags)) + b'\x00' * 8
    for from_frame, to_frame, loop_dir, name in tags:
        nb = name.encode('utf-8')
        body += struct.pack('<HHB', from_frame, to_frame, loop_dir)
        body += struct.pack('<H', 0)      # repeat
        body += b'\x00' * 6               # reserved
        body += b'\x00' * 3               # colour (deprecated)
        body += b'\x00'                   # extra
        body += struct.pack('<H', len(nb)) + nb
    return struct.pack('<IH', len(body) + 6, a2e.CHUNK_FRAME_TAGS) + body


def _frame(chunks, duration):
    body = b"".join(chunks)
    frame_size = 16 + len(body)
    hdr = struct.pack('<IHHH', frame_size, a2e.FRAME_MAGIC, 0xFFFF, duration)
    hdr += struct.pack('<HI', 0, len(chunks))
    return hdr + body


def _make_multiframe_aseprite(w, h, frame_pixels, durations, tags):
    frames = []
    # Frame 0 carries the FRAME_TAGS chunk plus its cel.
    frames.append(_frame([_frame_tags_chunk(tags), _cel_chunk(0, w, h, frame_pixels[0])], durations[0]))
    for i in range(1, len(frame_pixels)):
        frames.append(_frame([_cel_chunk(0, w, h, frame_pixels[i])], durations[i]))
    body = b"".join(frames)
    header = bytearray(128)
    struct.pack_into('<IH', header, 0, 128 + len(body), a2e.ASE_MAGIC)
    struct.pack_into('<HHHH', header, 6, len(frame_pixels), w, h, a2e.COLOR_DEPTH_INDEXED)
    return bytes(header) + body


def _build():
    w, h = 2, 2
    pix = [bytes([0, 1, 2, 3]), bytes([4, 5, 6, 7]), bytes([8, 9, 10, 11])]
    durations = [100, 150, 200]
    tags = [(0, 2, 2, "spin")]  # pingpong over all 3 frames
    return _make_multiframe_aseprite(w, h, pix, durations, tags)


def test_v2_sheet_has_clip_with_durations():
    ase_bytes = _build()
    with tempfile.TemporaryDirectory() as d:
        path = os.path.join(d, "anim.aseprite")
        with open(path, "wb") as f:
            f.write(ase_bytes)
        parsed_ase = a2e.parse_aseprite(path)

    assert parsed_ase['frame_count'] == 3
    assert parsed_ase['durations'] == [100, 150, 200]
    assert parsed_ase['tags'] == [(0, 2, 2, "spin")]

    data, frame_count, clips = a2e.emit_njn_v2_sheet(parsed_ase, grid_spec=None)
    assert frame_count == 3
    parsed = emit.parse_njn(data)
    assert parsed.version == 2
    assert emit.CHUNK_META in parsed.chunks
    assert emit.CHUNK_PIXL in parsed.chunks
    assert emit.CHUNK_CLIP in parsed.chunks

    body = parsed.chunks[emit.CHUNK_CLIP]
    (num_clips,) = struct.unpack_from("<B", body, 0)
    assert num_clips == 1
    assert body[1:17].rstrip(b"\x00") == b"spin"
    loop_mode, num_frames = struct.unpack_from("<BB", body, 17)
    assert (loop_mode, num_frames) == (emit.LOOP_PINGPONG, 3)
    # Per-frame durations preserved from the ASE frame headers.
    d0 = struct.unpack_from("<HHB", body, 19)
    d1 = struct.unpack_from("<HHB", body, 24)
    d2 = struct.unpack_from("<HHB", body, 29)
    assert (d0[0], d0[1]) == (0, 100)
    assert (d1[0], d1[1]) == (1, 150)
    assert (d2[0], d2[1]) == (2, 200)


def test_v2_sheet_without_tags_omits_clip():
    w, h = 2, 2
    ase = _make_multiframe_aseprite(w, h, [bytes([0, 1, 2, 3])], [120], tags=[])
    with tempfile.TemporaryDirectory() as d:
        path = os.path.join(d, "still.aseprite")
        with open(path, "wb") as f:
            f.write(ase)
        parsed_ase = a2e.parse_aseprite(path)
    data, frame_count, clips = a2e.emit_njn_v2_sheet(parsed_ase, grid_spec=None)
    assert clips is None
    parsed = emit.parse_njn(data)
    assert emit.CHUNK_CLIP not in parsed.chunks


def test_v2_grid_mode_drops_clips():
    # --grid slices one frame into cells; frame-tag clips don't apply → omitted.
    ase_bytes = _build()  # 3 frames, tag "spin"
    with tempfile.TemporaryDirectory() as d:
        path = os.path.join(d, "anim.aseprite")
        with open(path, "wb") as f:
            f.write(ase_bytes)
        parsed_ase = a2e.parse_aseprite(path)
    data, frame_count, clips = a2e.emit_njn_v2_sheet(parsed_ase, grid_spec=(1, 1))
    assert clips is None
    parsed = emit.parse_njn(data)
    assert emit.CHUNK_CLIP not in parsed.chunks


if __name__ == "__main__":
    test_v2_sheet_has_clip_with_durations()
    test_v2_sheet_without_tags_omits_clip()
    test_v2_grid_mode_drops_clips()
    print("PASS")
