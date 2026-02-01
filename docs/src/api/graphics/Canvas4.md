---
id: Canvas4
title: Canvas4
sidebar_label: Canvas4
---

# Canvas4


    



    

---

**Namespace:** `enjin2`

**Header:** `include/enjin2/graphics/canvas.hpp`

## Public Methods

### `` Canvas4()``


        


        

---

### ``virtual uint16_t getWidth() const override const``

Get canvas width in pixels. 

returnWidth in pixels 

---

### ``virtual uint16_t getHeight() const override const``

Get canvas height in pixels. 

returnHeight in pixels 

---

### ``virtual void setPixel(int16_t x, int16_t y, Pixel4 color) override``

Set pixel color at specified coordinates. 

paramxX coordinate yY coordinate colorPixel color to set 

---

### ``virtual Pixel4structenjin2_1_1Pixel4compound getPixel(int16_t x, int16_t y) const override const``

Get pixel color at specified coordinates. 

paramxX coordinate yY coordinate returnPixel color at the specified location 

---

### ``virtual void clear(Pixel4 color=Pixel4(0)) override``

Clear entire canvas to specified color. 

paramcolorColor to fill canvas with (default: black/zero) 

---

### ``void drawHLine(int16_t x, int16_t y, int16_t width, Pixel4 color)``

Optimized horizontal line drawing with batch operations. 

paramxStarting x coordinate yY coordinate widthLine width in pixels colorLine color 

---

### ``void drawVLine(int16_t x, int16_t y, int16_t height, Pixel4 color)``

Optimized vertical line drawing. 

paramxX coordinate yStarting y coordinate heightLine height in pixels colorLine color 

---

### ``virtual void fillRect(int16_t x, int16_t y, int16_t width, int16_t height, Pixel4 color)``

Optimized rectangle filling with batch operations. 

paramxStarting x coordinate yStarting y coordinate widthRectangle width heightRectangle height colorFill color 

---

### ``void setPixelBatch(int16_t x, int16_t y, const Pixel4 *pixels, int16_t count)``

Batch pixel setting for arrays of data. 

paramxStarting x coordinate yY coordinate pixelsArray of pixel values countNumber of pixels to set 

---

### ``virtual void fill(const Rect &rect, Pixel4 color) override``

Fill rectangular region with specified color. 

paramrectRectangle to fill colorColor to fill with 

---

### ``const  *PackedPixel4classenjin2_1_1PackedPixel4compound getBuffer() const const``


        


        

---

### `` *PackedPixel4classenjin2_1_1PackedPixel4compound getBuffer()``


        


        

---

### ``size_t getBufferSize() const const``


        


        

---

### ``void copyFrom(const Canvas4 &other, int16_t dst_x=0, int16_t dst_y=0)``


        


        

---

### ``void blit(const Canvas4 &sprite, int16_t x, int16_t y, Pixel4 transparent=Pixel4(0))``


        


        

---

## Private Methods

### ``size_t getIndex(int16_t x, int16_t y) const const``


        


        

---

### ``bool isLowPixel(int16_t x) const const``


        


        

---

