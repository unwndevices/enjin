#!/usr/bin/env python3
"""Host-side RGBA Aseprite/Pixquare parsing and flattening tests."""

import hashlib
import os
import struct
import sys
import zlib

import pytest

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import aseprite2enjin as a2e  # noqa: E402


def _chunk(chunk_type, body):
    return struct.pack('<IH', len(body) + 6, chunk_type) + body


def _layer_chunk(name, *, flags=1, layer_type=0, child_level=0,
                 blend_mode=0, opacity=255):
    name_bytes = name.encode('utf-8')
    body = struct.pack(
        '<HHHHHHB', flags, layer_type, child_level, 0, 0, blend_mode, opacity
    )
    body += b'\x00' * 3 + struct.pack('<H', len(name_bytes)) + name_bytes
    return _chunk(a2e.CHUNK_LAYER, body)


def _palette_chunk(first, colors):
    body = struct.pack('<III', 256, first, first + len(colors) - 1) + b'\x00' * 8
    for color in colors:
        body += struct.pack('<HBBBB', 0, *color)
    return _chunk(a2e.CHUNK_PALETTE, body)


def _cel_chunk(layer_index, pixels=None, *, cel_type=a2e.CEL_TYPE_COMPRESSED,
               width=1, height=1, x=0, y=0, opacity=255, z_index=0,
               linked_frame=None, compressed_payload=None):
    body = struct.pack(
        '<HhhBHh', layer_index, x, y, opacity, cel_type, z_index
    ) + b'\x00' * 5
    if cel_type == a2e.CEL_TYPE_LINKED:
        body += struct.pack('<H', linked_frame)
    else:
        payload = bytes(pixels)
        if cel_type == a2e.CEL_TYPE_COMPRESSED:
            payload = zlib.compress(payload) if compressed_payload is None else compressed_payload
        body += struct.pack('<HH', width, height) + payload
    return _chunk(a2e.CHUNK_CEL, body)


def _frame(chunks, duration=100):
    body = b''.join(chunks)
    header = struct.pack('<IHHH', 16 + len(body), a2e.FRAME_MAGIC, 0xFFFF, duration)
    return header + struct.pack('<HI', 0, len(chunks)) + body


def _aseprite(frames, *, width=1, height=1, depth=a2e.COLOR_DEPTH_RGBA,
              transparent_index=0):
    body = b''.join(frames)
    header = bytearray(128)
    struct.pack_into('<IH', header, 0, 128 + len(body), a2e.ASE_MAGIC)
    struct.pack_into('<HHHH', header, 6, len(frames), width, height, depth)
    struct.pack_into('<I', header, 14, 1)  # Layer opacity fields are valid.
    header[28] = transparent_index
    return bytes(header) + body


def _write(tmp_path, data, name='synthetic.aseprite'):
    path = tmp_path / name
    path.write_bytes(data)
    return path


def test_rgba_raw_compressed_visibility_and_opacity(tmp_path):
    frame = _frame([
        _layer_chunk('bottom'),
        _layer_chunk('middle', opacity=128),
        _layer_chunk('hidden', flags=0),
        _cel_chunk(0, [100, 0, 0, 255], cel_type=a2e.CEL_TYPE_RAW),
        _cel_chunk(1, [0, 100, 0, 128], opacity=128),
        _cel_chunk(2, [0, 0, 255, 255], cel_type=a2e.CEL_TYPE_RAW),
    ])

    parsed = a2e.parse_aseprite(_write(tmp_path, _aseprite([frame])))

    assert parsed['frames'] == [bytes([87, 13, 0, 255])]
    assert parsed['layers'][1] == {
        'name': 'middle', 'visible': True, 'flags': 1, 'type': 0,
        'child_level': 0, 'blend_mode': 0, 'opacity': 128, 'index': 1,
    }
    assert parsed['warnings'] == []


def test_linked_cel_uses_source_pixels_and_link_header_opacity(tmp_path):
    frame0 = _frame([
        _layer_chunk('art'),
        _cel_chunk(0, [20, 40, 60, 255], cel_type=a2e.CEL_TYPE_RAW),
    ], duration=75)
    frame1 = _frame([
        _cel_chunk(0, cel_type=a2e.CEL_TYPE_LINKED, linked_frame=0, opacity=128),
    ], duration=225)

    parsed = a2e.parse_aseprite(_write(tmp_path, _aseprite([frame0, frame1])))

    assert parsed['durations'] == [75, 225]
    assert parsed['frames'][0] == bytes([20, 40, 60, 255])
    assert parsed['frames'][1] == bytes([20, 40, 60, 128])


