---
id: Sprite
title: Sprite
sidebar_label: Sprite
---

# Sprite

 class for bitmap image rendering (matches original Enjin ). Spriteclassenjin2_1_1SpritecompoundSpriteclassenjin2_1_1Spritecompound


Handles rendering of bitmap images with frame animation support, blend modes, and transparency (matte) functionality. 

---

**Namespace:** enjin2

**Header:** include/enjin2/graphics/sprite.hpp

## Public Methods

### `cpp
* Sprite()*
``

Default constructor. 


        

---

### `cpp
* Sprite(const uint8_t *texture_data, uint8_t w, uint8_t h, BlendMode blend_mode=BlendMode::Normal)*
``

Construct sprite with texture data. 

paramtexture_dataPointer to texture bitmap data wWidth in pixels hHeight in pixels blend_modeBlend mode for compositing 

---

### `cpp
*void draw(ICanvas&lt; uint8_t &gt; &canvas)*
``

Draw sprite to canvas (matches original Enjin draw method). 

paramcanvasCanvas to draw to 

---

### `cpp
*void Add(ICanvas&lt; uint8_t &gt; &canvas)*
``

Add sprite data to canvas (matches original Add method). 

paramcanvasCanvas to add to 

---

### `cpp
*void Subtract(ICanvas&lt; uint8_t &gt; &canvas)*
``

Subtract sprite data from canvas (matches original Subtract method). 

paramcanvasCanvas to subtract from 

---

### `cpp
*void setTexture(const uint8_t *texture_data, uint8_t w, uint8_t h)*
``


        


        

---

### `cpp
*void setTexture(const uint8_t *texture_data, uint8_t frame_id)*
``


        


        

---

### `cpp
*void setTexture(uint8_t frame_id)*
``


        


        

---

### `cpp
*void setPosition(int16_t x, int16_t y)*
``


        


        

---

### `cpp
*void setPosition(Point pos)*
``


        


        

---

### `cpp
*void setMatte(uint8_t matte_color)*
``


        


        

---

### `cpp
*const uint8_t * GetTexture() const const*
``


        


        

---

### `cpp
*uint8_t GetWidth() const const*
``


        


        

---

### `cpp
*uint8_t GetHeight() const const*
``


        


        

---

### `cpp
*uint8_t getFrame() const const*
``


        


        

---

### `cpp
*Pointstructenjin2_1_1Pointcompound getPosition() const const*
``


        


        

---

### `cpp
*uint8_t getMatte() const const*
``


        


        

---

