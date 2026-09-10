# tiled2enjin.py + the `enjin_assets` import library

Turns purchased itch.io tilesets authored in **Tiled** into the engine's binary
formats — a `.njn` v2 tileset and a `.njm` map — plus a ColliderSet sidecar.
This is the ADR-0003 §7 asset pipeline (issue #87), sitting over the shared
`enjin_assets/` library that both front-ends use:

| Module                    | Responsibility |
|---------------------------|----------------|
| `enjin_assets.oklab`      | sRGB ↔ Oklab colour conversion (vectorised). |
| `enjin_assets.palette`    | Load a 15-index `.gpl` ramp palette. |
| `enjin_assets.quantize`   | Nearest-in-Oklab RGBA → 4bpp index + ΔEOK histogram. |
| `enjin_assets.png`        | Decode a PNG and slice a spacing/margin tilesheet. |
| `enjin_assets.tilebank`   | Flip-aware tile dedupe, 9-bit id assignment, 512-id cap. |
| `enjin_assets.tilemap_attr` | `TileAttr` (SOLID / ONEWAY / DIR / kind), the Python mirror of the C++ record. |
| `enjin_assets.emit`       | The `.njn` v2 typed-chunk writer + `.njm` writer + a validating reader. |
| `enjin_assets.tiled`      | Parse `.tmx`/`.tsx` (CSV + base64/gzip/zlib; refuses zstd + infinite maps). |
| `enjin_assets.pipeline`   | Orchestration: quantise → orient → dedupe → pack → emit → preview. |

The binary layouts are the Python mirror of the C++ single source of truth:
`enjin2/graphics/njn2.hpp` (the v2 container) and
`enjin2/graphics/tilemap_asset.hpp` (the `.njm` map + cell packing). The emit
module's `parse_njn` reproduces `NjnV2Reader::open`'s validation, so a byte blob
that round-trips in the tests is loadable on device/web.

## Usage

```sh
# Convert a Tiled map (external .tsx tileset resolved relative to the .tmx).
python tools/tiled2enjin.py path/to/map.tmx --output out/base

# With a preview PNG reconstructed from the emitted binaries + ΔEOK report.
python tools/tiled2enjin.py path/to/map.tmx --output out/base --preview out/base.png

# A different target palette (name in tools/palettes/, or a path).
python tools/tiled2enjin.py map.tmx --palette enjin_gameboy
```

Outputs (given `--output out/base`):

* `out/base.njn` — v2 tileset: `META` geometry, `PIXL` quantised 4bpp pixels,
  and an `ATTR` chunk **only when** the Tiled tileset authors per-tile
  attributes (class/`solid`/`oneway`/`dir`/`kind` properties).
* `out/base.njm` — 16-bit cells `[band|vflip|hflip|palbank:4|tileid:9]`.
* `out/base.colliders.lua` — **only when** the map has object-layer colliders;
  a Lua table mirroring `engine.scene.colliders()` (segments / circles / aabbs,
  each with restitution + kind).

### What the converter does

* **Quantises** each RGB tile to the 15-colour palette by nearest distance in
  Oklab (dithering off). The ΔEOK histogram in the report is the quality gate a
  human judges — a purchased pack whose palette is far from `enjin_default` will
  show a higher mean error, which is expected.
* **Flip-dedupes** tiles over the four cell orientations (identity, H, V, HV);
  mirror-equivalent art collapses to one id with the flip bits set in the cell.
* **Materialises Tiled's 90° (diagonal) flag** as distinct rotated tiles, since
  the 16-bit cell has no rotation bit — reported as the "D-materialised" count.
* **Flattens layers** into one banded cell grid: the bottom layer renders under
  (band 0 → L0), any upper layer over (band 1 → L2), top-most non-empty wins.

## Aseprite front-end (`aseprite2enjin.py --v2`)

Aseprite cannot read Tiled, so it stays a separate front-end for authored art
(the pinball slice). Its existing C-header, v1 `.njn` sprite, and v1 `.njn`+`.njm`
tilemap paths are unchanged. The new `--v2` flag emits a **`.njn` v2 sheet** with
a `CLIP` chunk built from **Aseprite frame tags** (per-frame durations from the
frame headers; loop direction → Once/Loop/PingPong):

```sh
python tools/aseprite2enjin.py anim.aseprite --v2 --output out/anim.njn
```

## Known limitation — runtime loader

The **device/web runtime loader for `.njn` v2 / `.njm` is deferred** (issue #82
shipped folder applets + MEMFS/LittleFS staging but not the file→`C_Tilemap`
loader; `engine.sprite.load` currently reads the v1 flat `.njn` only and ignores
ATTR/CLIP). So "renders in the web sim" through the engine's own loader is gated
on that follow-up. Until then, `--preview` reconstructs the map image directly
from the emitted `.njn`+`.njm` as the round-trip proof, and the collider Lua
sidecar is the runtime-consumable collider form (no binary collider chunk exists
yet; the v2 container makes adding one a zero-break change).

## Tests

```sh
cd tools && python -m pytest
```
