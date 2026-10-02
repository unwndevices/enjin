#!/usr/bin/env python3
"""The frozen sprite golden corpus (Tomodachi #295).

``testdata/sprite_golden/`` holds the reference the C++ importer is checked
against once, in the parity run (Tomodachi #299). Two kinds of check here:

- the tool still writes every committed golden byte for byte, so a change to
  the Python sprite modes can't silently drift from the frozen reference;
- each golden decodes to what its case is about, so the frozen bytes really
  carry the fixed behaviour (and not the bug they were built to cover).
"""

import os
import sys

import pytest

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import aseprite2enjin as a2e  # noqa: E402
from enjin_assets import emit  # noqa: E402

CORPUS = os.path.join(os.path.dirname(os.path.abspath(__file__)), 'testdata', 'sprite_golden')
sys.path.insert(0, CORPUS)
from make_corpus import read_manifest  # noqa: E402

MANIFEST = read_manifest()
T = 15  # enjin transparent index


def _path(name):
    return os.path.join(CORPUS, name)


def _golden(name):
    with open(_path(os.path.splitext(name)[0] + '.njn'), 'rb') as handle:
        return handle.read()


def _convert(name, kind, palette):
    """What ``aseprite2enjin.py --v2`` / ``--layered [--palette]`` writes."""
    if kind == 'sheet':
        data, _count, _clips = a2e.emit_njn_v2_sheet(a2e.parse_aseprite(_path(name)), None)
        return data
    target = a2e._load_target_palette(_path(palette)) if palette else None
    asset, _summary = a2e.build_layered_asset(a2e.parse_aseprite_layered(_path(name)), target)
    return emit.build_njn_layered(asset)


@pytest.mark.parametrize('name,kind,palette', MANIFEST, ids=[row[0] for row in MANIFEST])
def test_tool_reproduces_the_frozen_golden(name, kind, palette):
    assert _convert(name, kind, palette) == _golden(name), (
        f'{name}: the tool no longer writes the frozen golden. The corpus is the '
        'parity reference for Tomodachi #299; fix the tool, not the golden.'
    )


def test_manifest_lists_every_input():
    on_disk = sorted(f for f in os.listdir(CORPUS) if f.endswith('.aseprite'))
    assert sorted(row[0] for row in MANIFEST) == on_disk
    for name, _kind, _palette in MANIFEST:
        assert os.path.isfile(_path(os.path.splitext(name)[0] + '.njn')), name


# ---------------------------------------------------------------------------
# What each golden carries
# ---------------------------------------------------------------------------

def _sheet(name):
    parsed = emit.parse_njn(_golden(name))
    cell_w, cell_h, cols, rows = parsed.chunks[emit.CHUNK_META][:4]
    pixels = parsed.chunks[emit.CHUNK_PIXL]
    cell = cell_w * cell_h
    frames = [pixels[i * cell:(i + 1) * cell] for i in range(cols * rows)]
    return (cell_w, cell_h), frames, emit._parse_clip_chunk(parsed.chunks[emit.CHUNK_CLIP])


def _clip_shape(clips):
    return [(c.name, c.loop_mode, [f[0] for f in c.frames], [f[1] for f in c.frames])
            for c in clips]


ONE_PASS = {0: 100, 1: 150, 2: 200}

TAG_GOLDENS = {
    'tag_forward.aseprite':          (emit.LOOP_LOOP,     [0, 1, 2]),
    'tag_reverse.aseprite':          (emit.LOOP_LOOP,     [2, 1, 0]),
    'tag_pingpong.aseprite':         (emit.LOOP_PINGPONG, [0, 1, 2]),
    'tag_pingpong_reverse.aseprite': (emit.LOOP_PINGPONG, [2, 1, 0]),
    'repeat_once.aseprite':          (emit.LOOP_ONCE,     [0, 1, 2]),
    'repeat_n.aseprite':             (emit.LOOP_ONCE,     [0, 1, 2] * 3),
    'repeat_n_pingpong.aseprite':    (emit.LOOP_ONCE,     [0, 1, 2, 1, 0, 1, 2]),
}


