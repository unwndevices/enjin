---
id: Canvas4
title: Canvas4
sidebar_label: Canvas4
---

# Canvas4


    



    

---

**Namespace:** enjin2

**Header:** include/enjin2/graphics/canvas.hpp

## Public Methods

### `cpp
* Canvas4()*
``


        


        

---

### `cpp
*virtual uint16_t getWidth() const override const*
``

Get canvas width in pixels. 

returnWidth in pixels 

---

### `cpp
*virtual uint16_t getHeight() const override const*
``

Get canvas height in pixels. 

returnHeight in pixels 

---

### `cpp
*virtual void setPixel(int16_t x, int16_t y, Pixel4 color) override*
``

Set pixel color at specified coordinates. 

paramxX coordinate yY coordinate colorPixel color to set 

---

### `cpp
*virtual Pixel4structenjin2_1_1Pixel4compound getPixel(int16_t x, int16_t y) const override const*
``

Get pixel color at specified coordinates. 

paramxX coordinate yY coordinate returnPixel color at the specified location 

---

### `cpp
*virtual void clear(Pixel4 color=Pixel4(0)) override*
``

Clear entire canvas to specified color. 

paramcolorColor to fill canvas with (default: black/zero) 

---

### `cpp
*void drawHLine(int16_t x, int16_t y, int16_t width, Pixel4 color)*
``

Optimized horizontal line drawing with batch operations. 

paramxStarting x coordinate yY coordinate widthLine width in pixels colorLine color 

---

### `cpp
*void drawVLine(int16_t x, int16_t y, int16_t height, Pixel4 color)*
``

Optimized vertical line drawing. 

paramxX coordinate yStarting y coordinate heightLine height in pixels colorLine color 

---

### `cpp
*virtual void fillRect(int16_t x, int16_t y, int16_t width, int16_t height, Pixel4 color)*
``

Optimized rectangle filling with batch operations. 

paramxStarting x coordinate yStarting y coordinate widthRectangle width heightRectangle height colorFill color 

---

### `cpp
*void setPixelBatch(int16_t x, int16_t y, const Pixel4 *pixels, int16_t count)*
``

Batch pixel setting for arrays of data. 

paramxStarting x coordinate yY coordinate pixelsArray of pixel values countNumber of pixels to set 

---

### `cpp
*virtual void fill(const Rect &rect, Pixel4 color) override*
``

Fill rectangular region with specified color. 

paramrectRectangle to fill colorColor to fill with 

---

### `cpp
*const  *PackedPixel4classenjin2_1_1PackedPixel4compound getBuffer() const const*
``


        


        

---

### `cpp
* *PackedPixel4classenjin2_1_1PackedPixel4compound getBuffer()*
``


        


        

---

### `cpp
*size_t getBufferSize() const const*
``


        


        

---

### `cpp
*void copyFrom(const Canvas4 &other, int16_t dst_x=0, int16_t dst_y=0)*
``


        


        

---

### `cpp
*void blit(const Canvas4 &sprite, int16_t x, int16_t y, Pixel4 transparent=Pixel4(0))*
``


        


        

---

## Private Methods

### `cpp
*size_t getIndex(int16_t x, int16_t y) const const*
``


        


        

---

### `cpp
*bool isLowPixel(int16_t x) const const*
``


        


        

---

