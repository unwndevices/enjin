---
id: IScriptGraphics
title: IScriptGraphics
sidebar_label: IScriptGraphics
---

# IScriptGraphics

Platform-agnostic graphics interface for scripts. 


Provides drawing functions that work across different canvas types and can be implemented by platform-specific graphics backends. 

---

**Namespace:** `enjin2`

**Header:** `include/enjin2/scripting/script_interface.hpp`

## Public Methods

### `virtual  ~IScriptGraphics()=default`

Virtual destructor. 


        

---

### `uint16_t getWidth() const =0 const`

Get canvas width. 

Canvas width in pixels 

---

### `uint16_t getHeight() const =0 const`

Get canvas height. 

Canvas height in pixels 

---

### `void clear(uint8_t color)=0`

Clear canvas with specified color. 

colorClear color 

---

### `void setPixel(int16_t x, int16_t y, uint8_t color)=0`

Set pixel at coordinates. 

xX coordinate yY coordinate colorPixel color 

---

### `uint8_t getPixel(int16_t x, int16_t y) const =0 const`

Get pixel at coordinates. 

xX coordinate yY coordinate Pixel color value 

---

### `void drawLine(int16_t x1, int16_t y1, int16_t x2, int16_t y2, uint8_t color)=0`

Draw line. 

x1Start X coordinate y1Start Y coordinate x2End X coordinate y2End Y coordinate colorLine color 

---

### `void drawRect(int16_t x, int16_t y, uint16_t width, uint16_t height, uint8_t color)=0`

Draw rectangle outline. 

xX coordinate yY coordinate widthRectangle width heightRectangle height colorRectangle color 

---

### `void fillRect(int16_t x, int16_t y, uint16_t width, uint16_t height, uint8_t color)=0`

Fill rectangle. 

xX coordinate yY coordinate widthRectangle width heightRectangle height colorFill color 

---

### `void drawCircle(int16_t x, int16_t y, uint16_t radius, uint8_t color)=0`

Draw circle outline. 

xCenter X coordinate yCenter Y coordinate radiusCircle radius colorCircle color 

---

### `void fillCircle(int16_t x, int16_t y, uint16_t radius, uint8_t color)=0`

Fill circle. 

xCenter X coordinate yCenter Y coordinate radiusCircle radius colorFill color 

---

