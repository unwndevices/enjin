---
title: Getting Started
sidebar_label: Getting Started
---

# Getting Started

Get enjin2 up and running in three simple steps.

## 1. Clone Repository

```bash
git clone https://github.com/unwndevices/enjin.git
cd enjin
```

## 2. Configure Build

```bash
mkdir build && cd build
cmake ..
```

## 3. Build

```bash
cmake --build .
```

## Quick Example

Run the SDL3 binary with a Lua script:

```bash
./build/sdl3/enjin2_sdl --script scripts/tamagotchi.lua
```

Every Lua script defines two globals the engine calls each frame:

```lua
function update(dt)
    -- dt: delta time in seconds (clamped to 0.05)
    if engine.input.just_pressed(BTN.A) then
        engine.state.switch("playing")
    end
end

function draw()
    clear(COLOR.BLACK)
    setColor(COLOR.WHITE)
    textCentered("Hello, enjin2!", 32)
end
```

## Next Steps

[Components](./components.md) - Learn component system.

[Canvas](./canvas.md) - Graphics operations guide.

[Scene Management](./scene-management.md) - Scene system overview.

*Note: API Reference documentation will be available in the next phase.*
