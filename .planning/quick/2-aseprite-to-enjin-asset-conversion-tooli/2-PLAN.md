---
phase: quick
plan: 2
type: execute
wave: 1
depends_on: []
files_modified:
  - tools/palettes/enjin_default.gpl
  - tools/palettes/enjin_gameboy.gpl
  - tools/aseprite2enjin.py
  - tools/README_aseprite2enjin.md
autonomous: true
requirements: [QUICK-02]

must_haves:
  truths:
    - "Artist can import enjin_default.gpl into Aseprite and see all 15 preset colors plus transparent slot"
    - "Artist can import enjin_gameboy.gpl into Aseprite and see 4 green shades plus transparent slot"
    - "Running aseprite2enjin.py on an indexed-color .aseprite file produces a valid C header with const uint8_t array"
    - "Generated header compiles with enjin SpriteSheet constructor without modification"
  artifacts:
    - path: "tools/palettes/enjin_default.gpl"
      provides: "GIMP Palette file for enjin default (PICO-8 variant) preset"
    - path: "tools/palettes/enjin_gameboy.gpl"
      provides: "GIMP Palette file for enjin gameboy preset"
    - path: "tools/aseprite2enjin.py"
      provides: "Standalone Python conversion script"
  key_links:
    - from: "tools/palettes/*.gpl"
      to: "src/graphics/palette.cpp"
      via: "Color values must match exactly"
      pattern: "RGB values in .gpl must be decimal equivalents of hex values in palette.cpp"
    - from: "tools/aseprite2enjin.py output"
      to: "include/enjin2/graphics/sprite.hpp"
      via: "Generated array consumed by SpriteSheet constructor"
      pattern: "const uint8_t name[] = { ... }; SpriteSheet sheet(name, cellW, cellH, cols, rows);"
---

<objective>
Create Aseprite-to-enjin asset conversion tooling: palette files for artists and a Python conversion script that reads .aseprite files and outputs enjin-compatible C headers.

