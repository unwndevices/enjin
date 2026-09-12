#!/usr/bin/env python3
"""Tests for the ``aseprite2enjin.py --layered`` exporter (issue #94).

Synthesises indexed and RGBA .aseprite fixtures to exercise the layered authoring
contract: clipping before bounds, tight cropping, linked/identical reuse, moving
linked cels, painter order, hidden-layer reporting, part-name preservation, the
indexed→enjin index mapping, RGBA exact-color mapping, and every explicitly
rejected source feature. Emitted assets are decoded with the shared codec so a
clean parse proves on-device loadability.
"""

import os
import struct
import sys
import zlib

import pytest

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import aseprite2enjin as a2e  # noqa: E402
from enjin_assets import emit  # noqa: E402

T = 0  # Transparent source index used by the indexed fixtures.


# ---------------------------------------------------------------------------
# Minimal .aseprite writer
# ---------------------------------------------------------------------------

def _chunk(chunk_type, body):
    return struct.pack('<IH', len(body) + 6, chunk_type) + body


def _layer_chunk(name, *, flags=0x01, layer_type=0, child_level=0,
                 blend_mode=0, opacity=255):
    name_bytes = name.encode('utf-8')
    body = struct.pack(
        '<HHHHHHB', flags, layer_type, child_level, 0, 0, blend_mode, opacity
    )
    body += b'\x00' * 3 + struct.pack('<H', len(name_bytes)) + name_bytes
    return _chunk(a2e.CHUNK_LAYER, body)


def _cel_chunk(layer_index, pixels=None, *, cel_type=a2e.CEL_TYPE_COMPRESSED,
               width=1, height=1, x=0, y=0, opacity=255, z_index=0,
               linked_frame=None):
    body = struct.pack(
        '<HhhBHh', layer_index, x, y, opacity, cel_type, z_index
    ) + b'\x00' * 5
    if cel_type == a2e.CEL_TYPE_LINKED:
        body += struct.pack('<H', linked_frame)
    else:
        payload = zlib.compress(bytes(pixels)) if cel_type == a2e.CEL_TYPE_COMPRESSED else bytes(pixels)
        body += struct.pack('<HH', width, height) + payload
    return _chunk(a2e.CHUNK_CEL, body)


def _frame_tags_chunk(tags):
    body = struct.pack('<H', len(tags)) + b'\x00' * 8
    for from_frame, to_frame, loop_dir, name in tags:
        nb = name.encode('utf-8')
        body += struct.pack('<HHB', from_frame, to_frame, loop_dir)
        body += struct.pack('<H', 0) + b'\x00' * 6 + b'\x00' * 3 + b'\x00'
        body += struct.pack('<H', len(nb)) + nb
    return _chunk(a2e.CHUNK_FRAME_TAGS, body)


def _frame(chunks, duration=100):
    body = b''.join(chunks)
    header = struct.pack('<IHHH', 16 + len(body), a2e.FRAME_MAGIC, 0xFFFF, duration)
    return header + struct.pack('<HI', 0, len(chunks)) + body


def _aseprite(frames, *, width, height, depth=a2e.COLOR_DEPTH_INDEXED,
              transparent_index=T, layer_opacity_valid=False):
    body = b''.join(frames)
    header = bytearray(128)
    struct.pack_into('<IH', header, 0, 128 + len(body), a2e.ASE_MAGIC)
    struct.pack_into('<HHHH', header, 6, len(frames), width, height, depth)
    struct.pack_into('<I', header, 14, 1 if layer_opacity_valid else 0)
    header[28] = transparent_index
    return bytes(header) + body


def _write(tmp_path, data, name='fixture.aseprite'):
    path = tmp_path / name
    path.write_bytes(data)
    return str(path)


def _rgba(*pixels):
    flat = []
    for pixel in pixels:
        flat.extend(pixel)
    return flat