def test_indexed_file_uses_declared_transparent_index(tmp_path):
    frame = _frame([
        _layer_chunk('art'),
        _cel_chunk(0, [0, 7], cel_type=a2e.CEL_TYPE_RAW, width=2),
    ])
    data = bytearray(_aseprite([frame], width=2, depth=a2e.COLOR_DEPTH_INDEXED))
    data[28] = 0

    parsed = a2e.parse_aseprite(_write(tmp_path, bytes(data)))

    assert parsed['transparent_index'] == 0
    assert parsed['frames'] == [bytes([0, 7])]


def test_indexed_rgba_output_uses_palette_and_declared_transparency(tmp_path):
    frame = _frame([
        _layer_chunk('art'),
        _palette_chunk(7, [(10, 20, 30, 255)]),
        _cel_chunk(0, [3, 7], cel_type=a2e.CEL_TYPE_RAW, width=2),
    ])
    data = _aseprite(
        [frame], width=2, depth=a2e.COLOR_DEPTH_INDEXED, transparent_index=3
    )

    parsed = a2e.parse_aseprite(_write(tmp_path, data), rgba_output=True)

    assert parsed['color_depth'] == a2e.COLOR_DEPTH_INDEXED
    assert parsed['frame_format'] == 'rgba'
    assert parsed['frames'] == [bytes([0, 0, 0, 0, 10, 20, 30, 255])]


def test_indexed_rgba_output_applies_layer_and_cel_opacity(tmp_path):
    frame = _frame([
        _layer_chunk('bottom'),
        _layer_chunk('middle', opacity=128),
        _palette_chunk(1, [(100, 0, 0, 255), (0, 100, 0, 128)]),
        _cel_chunk(0, [1], cel_type=a2e.CEL_TYPE_RAW),
        _cel_chunk(1, [2], cel_type=a2e.CEL_TYPE_RAW, opacity=128),
    ])
    data = _aseprite(
        [frame], depth=a2e.COLOR_DEPTH_INDEXED, transparent_index=0
    )

    parsed = a2e.parse_aseprite(_write(tmp_path, data), rgba_output=True)

    assert parsed['frames'] == [bytes([87, 13, 0, 255])]


def test_default_indexed_output_rejects_opacity_it_cannot_preserve(tmp_path):
    frame = _frame([
        _layer_chunk('art', opacity=128),
        _cel_chunk(0, [1], cel_type=a2e.CEL_TYPE_RAW),
    ])
    data = _aseprite(
        [frame], depth=a2e.COLOR_DEPTH_INDEXED, transparent_index=0
    )

    with pytest.raises(ValueError, match='cannot preserve layer or cel opacity'):
        a2e.parse_aseprite(_write(tmp_path, data))


def test_indexed_rgba_output_respects_cel_z_order(tmp_path):
    frame = _frame([
        _layer_chunk('bottom'),
        _layer_chunk('top'),
        _palette_chunk(1, [(255, 0, 0, 255), (0, 255, 0, 255)]),
        _cel_chunk(0, [1], cel_type=a2e.CEL_TYPE_RAW),
        _cel_chunk(1, [2], cel_type=a2e.CEL_TYPE_RAW, z_index=-2),
    ])
    data = _aseprite(
        [frame], depth=a2e.COLOR_DEPTH_INDEXED, transparent_index=0
    )

    parsed = a2e.parse_aseprite(_write(tmp_path, data), rgba_output=True)

    assert parsed['frames'] == [bytes([255, 0, 0, 255])]


def test_indexed_rgba_output_uses_cumulative_palette_updates_per_frame(tmp_path):
    frame0 = _frame([
        _layer_chunk('art'),
        _palette_chunk(1, [(200, 10, 20, 255)]),
        _cel_chunk(0, [1], cel_type=a2e.CEL_TYPE_RAW),
    ])
    frame1 = _frame([
        _palette_chunk(1, [(5, 15, 225, 255)]),
        _cel_chunk(0, cel_type=a2e.CEL_TYPE_LINKED, linked_frame=0),
    ])
    data = _aseprite(
        [frame0, frame1], depth=a2e.COLOR_DEPTH_INDEXED, transparent_index=0
    )

    parsed = a2e.parse_aseprite(_write(tmp_path, data), rgba_output=True)

    assert parsed['frames'] == [
        bytes([200, 10, 20, 255]),
        bytes([5, 15, 225, 255]),
    ]


def test_indexed_rgba_output_rejects_missing_used_palette_color(tmp_path):
    frame = _frame([
        _layer_chunk('art'),
        _cel_chunk(0, [4], cel_type=a2e.CEL_TYPE_RAW),
    ])
    data = _aseprite(
        [frame], depth=a2e.COLOR_DEPTH_INDEXED, transparent_index=0
    )
    with pytest.raises(ValueError, match='Missing palette color for index 4'):
        a2e.parse_aseprite(_write(tmp_path, data), rgba_output=True)


