#!/usr/bin/env python3
"""Spec fixes in ``aseprite2enjin.py``'s parser (Tomodachi #295).

One test per spec bug found by Tomodachi #284 (old palette chunk ``0x0004``,
the z-index tie-break of spec NOTE.5, a 0 ms frame falling back to the header
speed), plus the parsed tag tuple. The tag → clip mapping lives in the C++
importer now (``enjin_sprite_import``, Tomodachi #299).
"""

import os
import struct
import sys
import zlib

import pytest

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import aseprite2enjin as a2e  # noqa: E402

T = 0  # Transparent source index used by the fixtures.

PINGPONG = a2e.TAG_PINGPONG


# ---------------------------------------------------------------------------
# Minimal .aseprite writer
# ---------------------------------------------------------------------------

def _chunk(chunk_type, body):
    return struct.pack('<IH', len(body) + 6, chunk_type) + body


def _layer_chunk(name):
    name_bytes = name.encode('utf-8')
    body = struct.pack('<HHHHHHB', 0x01, 0, 0, 0, 0, 0, 255)
    body += b'\x00' * 3 + struct.pack('<H', len(name_bytes)) + name_bytes
    return _chunk(a2e.CHUNK_LAYER, body)


def _cel_chunk(layer_index, pixels, *, width=1, height=1, x=0, y=0, z_index=0):
    body = struct.pack(
        '<HhhBHh', layer_index, x, y, 255, a2e.CEL_TYPE_COMPRESSED, z_index
    ) + b'\x00' * 5
    body += struct.pack('<HH', width, height) + zlib.compress(bytes(pixels))
    return _chunk(a2e.CHUNK_CEL, body)


def _old_palette_chunk(packets):
    """0x0004: WORD packets, then per packet BYTE skip, BYTE count, RGB×count."""
    body = struct.pack('<H', len(packets))
    for skip, colors in packets:
        body += struct.pack('<BB', skip, len(colors) & 0xFF)
        for color in colors:
            body += bytes(color)
    return _chunk(a2e.CHUNK_OLD_PALETTE, body)


def _new_palette_chunk(first, colors):
    body = struct.pack('<III', 256, first, first + len(colors) - 1) + b'\x00' * 8
    for color in colors:
        body += struct.pack('<HBBBB', 0, *color)
    return _chunk(a2e.CHUNK_PALETTE, body)


def _frame_tags_chunk(tags):
    """``tags``: (from, to, direction, repeat, name)."""
    body = struct.pack('<H', len(tags)) + b'\x00' * 8
    for from_frame, to_frame, direction, repeat, name in tags:
        nb = name.encode('utf-8')
        body += struct.pack('<HHBH', from_frame, to_frame, direction, repeat)
        body += b'\x00' * 6 + b'\x00' * 3 + b'\x00'
        body += struct.pack('<H', len(nb)) + nb
    return _chunk(a2e.CHUNK_FRAME_TAGS, body)


def _frame(chunks, duration=100):
    body = b''.join(chunks)
    header = struct.pack('<IHHH', 16 + len(body), a2e.FRAME_MAGIC, 0xFFFF, duration)
    return header + struct.pack('<HI', 0, len(chunks)) + body


def _aseprite(frames, *, width=1, height=1, speed=100):
    body = b''.join(frames)
    header = bytearray(128)
    struct.pack_into('<IH', header, 0, 128 + len(body), a2e.ASE_MAGIC)
    struct.pack_into('<HHHH', header, 6, len(frames), width, height,
                     a2e.COLOR_DEPTH_INDEXED)
    struct.pack_into('<H', header, 18, speed)
    header[28] = T
    return bytes(header) + body


def _write(tmp_path, data, name='fixture.aseprite'):
    path = tmp_path / name
    path.write_bytes(data)
    return str(path)


def _animation(tmp_path, durations, tags=(), *, layers=1, speed=100):
    """One 1×1 frame per duration; frame i paints index i + 1 on every layer."""
    frames = []
    for i, duration in enumerate(durations):
        chunks = []
        if i == 0:
            chunks += [_layer_chunk(f'layer{n}') for n in range(layers)]
            if tags:
                chunks.append(_frame_tags_chunk(list(tags)))
        chunks += [_cel_chunk(n, [i + 1]) for n in range(layers)]
        frames.append(_frame(chunks, duration))
    return _write(tmp_path, _aseprite(frames, speed=speed))


# ---------------------------------------------------------------------------
# Spec bug 1: the old palette chunk (0x0004) is read
# ---------------------------------------------------------------------------

