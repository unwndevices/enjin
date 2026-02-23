---
id: C_Drawable
title: C_Drawable
sidebar_label: C_Drawable
---

# C_Drawable

Base class for all drawable components (matches original Enjin ). C_Drawable


Provides common functionality for components that can be rendered, including layer management, blending, anchoring, and visibility. 

---

**Namespace:** `enjin2`

**Header:** `include/enjin2/components/drawable.hpp`

## Public Methods

### ` C_Drawable(Object *owner, uint8_t width, uint8_t height)`

Constructor (matches original Enjin). 

ownerOwner object widthWidth of drawable area heightHeight of drawable area 

---

### `virtual  ~C_Drawable()=default`

Virtual destructor. 


        

---

### `void draw(ICanvas&lt; uint8_t &gt; &canvas)=0`

Pure virtual draw method - must be implemented by derived classes. 

canvasThe 8-bit canvas to draw to (matches original Enjin GFXcanvas8) 

---

### `virtual bool continueToDraw() const const`

Check if this drawable should continue to be drawn. 

True if should continue drawing, false otherwise 

---

### `void SetSortOrder(int order)`

Set the sort order for drawing priority. 

orderSort order value 

---

### `int GetSortOrder() const const`

Get the sort order. 

Current sort order 

---

### `void SetBlendMode(BlendMode mode)`

Set the blend mode. 

modeBlend mode to use 

---

### `BlendMode GetBlendMode() const const`

Get the blend mode. 

Current blend mode 

---

### `void SetDrawLayer(DrawLayer drawLayer)`

Set the draw layer. 

drawLayerDraw layer to assign 

---

### `DrawLayer GetDrawLayer() const const`

Get the draw layer. 

Current draw layer 

---

### `void SetVisibility(bool visibility)`

Set the visibility. 

visibilityVisibility state 

---

### `bool GetVisibility() const const`

Get the visibility. 

Current visibility state 

---

### `bool isVisible() const const`

Check if visible. 

true if visible 

---

### `void SetAnchorPoint(Anchor anchor)`

Set the anchor point for positioning. 

anchorAnchor point 

---

### `void AddOffset(Point offset)`

Add offset to current anchor offset. 

offsetOffset to add 

---

### `void SetOffset(Point offset)`

Set the anchor offset. 

offsetNew offset value 

---

### `Point GetOffsetPosition() const const`

Get position adjusted for offset. 

Offset-adjusted position 

---

### `void SetXOffset(int16_t x)`

Set the X component of anchor offset. 

xX offset value 

---

### `void SetYOffset(int16_t y)`

Set the Y component of anchor offset. 

yY offset value 

---

### `uint8_t GetWidth() const const`

Get drawable width. 

Width in pixels 

---

### `uint8_t GetHeight() const const`

Get drawable height. 

Height in pixels 

---

### `bool shouldDrawBefore(const C_Drawable &other) const const`

Determine if this drawable should be drawn before another drawable. 

otherThe other drawable to compare against True if this should be drawn before other, false otherwise 

---

