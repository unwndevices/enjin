---
id: ICanvas
title: ICanvas
sidebar_label: ICanvas
---

# ICanvas

Abstract canvas interface for drawing operations. 



Provides a hardware-independent interface for all drawing operations. Both enjin1 and enjin2 can implement this interface for compile-time polymorphism.templateparamTPixelPixel type (e.g., , uint8_t)Pixel4structenjin2_1_1Pixel4compound
Provides a hardware-independent interface for all drawing operations. Concrete implementations handle the actual pixel storage and formatting. templateparamTPixelPixel type (e.g., , uint8_t)Pixel4structenjin2_1_1Pixel4compound

---

**Namespace:** `enjin2`

**Header:** `include/enjin2/abstract/icanvas.hpp`

## Public Methods

### ``virtual  ~ICanvas()=default``

Virtual destructor for proper cleanup through base pointer. 


        

---

### ``uint16_t getWidth() const =0 const``

Get canvas width in pixels. 

returnWidth in pixels 

---

### ``uint16_t getHeight() const =0 const``

Get canvas height in pixels. 

returnHeight in pixels 

---

### ``void setPixel(int16_t x, int16_t y, TPixel color)=0``

Set pixel color at specified coordinates. 

paramxX coordinate yY coordinate colorPixel color to set 

---

### ``TPixel getPixel(int16_t x, int16_t y) const =0 const``

Get pixel color at specified coordinates. 

paramxX coordinate yY coordinate returnPixel color at the specified location 

---

### ``void clear(TPixel color=TPixel(0))=0``

Clear entire canvas to specified color. 

paramcolorColor to fill canvas with (default: black/zero) 

---

### ``void fill(const Rect &rect, TPixel color)=0``

Fill rectangular region with specified color. 

paramrectRectangle to fill colorColor to fill with 

---

### ``void drawText(const char *text, int16_t x, int16_t y, TPixel color)=0``

Draw text at specified position. 

paramtextText to draw xX coordinate yY coordinate colorText color 

---

### ``void setTextColor(TPixel color)=0``

Set text color for subsequent text operations. 

paramcolorText color 

---

### ``void setTextSize(uint8_t size)=0``

Set text size for subsequent text operations. 

paramsizeText scaling factor 

---

### ``void drawLine(int16_t x0, int16_t y0, int16_t x1, int16_t y1, TPixel color)=0``

Draw a line from (x0, y0) to (x1, y1). 

paramx0Start X coordinate y0Start Y coordinate x1End X coordinate y1End Y coordinate colorLine color 

---

### ``void drawRect(int16_t x, int16_t y, int16_t width, int16_t height, TPixel color)=0``

Draw rectangle outline. 

paramxTop-left X coordinate yTop-left Y coordinate widthRectangle width heightRectangle height colorLine color 

---

### ``void fillRect(int16_t x, int16_t y, int16_t width, int16_t height, TPixel color)=0``

Draw filled rectangle. 

paramxTop-left X coordinate yTop-left Y coordinate widthRectangle width heightRectangle height colorFill color 

---

### ``void drawCircle(int16_t x, int16_t y, int16_t radius, TPixel color)=0``

Draw circle outline. 

paramxCenter X coordinate yCenter Y coordinate radiusCircle radius colorLine color 

---

### ``void fillCircle(int16_t x, int16_t y, int16_t radius, TPixel color)=0``

Draw filled circle. 

paramxCenter X coordinate yCenter Y coordinate radiusCircle radius colorFill color 

---

### ``void drawBitmap(const uint8_t *bitmap, int16_t x, int16_t y, int16_t width, int16_t height, TPixel color)=0``

Draw bitmap image at specified position. 

parambitmapPointer to bitmap data xDestination X coordinate yDestination Y coordinate widthImage width heightImage height colorImage color 

---

### ``void drawBitmap(const uint8_t *bitmap, uint8_t matte, int16_t x, int16_t y, int16_t width, int16_t height)=0``

Draw bitmap with transparency (skip pixels matching matte color). 

parambitmapPointer to bitmap data matteMatte color (pixels matching this are skipped) xDestination X coordinate yDestination Y coordinate widthImage width heightImage height 

---

### ``virtual  ~ICanvas()=default``


        


        

---

### ``uint16_t getWidth() const =0 const``

Get canvas width in pixels. 

returnWidth in pixels 

---

### ``uint16_t getHeight() const =0 const``

Get canvas height in pixels. 

returnHeight in pixels 

---

### ``void setPixel(int16_t x, int16_t y, TPixel color)=0``

Set pixel color at specified coordinates. 

paramxX coordinate yY coordinate colorPixel color to set 

---

### ``TPixel getPixel(int16_t x, int16_t y) const =0 const``

Get pixel color at specified coordinates. 

paramxX coordinate yY coordinate returnPixel color at the specified location 

---

### ``void clear(TPixel color=TPixel(0))=0``

Clear entire canvas to specified color. 

paramcolorColor to fill canvas with (default: black) 

---

### ``void fill(const Rect &rect, TPixel color)=0``

Fill rectangular region with specified color. 

paramrectRectangle to fill colorColor to fill with 

---

### ``bool inBounds(int16_t x, int16_t y) const const``

Check if coordinates are within canvas bounds. 

paramxX coordinate to check yY coordinate to check returntrue if coordinates are valid, false otherwise 

---

### ``Rectstructenjin2_1_1Rectcompound getBounds() const const``

Get canvas bounds as rectangle. 

returnRectangle representing entire canvas area 

---

