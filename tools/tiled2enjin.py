#!/usr/bin/env python3
"""tiled2enjin.py — Convert a Tiled .tmx map into enjin .njn v2 + .njm binaries.

Front-end over the shared ``enjin_assets`` library (issue #87, decision #53).
Tiled is the map/attribute/collider authoring surface for purchased packs:

    tiled2enjin.py map.tmx [--output base] [--palette enjin_default]
                   [--preview base_preview.png] [--strict]

Emits ``<base>.njn`` (v2 tileset: quantised PIXL + per-tile ATTR when authored),
``<base>.njm`` (packed 16-bit cells), and ``<base>.colliders.lua`` when the map
has object-layer colliders. ``--preview`` writes a PNG reconstructed from the
emitted binaries (a visual round-trip proof) and prints the ΔEOK histogram +
dedupe yield.
"""

import argparse
import os
import sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))

from enjin_assets import palette as palette_mod  # noqa: E402
from enjin_assets import pipeline, tiled  # noqa: E402


def main(argv=None):
    parser = argparse.ArgumentParser(description="Convert a Tiled .tmx to enjin .njn v2 + .njm.")
    parser.add_argument("input", help="Input .tmx map file")
    parser.add_argument("--output", default=None,
                        help="Output path base (default: input without extension)")
    parser.add_argument("--palette", default=None,
                        help="Palette .gpl path or name in tools/palettes/ (default: enjin_default)")
    parser.add_argument("--preview", default=None, metavar="PNG",
                        help="Write a preview PNG reconstructed from the emitted binaries")
    parser.add_argument("--strict", action="store_true",
                        help="Fail on out-of-range tiles instead of warning")
    args = parser.parse_args(argv)

    if not os.path.isfile(args.input):
        print(f"Error: file not found: {args.input}", file=sys.stderr)
        return 1

    pal = _load_palette(args.palette)
    try:
        tmap = tiled.parse_tmx(args.input)
        result = pipeline.convert_tiled(tmap, pal, strict=args.strict)
    except ValueError as e:
        print(f"Error: {e}", file=sys.stderr)
        return 1

    base = args.output or os.path.splitext(args.input)[0]
    njn_path = base + ".njn"
    njm_path = base + ".njm"
    os.makedirs(os.path.dirname(os.path.abspath(njn_path)), exist_ok=True)
    with open(njn_path, "wb") as f:
        f.write(result.njn)
    with open(njm_path, "wb") as f:
        f.write(result.njm)
    print(f"Written: {njn_path}  ({len(result.njn)} bytes)")
    print(f"Written: {njm_path}  ({len(result.njm)} bytes)")

    if result.colliders_lua is not None:
        coll_path = base + ".colliders.lua"
        with open(coll_path, "w") as f:
            f.write(result.colliders_lua)
        print(f"Written: {coll_path}")

    if args.preview:
        from PIL import Image
        img = pipeline.render_preview(result.njn, result.njm, pal)
        Image.fromarray(img, "RGB").save(args.preview)
        print(f"Written: {args.preview}  (preview {img.shape[1]}x{img.shape[0]})")

    print()
    print(result.report())
    return 0


def _load_palette(spec):
    if not spec:
        return palette_mod.load_default()
    if os.path.isfile(spec):
        return palette_mod.load_gpl(spec)
    candidate = os.path.join(palette_mod.PALETTES_DIR, spec if spec.endswith(".gpl") else spec + ".gpl")
    return palette_mod.load_gpl(candidate)


if __name__ == "__main__":
    sys.exit(main())