def _convert(path, palette=None):
    parsed = a2e.parse_aseprite_layered(path)
    asset, summary = a2e.build_layered_asset(parsed, palette)
    data = emit.build_njn_layered(asset)
    summary['storage_bytes'] = len(data)
    return asset, summary, emit.parse_njn_layered(data)


# ---------------------------------------------------------------------------
# Core shape: parts, refs, offsets, default clip
# ---------------------------------------------------------------------------

def test_layered_shape_refs_and_default_clip(tmp_path):
    frame0 = _frame([
        _layer_chunk('body'), _layer_chunk('eye'),
        _cel_chunk(0, [1] * 16, width=4, height=4),
        _cel_chunk(1, [2], width=1, height=1, x=2, y=1),
    ], duration=100)
    frame1 = _frame([_cel_chunk(0, cel_type=a2e.CEL_TYPE_LINKED, linked_frame=0)], duration=200)
    path = _write(tmp_path, _aseprite([frame0, frame1], width=4, height=4))

    _asset, summary, out = _convert(path)

    assert (out.canvas_w, out.canvas_h) == (4, 4)
    assert out.parts == ['body', 'eye']
    assert [(img.w, img.h, img.pixels) for img in out.images] == [
        (4, 4, bytes([1] * 16)), (1, 1, bytes([2])),
    ]
    assert out.refs == [
        (0, 0, 0), (1, 2, 1),
        (0, 0, 0), (emit.LAYERED_INVISIBLE, 0, 0),
    ]
    assert out.durations == [100, 200]
    assert [(c.name, c.loop_mode, [f[0] for f in c.frames]) for c in out.clips] == [
        ('default', emit.LOOP_LOOP, [0, 1]),
    ]
    assert summary['parts'] == ['body', 'eye']
    assert summary['num_frames'] == 2
    assert summary['ignored_layers'] == []


def test_layered_tags_become_named_clips(tmp_path):
    frames = [
        _frame([_layer_chunk('art'), _cel_chunk(0, [1], width=1, height=1)], duration=100),
        _frame([_cel_chunk(0, [2], width=1, height=1)], duration=150),
        _frame([_cel_chunk(0, [3], width=1, height=1)], duration=200),
    ]
    frame0 = _frame([
        _layer_chunk('art'), _frame_tags_chunk([(0, 2, 2, 'spin')]),
        _cel_chunk(0, [1], width=1, height=1),
    ], duration=100)
    path = _write(tmp_path, _aseprite([frame0, frames[1], frames[2]], width=1, height=1))

    _asset, _summary, out = _convert(path)

    assert [(c.name, c.loop_mode) for c in out.clips] == [('spin', emit.LOOP_PINGPONG)]
    assert [f[0] for f in out.clips[0].frames] == [0, 1, 2]
    assert [f[1] for f in out.clips[0].frames] == [100, 150, 200]


def test_tag_that_clamps_to_nothing_falls_back_to_default_clip(tmp_path):
    frame0 = _frame([
        _layer_chunk('art'), _frame_tags_chunk([(5, 2, 0, 'bad')]),
        _cel_chunk(0, [1], width=1, height=1),
    ])
    path = _write(tmp_path, _aseprite([frame0], width=1, height=1))

    _asset, _summary, out = _convert(path)

    assert [c.name for c in out.clips] == ['default']


# ---------------------------------------------------------------------------
# Clipping, cropping, empty/absent cels
# ---------------------------------------------------------------------------

def test_canvas_clipping_excludes_off_canvas_pixels(tmp_path):
    offscreen = [1, 1, 0, 0, 0, 0]
    onscreen = [0, 0, 1, 1, 0, 0]
    frame0 = _frame([
        _layer_chunk('offscreen'), _layer_chunk('onscreen'),
        _cel_chunk(0, offscreen, width=6, height=1, x=-2, y=0),
        _cel_chunk(1, onscreen, width=6, height=1, x=-2, y=0),
    ])
    path = _write(tmp_path, _aseprite([frame0], width=4, height=4))

    _asset, _summary, out = _convert(path)

    assert out.refs[0] == (emit.LAYERED_INVISIBLE, 0, 0)
    assert out.refs[1][0] != emit.LAYERED_INVISIBLE
    assert (out.images[out.refs[1][0]].w, out.images[out.refs[1][0]].h) == (2, 1)
    assert out.refs[1][1:] == (0, 0)


