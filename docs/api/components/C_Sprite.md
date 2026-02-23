---
id: C_Sprite
title: C_Sprite
sidebar_label: C_Sprite
---

# C_Sprite

 component for bitmap rendering (matches original Enjin ). Spriteclassenjin2_1_1SpritecompoundC_Spriteclassenjin2_1_1C__Spritecompound


 wrapper around the  class, providing ECS integration for bitmap image rendering with frame animation support. Componentclassenjin2_1_1ComponentcompoundSpriteclassenjin2_1_1Spritecompound

---

**Namespace:** `enjin2`

**Header:** `include/enjin2/components/sprite.hpp`

## Public Methods

### ` C_Sprite(Object *owner, uint8_t width, uint8_t height)`

Construct a new  component. Spriteclassenjin2_1_1Spritecompound

paramownerclassenjin2_1_1Component_1a3349b9210509e148f9f7625f6a39b022memberThe object that owns this component widthclassenjin2_1_1C__Drawable_1ac6e1a43e762d6c1ba2f9b04b981517ffmemberWidth of the sprite in pixels heightclassenjin2_1_1C__Drawable_1ae67d8735cdc3935c362d48f4973caf75memberHeight of the sprite in pixels 

---

### `void Load(const uint8_t *texture, uint8_t width, uint8_t height)`

Load texture data into the sprite. 

paramtexturePointer to texture bitmap data widthclassenjin2_1_1C__Drawable_1ac6e1a43e762d6c1ba2f9b04b981517ffmemberWidth in pixels heightclassenjin2_1_1C__Drawable_1ae67d8735cdc3935c362d48f4973caf75memberHeight in pixels 

---

### `void LoadFrame(const uint8_t *texture, uint8_t frameId)`

Load a specific frame from texture data. 

paramtexturePointer to texture bitmap data frameIdFrame index to load 

---

### `void LoadFrame(uint8_t frameId)`

Load a specific frame (texture already set). 

paramframeIdFrame index to load 

---

### `virtual void draw(ICanvas&lt; uint8_t &gt; &canvas) override`

Draw the sprite to canvas (overrides ). C_Drawableclassenjin2_1_1C__Drawablecompound

paramcanvasThe canvas to draw to 

---

### `virtual bool continueToDraw() const override const`

Check if should continue drawing (matches original Enjin). 

returnTrue if object is not queued for removal 

---

### `virtual void lateUpdate(uint16_t deltaTime) override`

Late update method for animation (matches original Enjin). 

paramdeltaTimeTime delta in milliseconds 

---

### `void setMatte(uint8_t matte)`

Set the matte (transparent) color. 

parammatteMatte color value 

---

### ` &Spriteclassenjin2_1_1Spritecompound getSprite()`

Get the underlying sprite object. 

returnReference to the sprite 

---

### `const  &Spriteclassenjin2_1_1Spritecompound getSprite() const const`

Get the underlying sprite object (const). 

returnConst reference to the sprite 

---

