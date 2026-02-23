---
id: C_Sprite
title: C_Sprite
sidebar_label: C_Sprite
---

# C_Sprite

Sprite component for bitmap rendering (matches original Enjin C_Sprite). 


Component wrapper around the Sprite class, providing ECS integration for bitmap image rendering with frame animation support. 

---

**Namespace:** `enjin2`

**Header:** `include/enjin2/components/sprite.hpp`

## Public Methods

### ` C_Sprite(Object *owner, uint8_t width, uint8_t height)`

Construct a new Sprite component. 

ownerThe object that owns this component widthWidth of the sprite in pixels heightHeight of the sprite in pixels 

---

### `void Load(const uint8_t *texture, uint8_t width, uint8_t height)`

Load texture data into the sprite. 

texturePointer to texture bitmap data widthWidth in pixels heightHeight in pixels 

---

### `void LoadFrame(const uint8_t *texture, uint8_t frameId)`

Load a specific frame from texture data. 

texturePointer to texture bitmap data frameIdFrame index to load 

---

### `void LoadFrame(uint8_t frameId)`

Load a specific frame (texture already set). 

frameIdFrame index to load 

---

### `virtual void draw(ICanvas&lt; uint8_t &gt; &canvas) override`

Draw the sprite to canvas (overrides C_Drawable). 

canvasThe canvas to draw to 

---

### `virtual bool continueToDraw() const override const`

Check if should continue drawing (matches original Enjin). 

True if object is not queued for removal 

---

### `virtual void lateUpdate(uint16_t deltaTime) override`

Late update method for animation (matches original Enjin). 

deltaTimeTime delta in milliseconds 

---

### `void setMatte(uint8_t matte)`

Set the matte (transparent) color. 

matteMatte color value 

---

### `Sprite & getSprite()`

Get the underlying sprite object. 

Reference to the sprite 

---

### `const Sprite & getSprite() const`

Get the underlying sprite object (const). 

Const reference to the sprite 

---

