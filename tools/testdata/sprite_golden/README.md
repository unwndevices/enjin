# Sprite golden corpus — FROZEN

This is the reference the C++ sprite importer (`enjin_sprite_import`) is checked
against **once**, in the parity run (Tomodachi #299). After that run, Python's sprite
modes are deleted (ADR-0015). Each `.aseprite` input has, next to it, the `.njn` that the
fixed `aseprite2enjin.py` writes (Tomodachi #295).

**Don't edit or regenerate these files.** The parity run compares against these exact
bytes. If the Python tool stops reproducing a golden, fix the tool, not the golden
(`test_sprite_golden_corpus.py` checks this).

`MANIFEST` lists each input with its kind (`sheet` = `--v2`, `layered` = `--layered`)
and its palette. `make_corpus.lua` and `make_corpus.py` record how the corpus was built
(Aseprite 1.3.18). They refuse to overwrite it without `--force`.

| Input | Kind | Covers |
|-------|------|--------|
| `tomo_tune2` | layered | The seed: a copy of `../tomo_tune2.aseprite`. It's RGBA, so it needs `tomo_tune2.gpl` (the same colours as the C++ `tomo` preset). Untagged, so it gets a `default` loop clip. |
| `tag_forward` | sheet | Forward, repeat 0 → frames in order, `loop` |
| `tag_reverse` | sheet | Reverse, repeat 0 → reversed frames, `loop` |
| `tag_pingpong` | sheet | Ping-pong, repeat 0 → `pingpong` |
| `tag_pingpong_reverse` | sheet | Ping-pong-reverse, repeat 0 → reversed frames, `pingpong` |
| `repeat_once` | sheet | Repeat 1 → `once` (previously imported as `loop`) |
| `repeat_n` | sheet | Repeat 3 → the frames unrolled 3×, `once` |
| `repeat_n_pingpong` | sheet | Ping-pong, repeat 3 → `0 1 2 1 0 1 2`, `once` (a pass doesn't repeat its turn frame) |
| `untagged` | sheet | No tags → one `default` loop clip over all frames |
| `zero_duration` | sheet | Frame 1 patched to 0 ms → it holds the header speed (patched to 90 ms) |
| `old_palette` | sheet | Only an old `0x0004` palette chunk, a 256-colour one (count-0 packet). Every Aseprite 1.3 indexed file here has only `0x0004`. Indexed sprites keep their slots, so the colours show only in `parse_aseprite(..., rgba_output=True)`, not in the `.njn`. |
| `z_index_tie` | sheet | Two cels at the same order (layer + z): the lower z-index paints first (spec NOTE.5) |
| `sheet` | sheet | Hidden layer, linked cel, empty frame, cels clipped by the canvas, two sub-range tags |
| `layered` | layered | Hidden layer, linked cels, a moving part, a missing part, ping-pong-reverse repeat 2 |

Every minimal input is indexed with transparent index 0, and its art uses slots 1..14.

**Known difference for the parity run:** the C++ importer gives new layered imports a
bottom-centre `LPIV` pivot. The Python tool writes no `LPIV` unless `--pivot` is given.
With the C++ CLI from #291, every sheet golden matches byte for byte, and both layered
goldens match except for that pivot.

The source palette never reaches a `.njn`, but the two readers treat it slightly
differently. Python follows the spec and ignores a frame's `0x0004` chunk when that frame
also has a `0x2019` chunk. C++ applies both, in chunk order.