@pytest.mark.parametrize('name', TAG_GOLDENS)
def test_tag_golden_clip(name):
    loop_mode, frames = TAG_GOLDENS[name]
    _size, _frames, clips = _sheet(name)
    assert _clip_shape(clips) == [('anim', loop_mode, frames, [ONE_PASS[f] for f in frames])]


def test_untagged_golden_has_one_default_clip():
    _size, _frames, clips = _sheet('untagged.aseprite')
    assert _clip_shape(clips) == [('default', emit.LOOP_LOOP, [0, 1, 2], [100, 150, 200])]


def test_zero_duration_golden_holds_the_header_speed():
    _size, _frames, clips = _sheet('zero_duration.aseprite')
    # Frame 1 is 0 ms in the file; the header speed is 90 ms.
    assert _clip_shape(clips) == [('default', emit.LOOP_LOOP, [0, 1, 2], [120, 90, 80])]


def test_old_palette_golden_reads_the_0x0004_colours():
    # Indexed sprites keep their slots, so the palette shows in the RGBA parse.
    parsed = a2e.parse_aseprite(_path('old_palette.aseprite'), rgba_output=True)
    rgba = parsed['frames'][0]
    pixels = [tuple(rgba[i:i + 4]) for i in range(0, len(rgba), 4)]
    assert pixels == [(i, 255 - i, (i * 7) % 256, 255) for i in range(1, 9)]
    _size, frames, _clips = _sheet('old_palette.aseprite')
    assert frames == [bytes(range(1, 9))]


def test_z_index_tie_golden_paints_the_lower_z_first():
    (w, _h), [frame], _clips = _sheet('z_index_tie.aseprite')
    # "low" (slot 5, z +1) covers (0..2, 0..2); "high" (slot 9, z 0) covers
    # (1..3, 1..3). Both sit at order 1, so "high" paints first and "low" wins
    # the 2x2 overlap.
    assert frame[1 * w + 1] == 5
    assert frame[2 * w + 2] == 5
    assert frame[3 * w + 3] == 9
    assert frame[0] == 5


def test_sheet_golden():
    (w, h), frames, clips = _sheet('sheet.aseprite')
    assert (w, h) == (6, 5)
    assert len(frames) == 5
    assert frames[2] == frames[1]                  # linked cel
    assert set(frames[3]) == {T}                   # empty frame
    assert 14 not in b''.join(frames)              # hidden "guide" layer
    assert frames[1][4 * w + 5] == 3               # cel clipped at the bottom-right
    assert frames[4][0:2] == bytes([4, 4])         # cel clipped at the left
    assert frames[4][2] == T
    assert _clip_shape(clips) == [
        ('idle', emit.LOOP_LOOP, [0, 1, 2], [100, 60, 60]),
        ('hit', emit.LOOP_ONCE, [4, 3], [140, 70]),
    ]


def test_layered_golden():
    out = emit.parse_njn_layered(_golden('layered.aseprite'))
    assert (out.canvas_w, out.canvas_h) == (8, 8)
    assert out.parts == ['body', 'eye', 'hat']     # hidden "notes" dropped
    assert out.durations == [100, 80, 120]
    body, eye, hat = (out.refs[i::3] for i in range(3))
    assert body[0] == body[1] == body[2]           # linked
    assert (eye[0][1:], eye[1][1:], eye[2]) == ((3, 4), (4, 4), eye[0])
    assert eye[0][0] == eye[1][0]                  # same image, moved
    assert hat[0] == hat[1] and hat[2][0] == emit.LAYERED_INVISIBLE
    assert _clip_shape(out.clips) == [
        # Ping-pong-reverse, repeat 2: back, then forth without the turn frame.
        ('blink', emit.LOOP_ONCE, [2, 1, 0, 1, 2], [120, 80, 100, 80, 120]),
    ]


if __name__ == '__main__':
    sys.exit(pytest.main([__file__, '-v']))