def test_old_palette_chunk_colours_indexed_rgba_output(tmp_path):
    # Aseprite 1.3 writes only 0x0004 for an opaque palette of <= 256 colours.
    frame = _frame([
        _layer_chunk('art'),
        _old_palette_chunk([(1, [(200, 10, 20), (5, 15, 225)])]),
        _cel_chunk(0, [1, 2], width=2, height=1),
    ])
    path = _write(tmp_path, _aseprite([frame], width=2, height=1))

    parsed = a2e.parse_aseprite(path, rgba_output=True)

    assert parsed['frames'] == [bytes([200, 10, 20, 255, 5, 15, 225, 255])]


def test_old_palette_chunk_count_zero_means_256_colours(tmp_path):
    colours = [(i, 255 - i, i // 2) for i in range(256)]
    frame = _frame([
        _layer_chunk('art'),
        _old_palette_chunk([(0, colours)]),
        _cel_chunk(0, [255], width=1, height=1),
    ])
    path = _write(tmp_path, _aseprite([frame]))

    parsed = a2e.parse_aseprite(path, rgba_output=True)

    assert parsed['frames'] == [bytes([255, 0, 127, 255])]


def test_old_palette_chunk_is_ignored_when_the_frame_has_a_new_one(tmp_path):
    frame = _frame([
        _layer_chunk('art'),
        _new_palette_chunk(1, [(1, 2, 3, 128)]),
        _old_palette_chunk([(1, [(9, 9, 9)])]),
        _cel_chunk(0, [1]),
    ])
    path = _write(tmp_path, _aseprite([frame]))

    parsed = a2e.parse_aseprite(path, rgba_output=True)

    assert parsed['frames'] == [bytes([1, 2, 3, 128])]


def test_truncated_old_palette_chunk_is_rejected(tmp_path):
    body = struct.pack('<H', 1) + struct.pack('<BB', 0, 2) + bytes([1, 2, 3])
    frame = _frame([_layer_chunk('art'), _chunk(a2e.CHUNK_OLD_PALETTE, body),
                    _cel_chunk(0, [1])])
    path = _write(tmp_path, _aseprite([frame]))

    with pytest.raises(ValueError, match='old palette'):
        a2e.parse_aseprite(path, rgba_output=True)


# ---------------------------------------------------------------------------
# Spec bug 2: z-index ties break by z-index (spec NOTE.5)
# ---------------------------------------------------------------------------

def test_z_index_tie_paints_the_lower_z_index_first(tmp_path):
    # Layer 0 with z +1 and layer 1 with z 0 both sit at order 1. NOTE.5: on a
    # tie the lower z-index paints first, so layer 0 (z +1) ends on top.
    frame = _frame([
        _layer_chunk('bottom'), _layer_chunk('top'),
        _cel_chunk(0, [1], z_index=1),
        _cel_chunk(1, [2], z_index=0),
    ])
    path = _write(tmp_path, _aseprite([frame]))

    assert a2e.parse_aseprite(path)['frames'] == [bytes([1])]


def test_z_index_order_still_sorts_before_the_tie_break(tmp_path):
    # Layer 0 z +2 → order 2 beats layer 1 z 0 → order 1, whatever the z values.
    frame = _frame([
        _layer_chunk('bottom'), _layer_chunk('top'),
        _cel_chunk(0, [1], z_index=2),
        _cel_chunk(1, [2], z_index=0),
    ])
    path = _write(tmp_path, _aseprite([frame]))

    assert a2e.parse_aseprite(path)['frames'] == [bytes([1])]


# ---------------------------------------------------------------------------
# Spec bug 3: a 0 ms frame falls back to the header speed
# ---------------------------------------------------------------------------

def test_zero_duration_falls_back_to_header_speed(tmp_path):
    path = _animation(tmp_path, [80, 0, 40], speed=120)

    assert a2e.parse_aseprite(path)['durations'] == [80, 120, 40]


def test_zero_duration_and_zero_speed_fall_back_to_100ms(tmp_path):
    path = _animation(tmp_path, [0, 50], speed=0)

    assert a2e.parse_aseprite(path)['durations'] == [100, 50]


# ---------------------------------------------------------------------------
# Tags
# ---------------------------------------------------------------------------

def test_tags_carry_their_repeat_count(tmp_path):
    path = _animation(tmp_path, [100] * 3, [(0, 2, PINGPONG, 4, 'spin')])

    assert a2e.parse_aseprite(path)['tags'] == [(0, 2, PINGPONG, 4, 'spin')]


if __name__ == '__main__':
    sys.exit(pytest.main([__file__, '-v']))
