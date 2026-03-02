---
title: Your First Script
sidebar_label: Your First Script
---

# Your First Script

Enjin2 scripts are Lua files loaded by the SDL3 runner. The runner calls two globals every frame: `update(dt)` for logic and `draw()` for rendering. Everything else — state, input, graphics, time — is accessed through the `engine.*` API table.

## Script Structure

Every script must define `update` and `draw`. The runner calls them in that order each frame.

```lua
function update(dt)
    -- dt: delta time in seconds, clamped to 0.05 max
    -- put all game logic here
end

function draw()
    -- no arguments; runs after update each frame
    -- all draw calls go here
end
```

`tamagotchi.lua` is a complete working example that covers every concept below. Run it with:

```bash
./build/sdl3/enjin2_sdl --script scripts/tamagotchi.lua
```

## Reading Resolution

`engine.config.resolution()` returns the canvas width and height as integers. Call it once at the top of your script to use as layout constants.

```lua
-- tamagotchi.lua line 5
local W, H = engine.config.resolution()
```

## State Management

`engine.state` is a built-in named-state FSM. `engine.state.switch("name")` transitions to a new state immediately. `engine.state.current()` returns the current state name as a string, which you use to branch logic in `update` and `draw`.

```lua
-- tamagotchi.lua lines 28–36: reset() transitions back to "alive"
local function reset()
    stats.hunger = 0
    stats.happiness = 100
    stats.energy = 100
    show_msg("HELLO!", 2.0)
    engine.state.switch("alive")
end

engine.state.switch("alive")  -- set initial state at script load
```

Reading the current state in `update`:

```lua
-- tamagotchi.lua lines 49–56
local cur = engine.state.current()

if cur == "dead" then
    if engine.input.just_pressed(BTN.START) or engine.input.just_pressed(BTN.A) then
        reset()
    end
    return
end
```

## Handling Input

`engine.input.just_pressed(BTN.X)` returns `true` only on the first frame a button goes down — it does not repeat while held. The `BTN` table provides: `UP`, `DOWN`, `LEFT`, `RIGHT`, `A`, `B`, `START`.

```lua
-- tamagotchi.lua lines 78–87: feed on LEFT
if engine.input.just_pressed(BTN.LEFT) then
    if stats.hunger > 0 then
        stats.hunger = clamp(stats.hunger - 20, 0, 100)
        stats.energy = clamp(stats.energy + 5, 0, 100)
        show_msg("YUM!", 1.0)
    else
        show_msg("FULL!", 1.0)
    end
end

-- tamagotchi.lua line 100: sleep on A
if engine.input.just_pressed(BTN.A) then
    show_msg("NIGHT NIGHT", 1.0)
    engine.state.switch("sleeping")
end
```

## Drawing

`draw()` runs after every `update` call. Start each frame with `clear()` to erase the previous frame, then set a color with `setColor()` before each draw command. The bare globals (`clear`, `setColor`, `text`, `textCentered`, `rectangle`, `circle`, `line`, `setPixel`) and the `engine.graphics.*` equivalents are both valid — the SDL3 runner exposes both forms.

```lua
-- tamagotchi.lua lines 130–136
function draw()
    clear(COLOR.BLACK)
    local cur = engine.state.current()

    setColor(COLOR.WHITE)
    textCentered("TAMAGOTCHI", 10, 2)

    setColor(COLOR.GRAY)
    text("HUNGER:", 10, 34)
```

Available color constants: `COLOR.BLACK`, `COLOR.WHITE`, `COLOR.GRAY`, `COLOR.DARK_GRAY`, `COLOR.RED`, `COLOR.GREEN`, `COLOR.BLUE`, `COLOR.PINK`, `COLOR.YELLOW`.

## Time

`engine.time.now()` returns the total seconds elapsed since the script started as a float. Combine it with `math.floor` and modulo for blinking effects.

```lua
-- tamagotchi.lua line 177: blink a "Z" every half second during sleep
if math.floor(engine.time.now() * 2) % 2 == 0 then
    text("Z", petX + 35, petY - 30)
end
```

## Running Your Script

```bash
./build/sdl3/enjin2_sdl --script path/to/script.lua
```

An optional `--fps N` flag caps the frame rate (default is uncapped).

## Next Steps

[Async Coroutines](./async-coroutines.md) — write non-blocking timed sequences using `engine.async.wait`, `engine.async.wait_frames`, and `engine.tween.await` without managing frame counters manually.
