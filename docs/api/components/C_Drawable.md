---
id: C_Drawable
title: C_Drawable
sidebar_label: C_Drawable
---

# C_Drawable

Base class for all drawable components (matches original Enjin ). C_Drawableclassenjin2_1_1C__Drawablecompound


Provides common functionality for components that can be rendered, including layer management, blending, anchoring, and visibility. 

---

**Namespace:** `enjin2`

**Header:** `include/enjin2/components/drawable.hpp`

## Public Methods

### ` C_Drawable(Object *owner, uint8_t width, uint8_t height)`

Constructor (matches original Enjin). 

paramownerclassenjin2_1_1Component_1a3349b9210509e148f9f7625f6a39b022memberOwner object widthclassenjin2_1_1C__Drawable_1ac6e1a43e762d6c1ba2f9b04b981517ffmemberWidth of drawable area heightclassenjin2_1_1C__Drawable_1ae67d8735cdc3935c362d48f4973caf75memberHeight of drawable area 

---

### `virtual  ~C_Drawable()=default`

Virtual destructor. 


        

---

### `void draw(ICanvas&lt; uint8_t &gt; &canvas)=0`

Pure virtual draw method - must be implemented by derived classes. 

paramcanvasThe 8-bit canvas to draw to (matches original Enjin GFXcanvas8) 

---

### `virtual bool continueToDraw() const const`

Check if this drawable should continue to be drawn. 

returnTrue if should continue drawing, false otherwise 

---

### `void SetSortOrder(int order)`

Set the sort order for drawing priority. 

paramorderSort order value 

---

### `int GetSortOrder() const const`

Get the sort order. 

returnCurrent sort order 

---

### `void SetBlendMode(BlendMode mode)`

Set the blend mode. 

parammodeBlend mode to use 

---

### `BlendModedrawable_8hpp_1a93eabee0843e21302c246269da3374dfmember GetBlendMode() const const`

Get the blend mode. 

returnCurrent blend mode 

---

### `void SetDrawLayer(DrawLayer drawLayer)`

Set the draw layer. 

paramdrawLayerDraw layer to assign 

---

### `DrawLayerdrawable_8hpp_1abc9fd42ae01df89be2b46fc64ace9b64member GetDrawLayer() const const`

Get the draw layer. 

returnCurrent draw layer 

---

### `void SetVisibility(bool visibility)`

Set the visibility. 

paramvisibilityVisibility state 

---

### `bool GetVisibility() const const`

Get the visibility. 

returnCurrent visibility state 

---

### `bool isVisible() const const`

Check if visible. 

returntrue if visible 

---

### `void SetAnchorPoint(Anchor anchor)`

Set the anchor point for positioning. 

paramanchorclassenjin2_1_1C__Drawable_1a2b6f351b7f70a5f7270711462172b696memberAnchor point 

---

### `void AddOffset(Point offset)`

Add offset to current anchor offset. 

paramoffsetOffset to add 

---

### `void SetOffset(Point offset)`

Set the anchor offset. 

paramoffsetNew offset value 

---

### `Pointstructenjin2_1_1Pointcompound GetOffsetPosition() const const`

Get position adjusted for offset. 

returnOffset-adjusted position 

---

### `void SetXOffset(int16_t x)`

Set the X component of anchor offset. 

paramxX offset value 

---

### `void SetYOffset(int16_t y)`

Set the Y component of anchor offset. 

paramyY offset value 

---

### `uint8_t GetWidth() const const`

Get drawable width. 

returnWidth in pixels 

---

### `uint8_t GetHeight() const const`

Get drawable height. 

returnHeight in pixels 

---

### `bool shouldDrawBefore(const C_Drawable &other) const const`

Determine if this drawable should be drawn before another drawable. 

paramotherThe other drawable to compare against returnTrue if this should be drawn before other, false otherwise 

---

