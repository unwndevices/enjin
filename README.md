![CI](https://img.shields.io/github/actions/workflow/status/unwndevices/enjin/docs.yml?branch=main)
![Docs](https://img.shields.io/badge/docs-latest-blue)
![License](https://img.shields.io/badge/license-TBD-green)

# enjin2 - Lightweight C++ game engine for embedded devices

enjin2 is a self-contained C++ game engine library designed for resource-constrained environments. It features static allocation throughout (no dynamic memory), Lua and WASM integration for game logic scripting, and multi-platform support targeting both ESP32-S3 embedded devices and desktop platforms.

## Documentation

Full documentation is available at: https://unwndevices.github.io/enjin/

- [Getting Started](https://unwndevices.github.io/enjin/getting-started) - Quick setup guide
- [API Reference](https://unwndevices.github.io/enjin/api) - Complete API documentation
- [Architecture](https://unwndevices.github.io/enjin/architecture) - Design overview

## Installation

```bash
git clone https://github.com/unwndevices/enjin.git
cd enjin
mkdir build && cd build
cmake ..
cmake --build .
```

## Quick Start

```cpp
#include <enjin2.hpp>
using namespace enjin2;

int main() {
    Canvas8_128x64 canvas;
    canvas.fillRect(10, 10, 108, 44, 15);
    return 0;
}
```

Creates a canvas and draws a filled rectangle.

### Running Examples

See the `examples/` directory for sample code.

## Features

- **Static Allocation:** No dynamic memory usage, predictable performance
- **Lua/WASM Integration:** Script game logic, target web platforms
- **Multi-Platform:** ESP32-S3 and desktop support
- **Scene Management:** Hierarchical scene system with transitions
- **Component System:** Entity-component architecture
- **Canvas & Graphics:** Hardware-abstracted rendering
- **Sprite System:** Sprite sheet and animation support
- **Text Rendering:** Font rendering with Adafruit GFX

## Project Structure

```
include/           # Public headers
  enjin2/          # Main library namespace
src/               # Implementation files
examples/          # Example programs
docs/              # Documentation source
scripts/           # Utility scripts (API generation, etc.)
```

## License

[Add license information]
