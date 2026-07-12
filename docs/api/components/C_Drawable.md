---
id: C_Drawable
title: C_Drawable
sidebar_label: C_Drawable
---

# C_Drawable

Base class for all drawable components (matches original Enjin C_Drawable). 


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

### `void draw(ICanvas&lt; Pixel4 &gt; &canvas)=0`

Pure virtual draw method - must be implemented by derived classes. 

canvasThe 4-bit canvas targeting ICanvas&lt;Pixel4&gt;

---

### `virtual bool continueToDraw() const`

Check if this drawable should continue to be drawn. 

True if should continue drawing, false otherwise 

---

### `void SetBufferIndex(uint8_t idx)`

Set the layer buffer index (0 = background, N-1 = foreground). 

idxBuffer index value 

---

### `uint8_t GetBufferIndex() const`

Get the layer buffer index. 

Current buffer index 

---

### `void SetBlendMode(BlendMode mode)`

Set the blend mode. 

modeBlend mode to use 

---

### `BlendMode GetBlendMode() const`

Get the blend mode. 

Current blend mode 

---

### `void SetVisibility(bool visibility)`

Set the visibility. 

visibilityVisibility state 

---

### `bool GetVisibility() const`

Get the visibility. 

Current visibility state 

---

### `bool isVisible() const`

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

### `Point GetOffsetPosition() const`

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

### `uint8_t GetWidth() const`

Get drawable width. 

Width in pixels 

---

### `uint8_t GetHeight() const`

Get drawable height. 

Height in pixels 

---

### `bool shouldDrawBefore(const C_Drawable &other) const`

Determine if this drawable should be drawn before another drawable. 

otherThe other drawable to compare against True if this should be drawn before other, false otherwise 

---

### `void setScreenSpace(bool ss)`

Set screen-space mode. 

Screen-space drawables (e.g. HUD, UI overlays) skip the camera offset applied by Scene::renderObjects() and always render at their world position.ssTrue to enable screen-space mode, false for world-space (default) 

---

### `bool isScreenSpace() const`

Check if this drawable is in screen-space mode. 

True if screen-space (ignores camera offset) 

---

### `virtual void drawWithOffset(ICanvas&lt; Pixel4 &gt; &canvas, Point offset)`

Draw with camera offset applied. 

Called by Scene::renderObjects() instead of draw() when a camera is active. Screen-space drawables (m_screenSpace==true) call draw() without any offset. World-space drawables temporarily shift anchor_offset by the camera offset, call draw(), then restore anchor_offset.Sign convention: offset = camera.getScreenOffset() = -(camera_pos + shake). Adding offset to anchor_offset moves the drawable left/up by camera_pos, producing the correct screen position: world_pos - camera_pos.canvasTarget 4-bit canvas offsetCamera screen offset (negative of camera world position) 

---

