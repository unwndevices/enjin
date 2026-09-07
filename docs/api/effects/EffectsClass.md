---
id: EffectsClass
title: Effects
sidebar_label: Effects
slug: EffectsClass
---

# Effects

Graphics effects for pixel manipulation. 


TPixelPixel type (e.g., Pixel4, uint8_t) 

---

**Namespace:** `enjin2`

**Header:** `include/enjin2/graphics/effects.hpp`

## Public Methods

### `static void blur(ICanvas&lt; TPixel &gt; &canvas, const Rect &rect, uint8_t radius=1)`

Apply simple blur effect (box filter). 

canvasTarget canvas rectRegion to blur radiusBlur radius in pixels (default: 1) Currently a placeholder for future implementation 

---

### `static void invert(ICanvas&lt; TPixel &gt; &canvas, const Rect &rect)`

Invert colors in a region. 

canvasTarget canvas rectRegion to invert 

---