def test_tight_bounds_and_offset(tmp_path):
    pixels = [T] * 9
    pixels[2] = 5  # local (col 2, row 0)
    frame0 = _frame([
        _layer_chunk('dot'),
        _cel_chunk(0, pixels, width=3, height=3, x=1, y=1),
    ])
    path = _write(tmp_path, _aseprite([frame0], width=6, height=6))

    _asset, _summary, out = _convert(path)

    assert (out.images[0].w, out.images[0].h) == (1, 1)
    assert out.images[0].pixels == bytes([5])
    assert out.refs == [(0, 3, 1)]


def test_empty_and_absent_cels_are_invisible(tmp_path):
    frame0 = _frame([
        _layer_chunk('body'), _layer_chunk('spark'),
        _cel_chunk(0, [1] * 4, width=2, height=2),
        _cel_chunk(1, [T, T, T, T], width=2, height=2),
    ])
    frame1 = _frame([_cel_chunk(0, cel_type=a2e.CEL_TYPE_LINKED, linked_frame=0)])
    path = _write(tmp_path, _aseprite([frame0, frame1], width=2, height=2))

    _asset, _summary, out = _convert(path)

    assert out.parts == ['body', 'spark']
    assert out.refs[1][0] == emit.LAYERED_INVISIBLE
    assert out.refs[3][0] == emit.LAYERED_INVISIBLE


# ---------------------------------------------------------------------------
# Reuse: linked, identical within a layer, not across layers, moving links
# ---------------------------------------------------------------------------

def test_linked_cels_reuse_one_image(tmp_path):
    frame0 = _frame([
        _layer_chunk('art'),
        _cel_chunk(0, [1], width=1, height=1),
    ])
    frame1 = _frame([_cel_chunk(0, cel_type=a2e.CEL_TYPE_LINKED, linked_frame=0)])
    path = _write(tmp_path, _aseprite([frame0, frame1], width=2, height=2))

    _asset, summary, out = _convert(path)

    assert len(out.images) == 1
    assert out.refs == [(0, 0, 0), (0, 0, 0)]
    assert summary['linked_refs'] == 1


def test_identical_images_dedup_within_a_layer(tmp_path):
    frame0 = _frame([
        _layer_chunk('art'),
        _cel_chunk(0, [1, 2, 3, 4], width=2, height=2),
    ])
    frame1 = _frame([_cel_chunk(0, [1, 2, 3, 4], width=2, height=2)])
    frame2 = _frame([_cel_chunk(0, [5, 6, 7, 8], width=2, height=2)])
    path = _write(tmp_path, _aseprite([frame0, frame1, frame2], width=2, height=2))

    _asset, summary, out = _convert(path)

    assert len(out.images) == 2
    assert out.refs[0][0] == out.refs[1][0]
    assert out.refs[2][0] != out.refs[0][0]
    assert summary['reused_refs'] == 1


def test_identical_images_across_layers_are_not_deduped(tmp_path):
    frame0 = _frame([
        _layer_chunk('under'), _layer_chunk('over'),
        _cel_chunk(0, [1, 2, 3, 4], width=2, height=2),
        _cel_chunk(1, [1, 2, 3, 4], width=2, height=2),
    ])
    path = _write(tmp_path, _aseprite([frame0], width=2, height=2))

    _asset, _summary, out = _convert(path)

    assert len(out.images) == 2
    assert out.refs[0][0] != out.refs[1][0]


