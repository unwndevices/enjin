#!/usr/bin/env python3
"""Build the sprite golden corpus (Tomodachi #295, #299). Provenance only.

1. Aseprite runs make_corpus.lua to author the minimal inputs.
2. zero_duration.aseprite gets frame 1's duration patched to 0 ms and the
   header speed to 90 ms (Aseprite clamps a frame to at least 1 ms).
3. tomo_tune2.aseprite is copied from ../ (the seed).
4. enjin_sprite_import writes each MANIFEST input's <stem>.njn golden.

    python3 make_corpus.py --force --cli build-importer/enjin_sprite_import

Without --force it refuses to touch an existing corpus: sprite_import_test
(SPI-27) pins the importer to these exact bytes (see README.md).
"""

import argparse
import os
import shutil
import struct
import subprocess
import sys

HERE = os.path.dirname(os.path.abspath(__file__))
TESTDATA = os.path.dirname(HERE)

ZERO_DURATION_FRAME = 1
ZERO_DURATION_SPEED_MS = 90


def read_manifest():
    """MANIFEST rows as (input, kind, palette or None)."""
    rows = []
    with open(os.path.join(HERE, 'MANIFEST')) as handle:
        for line in handle:
            line = line.split('#', 1)[0].strip()
            if not line:
                continue
            name, kind, palette = line.split()
            rows.append((name, kind, None if palette == '-' else palette))
    return rows


def patch_zero_duration(path):
    data = bytearray(open(path, 'rb').read())
    struct.pack_into('<H', data, 18, ZERO_DURATION_SPEED_MS)
    offset = 128
    for _ in range(ZERO_DURATION_FRAME):
        offset += struct.unpack_from('<I', data, offset)[0]
    struct.pack_into('<H', data, offset + 8, 0)  # frame duration
    open(path, 'wb').write(bytes(data))


def main():
    parser = argparse.ArgumentParser(description=__doc__.split('\n', 1)[0])
    parser.add_argument('--force', action='store_true',
                        help='overwrite the frozen corpus')
    parser.add_argument('--cli', default='enjin_sprite_import',
                        help='path to the enjin_sprite_import binary')
    args = parser.parse_args()

    rows = read_manifest()
    existing = [n for n, _k, _p in rows if os.path.exists(os.path.join(HERE, n))]
    if existing and not args.force:
        sys.exit('the corpus is frozen (see README.md); pass --force to rebuild it')

    subprocess.run(['aseprite', '-b', '--script-param', f'out={HERE}',
                    '--script', os.path.join(HERE, 'make_corpus.lua')], check=True)
    patch_zero_duration(os.path.join(HERE, 'zero_duration.aseprite'))
    shutil.copyfile(os.path.join(TESTDATA, 'tomo_tune2.aseprite'),
                    os.path.join(HERE, 'tomo_tune2.aseprite'))

    for name, kind, palette in rows:
        src = os.path.join(HERE, name)
        cmd = [args.cli, src, '-o', os.path.splitext(src)[0] + '.njn', f'--{kind}']
        if palette:
            cmd += ['--palette', palette]
        subprocess.run(cmd, check=True)


if __name__ == '__main__':
    main()
