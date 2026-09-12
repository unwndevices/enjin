# Aseprite to enjin Asset Conversion Tooling

Tools for creating sprites in Aseprite using enjin's exact palette and exporting directly to C headers.

## Quick Start

1. Import `tools/palettes/enjin_default.gpl` (or `enjin_gameboy.gpl`) into Aseprite
2. Draw your sprite using indexed colors 0-14; use index 15 as transparent
3. Run the converter:

```
python3 tools/aseprite2enjin.py player.aseprite
```

This produces `player.h` with a `const uint8_t player_data[]` array ready to include in your project.

## Palette Setup

1. In Aseprite: Palettes panel > menu > Load Palette > select `tools/palettes/enjin_default.gpl`
2. To make it permanent, copy the `.gpl` to `~/.config/aseprite/palettes/` (Linux) or `%APPDATA%\Aseprite\palettes\` (Windows)
3. Set new sprites to indexed color: Sprite > Color Mode > Indexed

## Converter Usage

**Single sprite (one frame):**
```
python3 tools/aseprite2enjin.py player.aseprite
```

**Animation (multiple Aseprite frames):**
```
python3 tools/aseprite2enjin.py walk.aseprite --name player_walk
```

**Spritesheet grid packed into a single image:**
```
python3 tools/aseprite2enjin.py tileset.aseprite --grid 8x8 --name tiles
```

**Explicit output path:**
```
python3 tools/aseprite2enjin.py hero.aseprite --name hero --output src/assets/hero.h
```

**Options:**

| Flag | Default | Description |
|------|---------|-------------|
| `--name NAME` | derived from filename | C identifier for the array |
| `--output FILE` | same dir as input, `.h` extension | Output header path |
| `--grid WxH` | none | Cell size for spritesheet-in-image mode |
| `--tilemap` | off | Dice under/over layers into a v1 `.njn` tileset + `.njm` map |
| `--v2` | off | Emit a `.njn` **v2** container sheet (META+PIXL, plus a CLIP chunk from Aseprite frame tags) |
| `--layered` | off | Emit a `.njn` **v2 layered** asset (cropped/deduplicated source-layer parts, frame-part references, durations, clips) |
| `--palette` | none | Target Enjin `.gpl` palette (path or `tools/palettes` name); required for RGBA `--layered` sources |

**`.njn` v2 animation sheet (frame tags → clips):**
```
python3 tools/aseprite2enjin.py walk.aseprite --v2 --output walk.njn
```
Each Aseprite frame becomes a sheet cell; every frame tag becomes a named clip
whose per-frame durations come from the frame headers and whose loop mode maps
from the tag's direction (forward/reverse → Loop, ping-pong → PingPong). See
`README_tiled2enjin.md` for the shared `enjin_assets` library and the `.njn` v2
container layout.

**`.njn` v2 layered sprite (source layers → parts):**
```
python3 tools/aseprite2enjin.py hero.aseprite --layered --output hero.njn
python3 tools/aseprite2enjin.py hero_rgba.aseprite --layered --palette enjin_default --output hero.njn
```
Each visible flat image layer (Normal blend, binary alpha, full opacity) becomes
one named sprite part in bottom-to-top painter order. Cels are clipped to the
authored canvas, tightly cropped, and deduplicated within their source layer
(linked cels and identical artwork alike). Empty and absent cels become
invisible frame-part references. Indexed sources map their palette indices to
Enjin indices (index 15 is transparency); RGBA sources need `--palette` and
match colours exactly. Groups, tilemap layers, non-Normal blending, nonzero cel
z-index, opacity below 255, and partially transparent painted pixels are
rejected. The converter prints an inspection summary (parts, images, reuse,
ignored layers, clips, storage).

## Using in enjin

```cpp
#include "enjin2/graphics/sprite.hpp"
#include "assets/player.h"   // generated header

// Construct the sprite sheet (non-owning pointer to static data)
enjin2::SpriteSheet player(player_data, 8, 8, 4, 1);  // 8x8 cells, 4 cols, 1 row

// Draw frame 2 at position (10, 20)
player.draw(canvas, 2, 10, 20);
```

The `SpriteSheet` constructor takes `(data, cellW, cellH, cols, rows)`. The usage comment at the bottom of each generated header gives the exact values for that sprite.

## Palette Reference

The default palette is a PICO-8 variant with 15 colors. Index 15 is always transparent.

| Index | Hex | Name |
|-------|-----|------|
|  0 | `#1A1C2C` | dark navy |
|  1 | `#5D275D` | dark purple |
|  2 | `#B13E53` | dark red |
|  3 | `#EF7D57` | orange |
|  4 | `#FFCD75` | yellow |
|  5 | `#A7F070` | light green |
|  6 | `#38B764` | green |
|  7 | `#257179` | dark teal |
|  8 | `#29366F` | dark blue |
|  9 | `#3B5DC9` | blue |
| 10 | `#41A6F6` | light blue |
| 11 | `#73EFF7` | cyan |
| 12 | `#F4F4F4` | near-white |
| 13 | `#566C86` | slate blue-grey |
| 14 | `#333C57` | dark slate |
| 15 | `#FF00FF` | TRANSPARENT (skip when drawing) |

The gameboy palette uses indices 0-3 (four green shades); indices 4-14 are unused (black placeholders); 15 is transparent.

## Limitations

- Indexed-color mode only. Set Sprite > Color Mode > Indexed before exporting.
- Only cel types 0 (raw) and 2 (compressed) are parsed; tilemap chunks are skipped.
- No layer compositing: only the first cel chunk per frame is used.
- Pixels are masked to lower nibble (`& 0x0F`); only indices 0-15 are valid in enjin.
- `--grid` reads only the first Aseprite frame; additional frames are ignored.