def test_moving_linked_cel_keeps_image_and_moves_offset(tmp_path):
    frame0 = _frame([
        _layer_chunk('ball'),
        _cel_chunk(0, [3], width=1, height=1, x=0, y=0),
    ])
    frame1 = _frame([_cel_chunk(0, cel_type=a2e.CEL_TYPE_LINKED, linked_frame=0, x=2, y=1)])
    path = _write(tmp_path, _aseprite([frame0, frame1], width=4, height=4))

    _asset, summary, out = _convert(path)

    assert len(out.images) == 1
    assert out.refs == [(0, 0, 0), (0, 2, 1)]
    assert summary['linked_refs'] == 1


# ---------------------------------------------------------------------------
# Painter order, names, hidden layers
# ---------------------------------------------------------------------------

def test_painter_order_is_bottom_to_top(tmp_path):
    frame0 = _frame([
        _layer_chunk('bottom'), _layer_chunk('middle'), _layer_chunk('top'),
        _cel_chunk(0, [1], width=1, height=1),
        _cel_chunk(1, [2], width=1, height=1, x=1),
        _cel_chunk(2, [3], width=1, height=1, x=2),
    ])
    path = _write(tmp_path, _aseprite([frame0], width=3, height=3))

    _asset, _summary, out = _convert(path)

    assert out.parts == ['bottom', 'middle', 'top']
    assert [out.refs[i][1] for i in range(3)] == [0, 1, 2]


def test_hidden_layers_are_ignored_and_reported(tmp_path):
    frame0 = _frame([
        _layer_chunk('visible'), _layer_chunk('scratch', flags=0),
        _cel_chunk(0, [1], width=1, height=1),
        _cel_chunk(1, [2], width=1, height=1),
    ])
    path = _write(tmp_path, _aseprite([frame0], width=2, height=2))

    _asset, summary, out = _convert(path)

    assert out.parts == ['visible']
    assert summary['ignored_layers'] == ['scratch']
    assert 'scratch' in a2e.format_layered_summary(summary)


def test_hidden_group_layer_is_ignored_not_rejected(tmp_path):
    frame0 = _frame([
        _layer_chunk('visible'),
        _layer_chunk('group', flags=0, layer_type=a2e.LAYER_TYPE_GROUP),
        _cel_chunk(0, [1], width=1, height=1),
    ])
    path = _write(tmp_path, _aseprite([frame0], width=2, height=2))

    _asset, summary, out = _convert(path)

    assert out.parts == ['visible']
    assert summary['ignored_layers'] == ['group']


def test_part_names_are_preserved(tmp_path):
    frame0 = _frame([
        _layer_chunk('left arm'), _layer_chunk('head'),
        _cel_chunk(0, [1], width=1, height=1),
        _cel_chunk(1, [2], width=1, height=1, x=1),
    ])
    path = _write(tmp_path, _aseprite([frame0], width=2, height=2))

    _asset, _summary, out = _convert(path)

    assert out.parts == ['left arm', 'head']


# ---------------------------------------------------------------------------
# Indexed palette mapping
# ---------------------------------------------------------------------------

def test_indexed_transparent_index_maps_to_15(tmp_path):
    frame0 = _frame([
        _layer_chunk('art'),
        _cel_chunk(0, [7, 0, 7], width=3, height=1),
    ])
    path = _write(tmp_path, _aseprite([frame0], width=3, height=1))

    _asset, _summary, out = _convert(path)

    assert out.images[0].pixels == bytes([7, 15, 7])


def test_indexed_opaque_index_15_is_rejected(tmp_path):
    frame0 = _frame([
        _layer_chunk('art'),
        _cel_chunk(0, [15], width=1, height=1),
    ])
    path = _write(tmp_path, _aseprite([frame0], width=1, height=1))

    with pytest.raises(ValueError, match="cannot be represented in enjin's 0..14 range"):
        _convert(path)


