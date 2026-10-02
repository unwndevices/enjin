# Aseprite to enjin

Two tools take Aseprite art into enjin:

- **Sprites** (`.njn` v2 sheets and layered sprites): the C++ CLI `enjin_sprite_import`
  (`tools/sprite_import`). It runs `enjin2::importSprite()`, the same importer the Studio's
  Sprites workspace runs (ADR-0015).
- **Tilemaps** (a `.njn` tileset + `.njm` map): `aseprite2enjin.py --tilemap`.

`aseprite2enjin.py` no longer converts sprites. Its sprite modes (the default C header and
its `--grid` spritesheet, `--v2`, `--layered` and `--pivot`) were deleted in Tomodachi
#299, after the C++ importer matched their output.

## Palette Setup

1. In Aseprite: Palettes panel > menu > Load Palette > select `tools/palettes/enjin_default.gpl`
2. To make it permanent, copy the `.gpl` to `~/.config/aseprite/palettes/` (Linux) or `%APPDATA%\Aseprite\palettes\` (Windows)
3. Set new sprites to indexed color: Sprite > Color Mode > Indexed

## Sprites: `enjin_sprite_import`

Build it from the enjin CMake project (host builds only):

```
cmake -S . -B build && cmake --build build --target enjin_sprite_import
```

```
enjin_sprite_import walk.aseprite -o walk.njn
enjin_sprite_import hero_rgba.aseprite -o hero.njn --palette tomo
enjin_sprite_import walk.aseprite --reimport walk.njn
```

| Flag | Default | Description |
|------|---------|-------------|
| `-o FILE` | none | Output `.njn` path |
| `--layered` / `--sheet` | layered when more than one layer is visible | Override the kind |
| `--palette NAME` | `default` (the system palette) | RGBA target palette preset: `default`, `tomo`, `pico8`, `gameboy` |
| `--reimport FILE` | none | Replace `FILE`'s pixels and frames and keep its clips by name. Writes over `FILE` unless `-o` is given. |

A **sheet** has one cell per Aseprite frame (META + PIXL + CLIP). A **layered sprite**
has one part per visible layer (LHDR + LIMG + LPRT + LREF + LDUR + LPIV + CLIP). It gets a
bottom-centre pivot.

Each frame tag becomes a named clip, and the per-frame durations come from the frame
headers. A 0 ms frame falls back to the header speed, or to 100 ms if that is 0 too.
Tags map like this:

| Tag | Clip |
|-----|------|
| forward / reverse | frames in order / reversed |
| ping-pong / ping-pong-reverse | frames in order / reversed, `pingpong` |
| repeat 0 | `loop` (`pingpong` for the ping-pong directions) |
| repeat 1 | `once` |
| repeat N > 1 | the N passes unrolled, as `once`. A ping-pong pass alternates direction and doesn't repeat its turn frame. |

An untagged file gets one looping `default` clip over all frames. Indexed sources keep their
indices (index 15 is transparency). RGBA sources need binary alpha and an exact match to the
`--palette` colours; there is no quantisation. `include/enjin2/import/sprite_import.hpp`
has the full rules. `testdata/sprite_golden/` is the importer's golden corpus.

## Tilemaps: `aseprite2enjin.py --tilemap`

```
python3 tools/aseprite2enjin.py room.aseprite --tilemap --output out/room
```

This writes `out/room.njn` (a v1 tileset, 16x16 tiles unless `--grid WxH` is given) and
`out/room.njm` (the map). A layer whose name contains `over` is the over band; one whose
name contains `under`, `floor` or `base` is the under band. Without those names, the
bottom layer is under and the top layer is over. Tiles are deduplicated. Only frame 0 is
read, and the source must be indexed.

The module's `parse_aseprite()` (flattened frames, optionally as RGBA) is also used by
`scripts/live_preview.py` in Tomodachi.

## Palette Reference

This is the `tomo` preset, the authored palette that used to be the default. The system default is now the green ramp (`default`). Index 15 is always transparent.

| Index | Hex | Name |
|-------|-----|------|
|  0 | `#000000` | black |
|  1 | `#898989` | grey |
|  2 | `#FFFFFF` | white |
|  3 | `#1A1C2C` | navy |
|  4 | `#5D275D` | purple |
|  5 | `#B13E53` | red |
|  6 | `#EF7D57` | orange |
|  7 | `#FFCD75` | yellow |
|  8 | `#A7F070` | light green |
|  9 | `#38B764` | green |
| 10 | `#257179` | teal |
| 11 | `#3B5DC9` | blue |
| 12 | `#73EFF7` | cyan |
| 13 | `#566C86` | slate |
| 14 | `#333C57` | dark slate |
| 15 | `#FF00FF` | TRANSPARENT (skip when drawing) |

The legacy PICO-8 variant is still selectable at runtime as the `pico8` palette preset. The gameboy palette uses indices 0-3 (four green shades); indices 4-14 are unused (black placeholders); 15 is transparent.
