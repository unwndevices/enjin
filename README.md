# enjin2

Lightweight C++ game engine library for embedded devices.

## Documentation

Full documentation is available at: https://unwndevices.github.io/enjin/

- [Getting Started](https://unwndevices.github.io/enjin/getting-started) - Quick setup guide
- [API Reference](https://unwndevices.github.io/enjin/api) - Complete API documentation
- [Architecture](https://unwndevices.github.io/enjin/architecture) - Design overview

## Quick Start

### Building

```bash
mkdir build && cd build
cmake ..
make
```

### Running Examples

See the `examples/` directory for sample code.

## Features

- **Scene Management** - Hierarchical scene system with transitions
- **Component System** - Entity-component architecture
- **Canvas & Graphics** - Hardware-abstracted rendering
- **Sprite System** - Sprite sheet and animation support
- **Text Rendering** - Font rendering with Adafruit GFX
- **Scripting** - Lua integration for game logic
- **Input System** - Button and touch input handling

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
