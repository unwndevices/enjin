# Sprite golden corpus

The C++ sprite importer's golden corpus. `sprite_import_test` (SPI-27) imports every
input and checks the result byte for byte against the `.njn` next to it.

**Don't regenerate these files to make a test pass.** A golden changes only when the
importer's output is meant to change. Then rebuild it with
`make_corpus.py --force --cli <enjin_sprite_import>`, and say why in the commit.

The goldens were first written by the fixed Python `aseprite2enjin.py` (Tomodachi #295).
In the parity run (Tomodachi #299), the C++ CLI matched every sheet golden byte for byte.
Both layered goldens differed by one deliberate change only: the C++ importer gives a new
layered import a bottom-centre `LPIV` pivot, and Python wrote none. These two goldens
were then rewritten by the CLI, and Python's sprite modes were deleted (ADR-0015).

`MANIFEST` lists each input with its kind (`sheet` or `layered`) and its palette preset.
`make_corpus.lua` and `make_corpus.py` record how the corpus was built (Aseprite 1.3.18).
They refuse to overwrite it without `--force`.

| Input | Kind | Covers |
|-------|------|--------|
| `tomo_tune2` | layered | The seed: a copy of `../tomo_tune2.aseprite`. It's RGBA, so it needs the `tomo` palette preset. Untagged, so it gets a `default` loop clip. |
| `tag_forward` | sheet | Forward, repeat 0 → frames in order, `loop` |
| `tag_reverse` | sheet | Reverse, repeat 0 → reversed frames, `loop` |
| `tag_pingpong` | sheet | Ping-pong, repeat 0 → `pingpong` |
| `tag_pingpong_reverse` | sheet | Ping-pong-reverse, repeat 0 → reversed frames, `pingpong` |
| `repeat_once` | sheet | Repeat 1 → `once` (previously imported as `loop`) |
| `repeat_n` | sheet | Repeat 3 → the frames unrolled 3×, `once` |
| `repeat_n_pingpong` | sheet | Ping-pong, repeat 3 → `0 1 2 1 0 1 2`, `once` (a pass doesn't repeat its turn frame) |
| `untagged` | sheet | No tags → one `default` loop clip over all frames |
| `zero_duration` | sheet | Frame 1 patched to 0 ms → it holds the header speed (patched to 90 ms) |
| `old_palette` | sheet | Only an old `0x0004` palette chunk, a 256-colour one (count-0 packet). Every Aseprite 1.3 indexed file here has only `0x0004`. Indexed sprites keep their slots, so the colours never reach the `.njn`. |
| `z_index_tie` | sheet | Two cels at the same order (layer + z): the lower z-index paints first (spec NOTE.5) |
| `sheet` | sheet | Hidden layer, linked cel, empty frame, cels clipped by the canvas, two sub-range tags |
| `layered` | layered | Hidden layer, linked cels, a moving part, a missing part, ping-pong-reverse repeat 2 |

Every minimal input is indexed with transparent index 0, and its art uses slots 1..14.