def test_off_canvas_indexed_pixels_do_not_affect_mapping(tmp_path):
    frame0 = _frame([
        _layer_chunk('base'), _layer_chunk('offscreen'),
        _cel_chunk(0, [7], width=1, height=1),
        # Opaque index 15 would be rejected, but the cel lies wholly off-canvas.
        _cel_chunk(1, [15, 15], width=2, height=1, x=-2, y=0),
    ])
    path = _write(tmp_path, _aseprite([frame0], width=1, height=1))

    _asset, _summary, out = _convert(path)

    assert out.refs[1][0] == emit.LAYERED_INVISIBLE


# ---------------------------------------------------------------------------
# RGBA exact-colour mapping
# ---------------------------------------------------------------------------

PALETTE = [(i, i, i) for i in range(15)]


def test_rgba_exact_colour_mapping(tmp_path):
    pixels = _rgba((10, 10, 10, 255), (0, 0, 0, 0), (10, 10, 10, 255))
    frame0 = _frame([
        _layer_chunk('art'),
        _cel_chunk(0, pixels, cel_type=a2e.CEL_TYPE_RAW, width=3, height=1),
    ])
    path = _write(tmp_path, _aseprite(
        [frame0], width=3, height=1, depth=a2e.COLOR_DEPTH_RGBA
    ))

    _asset, _summary, out = _convert(path, PALETTE)

    assert out.images[0].pixels == bytes([10, 15, 10])


def test_rgba_requires_explicit_target_palette(tmp_path):
    frame0 = _frame([
        _layer_chunk('art'),
        _cel_chunk(0, _rgba((1, 1, 1, 255)), cel_type=a2e.CEL_TYPE_RAW),
    ])
    path = _write(tmp_path, _aseprite(
        [frame0], width=1, height=1, depth=a2e.COLOR_DEPTH_RGBA
    ))

    with pytest.raises(ValueError, match='explicit target'):
        _convert(path)


def test_rgba_absent_colours_fail_and_are_listed(tmp_path):
    pixels = _rgba((1, 2, 3, 255), (200, 100, 50, 255))
    frame0 = _frame([
        _layer_chunk('art'),
        _cel_chunk(0, pixels, cel_type=a2e.CEL_TYPE_RAW, width=2, height=1),
    ])
    path = _write(tmp_path, _aseprite(
        [frame0], width=2, height=1, depth=a2e.COLOR_DEPTH_RGBA
    ))

    with pytest.raises(ValueError) as excinfo:
        _convert(path, PALETTE)
    message = str(excinfo.value)
    assert 'absent from target palette' in message
    assert '#010203' in message
    assert '#C86432' in message


def test_rgba_partial_alpha_is_rejected(tmp_path):
    frame0 = _frame([
        _layer_chunk('art'),
        _cel_chunk(0, _rgba((10, 10, 10, 128)), cel_type=a2e.CEL_TYPE_RAW),
    ])
    path = _write(tmp_path, _aseprite(
        [frame0], width=1, height=1, depth=a2e.COLOR_DEPTH_RGBA
    ))

    with pytest.raises(ValueError, match='binary alpha'):
        _convert(path, PALETTE)


def test_off_canvas_rgba_pixels_do_not_affect_validation(tmp_path):
    # The off-canvas cel has both an absent colour and partial alpha; clipping
    # before validation must discard it entirely.
    pixels = _rgba((1, 2, 3, 255), (1, 2, 3, 128))
    frame0 = _frame([
        _layer_chunk('base'), _layer_chunk('offscreen'),
        _cel_chunk(0, _rgba((10, 10, 10, 255)), cel_type=a2e.CEL_TYPE_RAW,
                   width=1, height=1),
        _cel_chunk(1, pixels, cel_type=a2e.CEL_TYPE_RAW, width=2, height=1,
                   x=-2, y=0),
    ])
    path = _write(tmp_path, _aseprite(
        [frame0], width=1, height=1, depth=a2e.COLOR_DEPTH_RGBA
    ))

    _asset, _summary, out = _convert(path, PALETTE)

    assert out.refs[1][0] == emit.LAYERED_INVISIBLE


