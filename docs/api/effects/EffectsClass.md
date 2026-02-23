---
id: EffectsClass
title: Effects
sidebar_label: Effects
slug: EffectsClass
---

# Effects

Graphics effects for pixel manipulation. 


TPixelPixel type (e.g., , uint8_t) Pixel4

---

**Namespace:** `enjin2`

**Header:** `include/enjin2/graphics/effects.hpp`

## Public Methods

### `static void ditherPattern(ICanvas&lt; TPixel &gt; &canvas, const Rect &rect, TPixel color1, TPixel color2, uint8_t pattern=0xAA)`

Dither pattern for anti-aliasing. 

canvasTarget canvas rectRegion to dither color1First color in pattern color2Second color in pattern patternDithering pattern bitmask (default: 0xAA checkerboard) 

---

### `static void blur(ICanvas&lt; TPixel &gt; &canvas, const Rect &rect, uint8_t radius=1)`

Apply simple blur effect (box filter). 

canvasTarget canvas rectRegion to blur radiusBlur radius in pixels (default: 1) Currently a placeholder for future implementation 

---

### `static void invert(ICanvas&lt; TPixel &gt; &canvas, const Rect &rect)`

Invert colors in a region. 

canvasTarget canvas rectRegion to invert 

---