def test_converter_remaps_declared_transparency_to_15(tmp_path):
    frame = _frame([
        _layer_chunk('art'),
        _cel_chunk(0, [0, 14], cel_type=a2e.CEL_TYPE_RAW, width=2),
    ])
    parsed = a2e.parse_aseprite(_write(
        tmp_path,
        _aseprite([frame], width=2, depth=a2e.COLOR_DEPTH_INDEXED,
                  transparent_index=0),
    ))

    pixels, *_layout = a2e.build_pixel_array(
        parsed['frames'], parsed['width'], parsed['height'], None,
        parsed['transparent_index'],
    )
    assert pixels == bytes([15, 14])


def test_converter_rejects_opaque_index_that_would_alias(tmp_path):
    frame = _frame([
        _layer_chunk('art'),
        _cel_chunk(0, [15], cel_type=a2e.CEL_TYPE_RAW),
    ])
    parsed = a2e.parse_aseprite(_write(
        tmp_path,
        _aseprite([frame], depth=a2e.COLOR_DEPTH_INDEXED,
                  transparent_index=0),
    ))

    with pytest.raises(ValueError, match='Opaque source palette index 15'):
        a2e.build_pixel_array(
            parsed['frames'], parsed['width'], parsed['height'], None,
            parsed['transparent_index'],
        )


@pytest.mark.parametrize('layer_options, message', [
    ({'blend_mode': 1}, 'Unsupported blend mode 1'),
    ({'layer_type': a2e.LAYER_TYPE_GROUP}, 'Group layer semantics are unsupported'),
    ({'layer_type': a2e.LAYER_TYPE_TILEMAP}, 'Tilemap layers are unsupported'),
])
def test_unsupported_layer_semantics_are_rejected(tmp_path, layer_options, message):
    frame = _frame([_layer_chunk('unsupported', **layer_options)])
    with pytest.raises(ValueError, match=message):
        a2e.parse_aseprite(_write(tmp_path, _aseprite([frame])))


def test_corrupt_compressed_cel_is_not_broadly_repaired(tmp_path):
    frame = _frame([
        _layer_chunk('art'),
        _cel_chunk(0, [1, 2, 3, 4], compressed_payload=b'\x78\x9cgarbage'),
    ])
    with pytest.raises(ValueError, match='Corrupt compressed cel'):
        a2e.parse_aseprite(_write(tmp_path, _aseprite([frame])))


def test_pixquare_fixture_properties_and_exact_flattened_frames():
    fixture = os.path.join(os.path.dirname(__file__), 'testdata', 'tomo_tune.aseprite')
    parsed = a2e.parse_aseprite(fixture)

    assert (parsed['width'], parsed['height'], parsed['color_depth']) == (160, 160, 32)
    assert parsed['frame_count'] == 8
    assert parsed['durations'] == [100] * 8
    assert parsed['tags'] == []
    assert [layer['name'] for layer in parsed['layers']] == [
        'Detail', 'Vaso', 'Bunny', 'Arrow', 'Note letter', 'Gambo', 'Fiore', 'Sparkles'
    ]
    assert all(layer['visible'] and layer['type'] == 0 and layer['blend_mode'] == 0
               for layer in parsed['layers'])
    assert len(parsed['warnings']) == 57
    assert all('missing Adler-32' in warning for warning in parsed['warnings'])
    assert [len(frame) for frame in parsed['frames']] == [160 * 160 * 4] * 8
    assert [hashlib.sha256(frame).hexdigest() for frame in parsed['frames']] == [
        '504e05d435ea12404eeec6fa4f3ffe6267e0b9a1baf64cddee3b1bd3b0097cb0',
        '54989a3b32fe3576a8216834afd3c3aee53c0d8e846109ca6c45d64d4a99f464',
        '9dc1683338892fee0c7bf64a73eed26951c6978c3298ac153212f54148da569b',
        '32b978acac051e9ffe5844b0c90c69bdb6e4e63686d40c21d6a3967b0665df12',
        '66e69cb156986c8adddd26092fefef78bba3921a6218bf1cfda8798c6c1a1c1f',
        '06f0380e0fb778600c61a48fd84adf48859109c6e73d5ca00ffc2a31216f2dea',
        'e5b4e9a55cbd236949b39d4dd5a2480b8dbbaf84b120e4286dc5065f902d33d3',
        'f7af1c8e4742e4a8e957369210c424fbf5447c4bc030333cf0d5239e33d3353a',
    ]