# ---------------------------------------------------------------------------
# Rejected source features
# ---------------------------------------------------------------------------

@pytest.mark.parametrize('layer_options, message', [
    ({'layer_type': a2e.LAYER_TYPE_GROUP}, 'Group layer semantics are unsupported'),
    ({'layer_type': a2e.LAYER_TYPE_TILEMAP}, 'Tilemap layers are unsupported'),
    ({'blend_mode': 1}, 'Unsupported blend mode 1'),
])
def test_unsupported_layer_semantics_are_rejected(tmp_path, layer_options, message):
    frame0 = _frame([_layer_chunk('bad', **layer_options)])
    path = _write(tmp_path, _aseprite([frame0], width=1, height=1))

    with pytest.raises(ValueError, match=message):
        _convert(path)


def test_nonzero_cel_z_index_is_rejected(tmp_path):
    frame0 = _frame([
        _layer_chunk('art'),
        _cel_chunk(0, [1], width=1, height=1, z_index=2),
    ])
    path = _write(tmp_path, _aseprite([frame0], width=1, height=1))

    with pytest.raises(ValueError, match='z-index'):
        _convert(path)


def test_layer_opacity_below_255_is_rejected(tmp_path):
    frame0 = _frame([
        _layer_chunk('art', opacity=128),
        _cel_chunk(0, [1], width=1, height=1),
    ])
    path = _write(tmp_path, _aseprite(
        [frame0], width=1, height=1, layer_opacity_valid=True
    ))

    with pytest.raises(ValueError, match='layer opacity'):
        _convert(path)


def test_cel_opacity_below_255_is_rejected(tmp_path):
    frame0 = _frame([
        _layer_chunk('art'),
        _cel_chunk(0, [1], width=1, height=1, opacity=128),
    ])
    path = _write(tmp_path, _aseprite([frame0], width=1, height=1))

    with pytest.raises(ValueError, match='cel opacity'):
        _convert(path)


# ---------------------------------------------------------------------------
# Inspection summary + CLI
# ---------------------------------------------------------------------------

def test_summary_reports_required_fields(tmp_path):
    frame0 = _frame([
        _layer_chunk('body'), _layer_chunk('spark'),
        _cel_chunk(0, [1] * 4, width=2, height=2),
        _cel_chunk(1, [T, T, T, T], width=2, height=2),
    ])
    path = _write(tmp_path, _aseprite([frame0], width=2, height=2))

    _asset, summary, _out = _convert(path)

    for key in ('canvas_w', 'canvas_h', 'parts', 'num_frames', 'clips',
                'ignored_layers', 'num_images', 'linked_refs', 'reused_refs',
                'storage_bytes'):
        assert key in summary
    text = a2e.format_layered_summary(summary)
    assert '2x2' in text
    assert 'body' in text
    assert 'frames' in text
    assert 'clips' in text
    assert 'storage' in text
    assert 'ignored' in text


def test_cli_layered_writes_loadable_asset(tmp_path):
    import subprocess

    frame0 = _frame([
        _layer_chunk('body'),
        _cel_chunk(0, [1] * 4, width=2, height=2),
    ])
    ase_path = _write(tmp_path, _aseprite([frame0], width=2, height=2), 'cli.aseprite')
    out_path = os.path.join(str(tmp_path), 'cli.njn')
    tool = os.path.join(os.path.dirname(os.path.abspath(__file__)), 'aseprite2enjin.py')

    result = subprocess.run(
        [sys.executable, tool, ase_path, '--layered', '--output', out_path],
        capture_output=True, text=True, check=True,
    )

    with open(out_path, 'rb') as f:
        out = emit.parse_njn_layered(f.read())
    assert out.parts == ['body']
    assert out.canvas_w == 2
    assert 'Layered export' in result.stdout


if __name__ == '__main__':
    sys.exit(pytest.main([__file__, '-v']))
