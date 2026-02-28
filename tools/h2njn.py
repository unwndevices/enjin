#!/usr/bin/env python3
"""h2njn.py — Convert enjin C header sprite data to .njn binary format.

Usage:
    python tools/h2njn.py tests/pikachu.h --cellw 38 --cellh 38 --cols 1 --rows 1 -o tests/test_pikachu.njn

The tool parses a C header containing `const uint8_t name_data[] = { ... };`
and writes the .njn binary (8-byte header + raw pixel data).
"""

import argparse
import re
import struct
import sys


def parse_header_bytes(text: str) -> list[int]:
    """Extract uint8_t array values from C header text."""
    # Find the array body between { and }
    match = re.search(r'\{([^}]+)\}', text, re.DOTALL)
    if not match:
        print("Error: could not find uint8_t array in header file", file=sys.stderr)
        sys.exit(1)

    body = match.group(1)
    # Extract all hex/decimal byte values
    values = []
    for token in re.findall(r'0[xX][0-9a-fA-F]+|\d+', body):
        values.append(int(token, 0))
    return values


def main():
    parser = argparse.ArgumentParser(
        description="Convert enjin C header sprite data to .njn binary"
    )
    parser.add_argument("input", help="Input .h file path")
    parser.add_argument("-o", "--output", required=True, help="Output .njn file path")
    parser.add_argument("--cellw", type=int, required=True, help="Cell width in pixels")
    parser.add_argument("--cellh", type=int, required=True, help="Cell height in pixels")
    parser.add_argument("--cols", type=int, default=1, help="Grid columns (default: 1)")
    parser.add_argument("--rows", type=int, default=1, help="Grid rows (default: 1)")
    args = parser.parse_args()

    # Read and parse C header
    with open(args.input, 'r') as f:
        text = f.read()
    pixels = parse_header_bytes(text)

    expected = args.cellw * args.cellh * args.cols * args.rows
    if len(pixels) != expected:
        print(f"Warning: header has {len(pixels)} bytes, expected {expected} "
              f"({args.cellw}x{args.cellh}x{args.cols}x{args.rows})",
              file=sys.stderr)

    # Write .njn binary
    header = struct.pack('8B',
        ord('N'), ord('J'),  # magic
        1,                    # version
        args.cellw,
        args.cellh,
        args.cols,
        args.rows,
        0,                    # reserved
    )

    with open(args.output, 'wb') as f:
        f.write(header)
        f.write(bytes(pixels))

    print(f"Wrote {args.output}: {len(header) + len(pixels)} bytes "
          f"({args.cellw}x{args.cellh}, {args.cols}x{args.rows}, {len(pixels)} px)")


if __name__ == '__main__':
    main()
