---
id: PostFx
title: PostFx
sidebar_label: PostFx
---

# PostFx

Post-processing effects system. 


Provides various visual effects that can be applied to canvases including CRT simulation, noise, blur, glow, etc. Based on original Enjin  with expanded functionality. PostFxclassenjin2_1_1PostFxcompound

---

**Namespace:** `enjin2`

**Header:** `include/enjin2/effects/postfx.hpp`

## Public Methods

### ` PostFx()`

Constructor. 


        

---

### `void update(uint16_t deltaTime)`

Update effect animations. 

paramdeltaTimeTime since last update in milliseconds 

---

### `uint32_t getTime() const const`

Get current animation time for time-based effects. 

returnCurrent time in milliseconds 

---

### `static void applyCrtScanlines(ICanvas< uint8_t > &canvas, const PostFxParams &params=PostFxParams(0.5f))`

Apply CRT scanlines effect. 

paramcanvasTarget canvas to modify paramsEffect parameters 

---

### `static void applyMovingScanlines(ICanvas< uint8_t > &canvas, const PostFxParams &params, uint32_t time)`

Apply moving scanlines effect. 

paramcanvasTarget canvas to modify paramsEffect parameters timeCurrent time for animation 

---

### `static void applyBarrelDistortion(ICanvas< uint8_t > &canvas, const PostFxParams &params=PostFxParams(0.3f))`

Apply barrel distortion effect. 

paramcanvasTarget canvas to modify paramsEffect parameters (intensity controls distortion strength) 

---

### `static void applyNoise(ICanvas< uint8_t > &canvas, const PostFxParams &params=PostFxParams(0.3f))`

Apply noise overlay. 

paramcanvasTarget canvas to modify paramsEffect parameters 

---

### `static void applyBlur(ICanvas< uint8_t > &canvas, const PostFxParams &params=PostFxParams(0.5f))`

Apply simple blur effect. 

paramcanvasTarget canvas to modify paramsEffect parameters 

---

### `static void applyGlow(ICanvas< uint8_t > &canvas, const PostFxParams &params=PostFxParams(0.4f))`

Apply glow/bloom effect. 

paramcanvasTarget canvas to modify paramsEffect parameters 

---

### `static void applyDither(ICanvas< uint8_t > &canvas, const PostFxParams &params=PostFxParams(0.5f))`

Apply dithering pattern. 

paramcanvasTarget canvas to modify paramsEffect parameters 

---

### `static void applyContrast(ICanvas< uint8_t > &canvas, const PostFxParams &params=PostFxParams(1.2f))`

Apply contrast adjustment. 

paramcanvasTarget canvas to modify paramsEffect parameters (intensity = contrast multiplier) 

---

### `static void applyBrightness(ICanvas< uint8_t > &canvas, const PostFxParams &params=PostFxParams(2.0f))`

Apply brightness adjustment. 

paramcanvasTarget canvas to modify paramsEffect parameters (intensity = brightness offset) 

---

### `static void applyEffectChain(ICanvas< uint8_t > &canvas, const std::vector< std::pair< EffectType, PostFxParams > > &effects, uint32_t time=0)`

Apply multiple effects in sequence. 

paramcanvasTarget canvas to modify effectsVector of effect type and parameter pairs timeCurrent time for animated effects 

---

## Private Methods

### `static uint8_t random()`

Simple random number generator for effects. 

returnRandom value 0-255 

---

### `static uint8_t clamp4bit(int value)`

Clamp value to 4-bit range (0-15). 

paramvalueInput value returnClamped value 

---

### `static uint8_t getDitherPattern(uint8_t x, uint8_t y)`

Get dither pattern value for coordinates. 

paramxX coordinate yY coordinate returnDither pattern value 

---

