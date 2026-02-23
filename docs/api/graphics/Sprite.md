---
id: Sprite
title: Sprite
sidebar_label: Sprite
---

# Sprite

 class for bitmap image rendering (matches original Enjin ). SpriteSprite


Handles rendering of bitmap images with frame animation support, blend modes, and transparency (matte) functionality. 

---

**Namespace:** `enjin2`

**Header:** `include/enjin2/graphics/sprite.hpp`

## Public Methods

### ` Sprite()`

Default constructor. 


        

---

### ` Sprite(const uint8_t *texture_data, uint8_t w, uint8_t h, BlendMode blend_mode=BlendMode::Normal)`

Construct sprite with texture data. 

texture_dataPointer to texture bitmap data wWidth in pixels hHeight in pixels blend_modeBlend mode for compositing 

---

### `void draw(ICanvas&lt; uint8_t &gt; &canvas)`

Draw sprite to canvas (matches original Enjin draw method). 

canvasCanvas to draw to 

---

### `void Add(ICanvas&lt; uint8_t &gt; &canvas)`

Add sprite data to canvas (matches original Add method). 

canvasCanvas to add to 

---

### `void Subtract(ICanvas&lt; uint8_t &gt; &canvas)`

Subtract sprite data from canvas (matches original Subtract method). 

canvasCanvas to subtract from 

---

### `void setTexture(const uint8_t *texture_data, uint8_t w, uint8_t h)`

Set texture data with dimensions. 

texture_dataPointer to texture bitmap data wWidth in pixels hHeight in pixels 

---

### `void setTexture(const uint8_t *texture_data, uint8_t frame_id)`

Set texture data and frame. 

texture_dataPointer to texture bitmap data frame_idFrame index to display 

---

### `void setTexture(uint8_t frame_id)`

Set current animation frame. 

frame_idFrame index to display 

---

### `void setPosition(int16_t x, int16_t y)`

Set sprite position. 

xX coordinate yY coordinate 

---

### `void setPosition(Point pos)`

Set sprite position from point. 

posNew position 

---

### `void setMatte(uint8_t matte_color)`

Set matte (transparent) color. 

matte_colorColor value to treat as transparent 

---

### `const uint8_t * GetTexture() const const`

Get pointer to current frame texture data. 

Pointer to texture data, or nullptr if no texture set 

---

### `uint8_t GetWidth() const const`

Get sprite width. 

Width in pixels 

---

### `uint8_t GetHeight() const const`

Get sprite height. 

Height in pixels 

---

### `uint8_t getFrame() const const`

Get current frame index. 

Frame index 

---

### `Point getPosition() const const`

Get sprite position. 

Current position 

---

### `uint8_t getMatte() const const`

Get matte (transparent) color. 

Matte color value 

---

