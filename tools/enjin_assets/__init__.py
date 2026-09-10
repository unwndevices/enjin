"""enjin_assets — shared asset-import library for the enjin toolchain (issue #87).

The pipeline that turns purchased tilesets and Aseprite art into the engine's
binary formats (`.njn` v2 tilesets/sheets, `.njm` maps) is split into small,
independently-testable modules so both front-ends — ``tiled2enjin.py`` (Tiled
maps) and ``aseprite2enjin.py`` (Aseprite sheets) — share one implementation:

* :mod:`enjin_assets.oklab`    — sRGB ↔ Oklab colour conversions.
* :mod:`enjin_assets.palette`  — load a 15-index ``.gpl`` ramp palette.
* :mod:`enjin_assets.quantize` — nearest-in-Oklab RGBA → 4bpp index + ΔEOK stats.
* :mod:`enjin_assets.png`      — decode a PNG and slice a tilesheet into tiles.
* :mod:`enjin_assets.tilebank` — flip-aware tile dedupe, id assignment, 511 cap.
* :mod:`enjin_assets.emit`     — the ``.njn`` v2 typed-chunk writer and ``.njm`` writer.
* :mod:`enjin_assets.tiled`    — parse Tiled ``.tmx``/``.tsx`` maps and tilesets.

The binary layouts mirror the C++ single-source-of-truth headers
``enjin2/graphics/njn2.hpp`` and ``enjin2/graphics/tilemap_asset.hpp``.

Submodules are imported lazily (plain ``import enjin_assets.oklab``) to keep the
package import cheap and free of ordering constraints during development.
"""

__all__ = ["oklab", "palette", "quantize", "png", "tilebank", "emit", "tiled"]
