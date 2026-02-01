---
id: PostFx
title: PostFx
sidebar_label: PostFx
---

# PostFx

Post-processing effects system. 


Provides various visual effects that can be applied to canvases including CRT simulation, noise, blur, glow, etc. Based on original Enjin  with expanded functionality. PostFxclassenjin2_1_1PostFxcompound

---

**Namespace:** enjin2

**Header:** include/enjin2/effects/postfx.hpp

## Public Methods

### `cpp
* PostFx()*
``

Constructor. 


        

---

### `cpp
*void update(uint16_t deltaTime)*
``

Update effect animations. 

paramdeltaTimeTime since last update in milliseconds 

---

### `cpp
*uint32_t getTime() const const*
``

Get current animation time for time-based effects. 

returnCurrent time in milliseconds 

---

### `cpp
*static void applyCrtScanlines(ICanvas&lt; uint8_t &gt; &canvas, const PostFxParams &params=PostFxParams(0.5f))*
``

Apply CRT scanlines effect. 

paramcanvasTarget canvas to modify paramsEffect parameters 

---

### `cpp
*static void applyMovingScanlines(ICanvas&lt; uint8_t &gt; &canvas, const PostFxParams &params, uint32_t time)*
``

Apply moving scanlines effect. 

paramcanvasTarget canvas to modify paramsEffect parameters timeCurrent time for animation 

---

### `cpp
*static void applyBarrelDistortion(ICanvas&lt; uint8_t &gt; &canvas, const PostFxParams &params=PostFxParams(0.3f))*
``

Apply barrel distortion effect. 

paramcanvasTarget canvas to modify paramsEffect parameters (intensity controls distortion strength) 

---

### `cpp
*static void applyNoise(ICanvas&lt; uint8_t &gt; &canvas, const PostFxParams &params=PostFxParams(0.3f))*
``

Apply noise overlay. 

paramcanvasTarget canvas to modify paramsEffect parameters 

---

### `cpp
*static void applyBlur(ICanvas&lt; uint8_t &gt; &canvas, const PostFxParams &params=PostFxParams(0.5f))*
``

Apply simple blur effect. 

paramcanvasTarget canvas to modify paramsEffect parameters 

---

### `cpp
*static void applyGlow(ICanvas&lt; uint8_t &gt; &canvas, const PostFxParams &params=PostFxParams(0.4f))*
``

Apply glow/bloom effect. 

paramcanvasTarget canvas to modify paramsEffect parameters 

---

### `cpp
*static void applyDither(ICanvas&lt; uint8_t &gt; &canvas, const PostFxParams &params=PostFxParams(0.5f))*
``

Apply dithering pattern. 

paramcanvasTarget canvas to modify paramsEffect parameters 

---

### `cpp
*static void applyContrast(ICanvas&lt; uint8_t &gt; &canvas, const PostFxParams &params=PostFxParams(1.2f))*
``

Apply contrast adjustment. 

paramcanvasTarget canvas to modify paramsEffect parameters (intensity = contrast multiplier) 

---

### `cpp
*static void applyBrightness(ICanvas&lt; uint8_t &gt; &canvas, const PostFxParams &params=PostFxParams(2.0f))*
``

Apply brightness adjustment. 

paramcanvasTarget canvas to modify paramsEffect parameters (intensity = brightness offset) 

---

### `cpp
*static void applyEffectChain(ICanvas&lt; uint8_t &gt; &canvas, const std::vector&lt; std::pair&lt; EffectType, PostFxParams &gt; &gt; &effects, uint32_t time=0)*
``

Apply multiple effects in sequence. 

paramcanvasTarget canvas to modify effectsVector of effect type and parameter pairs timeCurrent time for animated effects 

---

## Private Methods

### `cpp
*static uint8_t random()*
``

Simple random number generator for effects. 

returnRandom value 0-255 

---

### `cpp
*static uint8_t clamp4bit(int value)*
``

Clamp value to 4-bit range (0-15). 

paramvalueInput value returnClamped value 

---

### `cpp
*static uint8_t getDitherPattern(uint8_t x, uint8_t y)*
``

Get dither pattern value for coordinates. 

paramxX coordinate yY coordinate returnDither pattern value 

---

