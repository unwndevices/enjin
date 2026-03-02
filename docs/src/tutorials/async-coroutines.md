---
title: Async Coroutines
sidebar_label: Async Coroutines
---

# Async Coroutines

The coroutine system lets you write sequential async logic — timed delays, frame-precise waits, tween synchronization — as straight-line code inside a coroutine. No state machine required. All async work runs in coroutines started with `engine.async.start`.

## Starting a Coroutine

`engine.async.start(fn)` takes a function and runs it as a coroutine. It returns an integer ID, or `nil` if all 8 coroutine slots are full. Inside the coroutine, yield primitives suspend execution until a condition is met, then resume automatically.

```lua
local id = engine.async.start(function()
    -- runs immediately; yield primitives pause execution here
end)
```

Cancel a specific coroutine by ID, or cancel all at once:

```lua
engine.async.cancel(id)
engine.async.cancelAll()
```

## Waiting by Time

`engine.async.wait(seconds)` suspends the coroutine for the specified number of seconds, then resumes automatically.

```lua
engine.async.start(function()
    show_message("LEVEL UP!")
    engine.async.wait(2.0)   -- pause for 2 seconds
    hide_message()
end)
```

## Waiting by Frames

`engine.async.wait_frames(n)` suspends the coroutine for exactly `n` frames. Use this when timing must be frame-precise rather than time-based (e.g., invincibility windows, hitbox active frames).

```lua
engine.async.start(function()
    set_invincible(true)
    engine.async.wait_frames(60)   -- invincible for 60 frames
    set_invincible(false)
end)
```

## Waiting for a Tween

`engine.tween.await(id)` suspends the coroutine until the tween with the given ID completes. If the ID is invalid or the tween has already finished, it resumes immediately without yielding.

```lua
local tid = engine.tween.to(sprite, "x", 200, 0.5)  -- slide to x=200 over 0.5s
engine.async.start(function()
    engine.tween.await(tid)     -- yield until slide finishes
    trigger_explosion()         -- runs exactly when tween is done
end)
```

## Combining Primitives

All three yield primitives compose naturally inside a single coroutine, letting you chain animation, delays, and frame windows without callbacks or state flags:

```lua
engine.async.start(function()
    -- 1. animate the character in
    local tid = engine.tween.to(sprite, "alpha", 1.0, 0.3)
    engine.tween.await(tid)           -- wait for fade-in to complete

    -- 2. hold for 1 second
    engine.async.wait(1.0)

    -- 3. flash for 30 frames
    engine.async.wait_frames(30)

    -- 4. transition
    engine.state.switch("next_level")
end)
```

## Coroutine Pool

The engine maintains 8 coroutine slots. `engine.async.start` returns `nil` when the pool is full. For most scripts, 8 slots is enough. Cancel coroutines you no longer need to free slots for new ones.

```lua
local id = engine.async.start(function()
    engine.async.wait(10.0)
end)

-- later, if the sequence is no longer needed:
engine.async.cancel(id)
```
