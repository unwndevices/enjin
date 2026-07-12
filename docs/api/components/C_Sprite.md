---
id: C_Sprite
title: C_Sprite
sidebar_label: C_Sprite
---

# C_Sprite

Sprite component with SpriteSheet and frame animation. 


Wraps a SpriteSheet and drives frame animation automatically via lateUpdate(). Supports Once, Loop, and PingPong animation modes.Transparency: palette index 15 is always skipped at blit time (compile-time constant). 

---

**Namespace:** `enjin2`

**Header:** `include/enjin2/components/sprite.hpp`

## Public Methods

### ` C_Sprite(Object *owner, uint8_t width, uint8_t height)`

ownerThe object that owns this component widthDrawable width (passed to C_Drawable for sort/anchor math) heightDrawable height 

---

### `void setSheet(const SpriteSheet &sheet)`

Replace the sprite sheet. Resets frame to 0 and animation state. sheetNew sprite sheet to use 

---

### `void setFPS(float fps)`

Set frames-per-second playback rate. Must be &gt; 0. fpsPlayback rate 

---

### `void setMode(AnimMode mode)`

Set animation loop mode. modeLoop mode 

---

### `void setFrame(uint8_t index)`

Directly set the current frame. Clamped to valid range [0, frameCount-1]. indexFrame index to set 

---

### `uint8_t getFrame() const`

Get the current frame index. Current frame index 

---

### `bool isDone() const`

True when Once mode animation has completed (frozen on last frame). true if animation is done 

---

### `virtual void draw(ICanvas&lt; Pixel4 &gt; &canvas) override`

Draw the current frame to canvas at the component's position. 

Uses GetOffsetPosition() from C_Drawable for position plumbing. Skips draw if not visible or sheet has no data. canvasTarget 4-bit canvas to draw on 

---

### `virtual void lateUpdate(float dt) override`

Advance animation by dt seconds. 

Uses delta-time accumulator: accumulator += dt, advance when &gt;= frame_duration. Subtracts frame duration rather than zeroing to preserve carry-over. dtDelta-time in seconds since last frame 

---

### `virtual bool continueToDraw() const override const`

Check if this drawable should continue to be drawn. 

True if should continue drawing, false otherwise 

---

## Private Methods

### `void advanceFrame()`

---