Purpose: Enable artists to create sprites in Aseprite using enjin's exact palette, then export directly to C header format that compiles with SpriteSheet without manual transcription.
Output: tools/palettes/*.gpl palette files, tools/aseprite2enjin.py conversion script.
</objective>

<execution_context>
@/home/unwn/.claude/get-shit-done/workflows/execute-plan.md
@/home/unwn/.claude/get-shit-done/templates/summary.md
</execution_context>

<context>
@src/graphics/palette.cpp
@include/enjin2/graphics/sprite.hpp
@include/enjin2/graphics/palette.hpp
</context>

<tasks>

<task type="auto">
  <name>Task 1: Generate Aseprite palette files from enjin presets</name>
  <files>tools/palettes/enjin_default.gpl, tools/palettes/enjin_gameboy.gpl</files>
  <action>
Create two GIMP Palette Format (.gpl) files that Aseprite can import directly.

**enjin_default.gpl** — 16 entries (15 colors + transparent marker):
Use the exact RGB values from DEFAULT_COLORS in src/graphics/palette.cpp, converted to decimal. The 16th entry (index 15) should be bright magenta (255, 0, 255) labeled "Transparent" so artists can visually distinguish it. The file format is:

```
GIMP Palette
Name: enjin-default
Columns: 16
#
 26  28  44	0 dark navy
 93  39  93	1 dark purple
177  62  83	2 dark red
239 125  87	3 orange
255 205 117	4 yellow
167 240 112	5 light green
 56 183 100	6 green
 37 113 121	7 dark teal
 41  54 111	8 dark blue
 59  93 201	9 blue
 65 166 246	10 light blue
115 239 247	11 cyan
244 244 244	12 near-white
 86 108 134	13 slate blue-grey
 51  60  87	14 dark slate
255   0 255	15 TRANSPARENT
```

**enjin_gameboy.gpl** — 16 entries (4 colors + transparent at index 15, indices 4-14 filled with black placeholders):
Use the exact RGB values from GAMEBOY_COLORS in palette.cpp. Since enjin wraps indices via `index % size`, indices 4-14 are unused; fill them with black (0, 0, 0) labeled "unused N". Index 15 is bright magenta "Transparent".

```
GIMP Palette
Name: enjin-gameboy
Columns: 16
#
 15  56  15	0 darkest green
 48  98  48	1 dark green
139 172  15	2 light green
155 188  15	3 lightest green
  0   0   0	4 unused
  0   0   0	5 unused
  0   0   0	6 unused
  0   0   0	7 unused
  0   0   0	8 unused
  0   0   0	9 unused
  0   0   0	10 unused
  0   0   0	11 unused
  0   0   0	12 unused
  0   0   0	13 unused
  0   0   0	14 unused
255   0 255	15 TRANSPARENT
```

Note: Aseprite reads .gpl files. The format is tab-separated: `R G B\tLabel`. Leading spaces in R/G/B columns are conventional for alignment but not required.
  </action>
  <verify>
Verify both files exist and have correct structure:
- Each file has "GIMP Palette" on line 1, "Name:" on line 2, "Columns:" on line 3, "#" on line 4
- enjin_default.gpl has 16 color lines after the header (indices 0-15)
- enjin_gameboy.gpl has 16 color lines after the header (indices 0-15)
- Spot-check: default index 0 = "26 28 44", default index 4 = "255 205 117", gameboy index 0 = "15 56 15"
  </verify>
  <done>Both .gpl files exist in tools/palettes/ with RGB values matching palette.cpp exactly, 16 entries each, importable by Aseprite.</done>
</task>

<task type="auto">
  <name>Task 2: Create aseprite2enjin.py conversion script</name>
  <files>tools/aseprite2enjin.py</files>
  <action>
Create a standalone Python 3 script that reads .aseprite files and outputs C header files compatible with enjin's SpriteSheet.

**Dependencies:** Only Python stdlib. Parse the Aseprite binary format directly (it is a well-documented format: https://github.com/aseprite/aseprite/blob/main/docs/ase-file-specs.md). The script needs to handle only the subset relevant to indexed-color sprites:
- File header: magic number 0xA5E0, width, height, color depth (must be 8 = indexed), frame count
- Frame header: frame size, magic 0xF1FA, chunk count
- Cel chunk (0x2005): layer index, x/y position, cel type (0=raw, 2=compressed/zlib)
- For compressed cels: decompress with zlib

**DO NOT** use any third-party library like `aseprite` or `PIL`. The format is simple enough to parse with struct.unpack + zlib.

**CLI interface:**
```
python3 tools/aseprite2enjin.py input.aseprite [--name SPRITE_NAME] [--output OUTPUT.h] [--grid WxH]
```

- `--name`: C identifier for the array (default: derived from filename, e.g., "player" from "player.aseprite")
- `--output`: Output path (default: same directory as input, .h extension)
- `--grid WxH`: Override cell size for spritesheet grid detection. If not provided, use the full canvas as a single frame if only 1 Aseprite frame exists, or use frame dimensions from the Aseprite file if multiple frames exist.

**Conversion logic:**
1. Parse .aseprite file header. Reject if color depth is not 8 (indexed).
2. For each frame, find the first visible cel chunk. Extract pixel data (decompress if zlib-compressed).
3. The cel may have an offset (x, y) within the canvas. Composite the cel onto a canvas-sized buffer initialized to index 15 (transparent). Only handle cel type 0 (raw image) and type 2 (compressed image).
4. Collect all frame buffers into one contiguous pixel array.
5. Determine grid layout:
   - If `--grid WxH` specified: cellW=W, cellH=H, cols=canvas_width/W, rows=(total_frames * canvas_height/H)... Actually, simpler: with --grid, treat the FIRST frame's canvas as a spritesheet grid (cols=canvas_width/W, rows=canvas_height/H), total frames = cols*rows, and extract cells left-to-right, top-to-bottom from that single image.
   - If no --grid and multiple Aseprite frames: cellW=canvas_width, cellH=canvas_height, cols=frame_count, rows=1
   - If no --grid and single frame: cellW=canvas_width, cellH=canvas_height, cols=1, rows=1

6. Output C header:

```c
// Generated by aseprite2enjin.py from {filename}
// Cell: {cellW}x{cellH}, Grid: {cols}x{rows}, Frames: {total}
#pragma once
#include <cstdint>

const uint8_t {name}_data[] = {
    // Frame 0
    0x00, 0x01, 0x0F, ...
    // Frame 1
    ...
};

// Usage:
// #include "enjin2/graphics/sprite.hpp"
// enjin2::SpriteSheet {name}({name}_data, {cellW}, {cellH}, {cols}, {rows});
```

Each pixel byte = the palette index from the Aseprite indexed pixel (already 0-255, but enjin only uses lower nibble 0-15). Clamp/mask to 0x0F. Emit as hex literals (0x00-0x0F), 16 values per line for readability.

**Error handling:**
- If file is not a valid .aseprite (bad magic): print error and exit 1
- If color depth is not indexed: print "Error: only indexed-color sprites are supported (found {depth}-bit)" and exit 1
- If --grid dimensions don't evenly divide canvas: print warning but proceed (truncate)

Make the script executable (add shebang: `#!/usr/bin/env python3`).
  </action>
  <verify>
    <automated>
Create a minimal test .aseprite file programmatically in Python (the format is simple enough to construct), then run the converter on it and verify the output header is valid:

```bash
cd /home/unwn/dev/enjin && python3 -c "
import struct, zlib, os, subprocess, tempfile

# Build a minimal 4x4, 1-frame, indexed-color .aseprite file
width, height = 4, 4
pixels = bytes([0,1,2,3, 4,5,6,7, 8,9,10,11, 12,13,14,15])

# Cel chunk (type 2 = compressed)
compressed = zlib.compress(pixels)
cel_data = struct.pack('<HhhBH', 0, 0, 0, 2, 0) + struct.pack('<HH', width, height) + compressed
cel_chunk = struct.pack('<IH', len(cel_data) + 6, 0x2005) + cel_data

# Frame
frame_chunks = cel_chunk
frame_data = struct.pack('<IHHIBxxx', len(frame_chunks) + 16, 0xF1FA, 1, 0, 0) + frame_chunks

# File header (128 bytes)
file_size = 128 + len(frame_data)
header = struct.pack('<IH', file_size, 0xA5E0)
header += struct.pack('<H', 1)  # frames
header += struct.pack('<HH', width, height)
header += struct.pack('<H', 8)  # color depth = indexed
header += b'\x00' * (128 - len(header))

with tempfile.NamedTemporaryFile(suffix='.aseprite', delete=False) as f:
    f.write(header + frame_data)
    tmp = f.name

result = subprocess.run(['python3', 'tools/aseprite2enjin.py', tmp, '--name', 'test_sprite', '--output', '/tmp/test_sprite.h'], capture_output=True, text=True)
os.unlink(tmp)
print('STDOUT:', result.stdout)
print('STDERR:', result.stderr)
print('RC:', result.returncode)

if result.returncode == 0:
    with open('/tmp/test_sprite.h') as f:
        content = f.read()
    print(content)
    # Verify key properties
    assert 'test_sprite_data' in content, 'Missing array name'
    assert 'pragma once' in content, 'Missing pragma'
    assert '0x00' in content, 'Missing hex values'
    assert '0x0F' in content, 'Missing transparent index'
    print('ALL CHECKS PASSED')
else:
    print('CONVERSION FAILED')
"
```
    </automated>
  </verify>
  <done>
- aseprite2enjin.py exists at tools/aseprite2enjin.py, is executable
- Parses .aseprite binary format without third-party dependencies
- Outputs valid C headers with const uint8_t arrays and SpriteSheet usage comments
- Handles: single frames, multi-frame animations, --grid for spritesheet-in-single-image
- Rejects non-indexed-color files with clear error message
  </done>
</task>

<task type="auto">
  <name>Task 3: Create usage documentation for the conversion tooling</name>
  <files>tools/README_aseprite2enjin.md</files>
  <action>
Create a concise README in tools/ documenting the conversion workflow. Include:

1. **Quick Start** — three-step workflow: (a) import palette into Aseprite, (b) draw sprite using indexed colors 0-14 with 15 as transparent, (c) run converter
2. **Palette Setup** — how to import .gpl files in Aseprite (Edit > Presets > Load Palette, or copy to Aseprite's palettes directory)
3. **Converter Usage** — CLI examples for common cases:
   - Single sprite: `python3 tools/aseprite2enjin.py player.aseprite`
   - Animation: `python3 tools/aseprite2enjin.py walk.aseprite --name player_walk`
   - Spritesheet grid: `python3 tools/aseprite2enjin.py tileset.aseprite --grid 8x8 --name tiles`
4. **Using in enjin** — C++ snippet showing #include and SpriteSheet construction
5. **Palette Reference** — table of default palette indices 0-14 with color names and hex values, note that index 15 is always transparent
6. **Limitations** — only indexed-color mode, only cel types 0 and 2, no layer blending, no tilemap chunks

Keep it under 100 lines. This is a developer/artist reference, not marketing copy.
  </action>
  <verify>File exists and contains sections for Quick Start, usage examples, and palette reference.</verify>
  <done>tools/README_aseprite2enjin.md exists with complete usage instructions for the palette files and converter script.</done>
</task>

</tasks>

<verification>
1. Both .gpl files exist and are valid GIMP Palette format with correct RGB values
2. aseprite2enjin.py runs without third-party dependencies (stdlib only)
3. Converter produces compilable C headers matching enjin SpriteSheet format
4. README documents the full workflow
</verification>

<success_criteria>
- Artist can import either palette into Aseprite and paint with enjin's exact colors
- `python3 tools/aseprite2enjin.py sprite.aseprite` produces a valid .h file
- Generated header's array + metadata works with `enjin2::SpriteSheet` constructor
- All tools are self-contained in tools/ with no external Python dependencies
</success_criteria>

<output>
After completion, create `.planning/quick/2-aseprite-to-enjin-asset-conversion-tooli/2-SUMMARY.md`
</output>
