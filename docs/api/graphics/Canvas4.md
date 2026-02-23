---
id: Canvas4
title: Canvas4
sidebar_label: Canvas4
---

# Canvas4

4-bit canvas with packed pixel storage 


WIDTHCanvas width in pixels (must be even) HEIGHTCanvas height in pixels 

---

**Namespace:** `enjin2`

**Header:** `include/enjin2/graphics/canvas.hpp`

## Public Methods

### ` Canvas4()`

Constructor initializes clear canvas. 


        

---

### `virtual uint16_t getWidth() const override const`

Get canvas width in pixels. 

Width in pixels 

---

### `virtual uint16_t getHeight() const override const`

Get canvas height in pixels. 

Height in pixels 

---

### `virtual void setPixel(int16_t x, int16_t y, Pixel4 color) override`

Set pixel color at specified coordinates. 

xX coordinate yY coordinate colorPixel color to set 

---

### `virtual Pixel4 getPixel(int16_t x, int16_t y) const override const`

Get pixel color at specified coordinates. 

xX coordinate yY coordinate Pixel color at the specified location 

---

### `virtual void clear(Pixel4 color=Pixel4(0)) override`

Clear entire canvas to specified color. 

colorColor to fill canvas with (default: black/zero) 

---

### `void drawHLine(int16_t x, int16_t y, int16_t width, Pixel4 color)`

Optimized horizontal line drawing with batch operations. 

xStarting x coordinate yY coordinate widthLine width in pixels colorLine color 

---

### `void drawVLine(int16_t x, int16_t y, int16_t height, Pixel4 color)`

Optimized vertical line drawing. 

xX coordinate yStarting y coordinate heightLine height in pixels colorLine color 

---

### `virtual void fillRect(int16_t x, int16_t y, int16_t width, int16_t height, Pixel4 color)`

Optimized rectangle filling with batch operations. 

xStarting x coordinate yStarting y coordinate widthRectangle width heightRectangle height colorFill color 

---

### `void setPixelBatch(int16_t x, int16_t y, const Pixel4 *pixels, int16_t count)`

Batch pixel setting for arrays of data. 

xStarting x coordinate yY coordinate pixelsArray of pixel values countNumber of pixels to set 

---

### `virtual void fill(const Rect &rect, Pixel4 color) override`

Fill rectangular region with specified color. 

rectRectangle to fill colorColor to fill with 

---

### `const  *PackedPixel4 getBuffer() const const`

Get read-only pointer to pixel buffer. 

Pointer to packed pixel data 

---

### ` *PackedPixel4 getBuffer()`

Get mutable pointer to pixel buffer. 

Pointer to packed pixel data 

---

### `size_t getBufferSize() const const`

Get buffer size in bytes. 

Buffer size 

---

### `void copyFrom(const Canvas4 &other, int16_t dst_x=0, int16_t dst_y=0)`

Copy pixels from another canvas. 

otherSource canvas to copy from dst_xDestination X offset dst_yDestination Y offset 

---

### `void blit(const Canvas4 &sprite, int16_t x, int16_t y, Pixel4 transparent=Pixel4(0))`

Blit sprite with transparency. 

spriteSource canvas to blit xDestination X coordinate yDestination Y coordinate transparentColor to treat as transparent 

---

## Private Methods

### `size_t getIndex(int16_t x, int16_t y) const const`


        


        

---

### `bool isLowPixel(int16_t x) const const`


        


        

---

