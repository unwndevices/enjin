---
id: CanvasGraphicsAdapter
title: CanvasGraphicsAdapter
sidebar_label: CanvasGraphicsAdapter
---

# CanvasGraphicsAdapter

Graphics adapter for enjin2 canvases. 


Implements  interface using enjin2 canvas types. Allows scripts to draw on any enjin2 canvas through the common interface. IScriptGraphics

---

**Namespace:** `enjin2`

**Header:** `include/enjin2/scripting/lua_interpreter.hpp`

## Public Methods

### ` CanvasGraphicsAdapter(CanvasType *targetCanvas)`

Constructor. 

targetCanvasCanvas to draw on 

---

### `void setCanvas(CanvasType *targetCanvas)`

Set target canvas. 

targetCanvasNew canvas to draw on 

---

### `void setColor(uint8_t color)`

Set current drawing color. 

colorColor to use for drawing 

---

### `virtual uint16_t getWidth() const override const`

Get canvas width. 

Canvas width in pixels 

---

### `virtual uint16_t getHeight() const override const`

Get canvas height. 

Canvas height in pixels 

---

### `virtual void clear(uint8_t color) override`

Clear canvas with specified color. 

colorClear color 

---

### `virtual void setPixel(int16_t x, int16_t y, uint8_t color) override`

Set pixel at coordinates. 

xX coordinate yY coordinate colorPixel color 

---

### `virtual uint8_t getPixel(int16_t x, int16_t y) const override const`

Get pixel at coordinates. 

xX coordinate yY coordinate Pixel color value 

---

### `virtual void drawLine(int16_t x1, int16_t y1, int16_t x2, int16_t y2, uint8_t color) override`

Draw line. 

x1Start X coordinate y1Start Y coordinate x2End X coordinate y2End Y coordinate colorLine color 

---

### `virtual void drawRect(int16_t x, int16_t y, uint16_t width, uint16_t height, uint8_t color) override`

Draw rectangle outline. 

xX coordinate yY coordinate widthRectangle width heightRectangle height colorRectangle color 

---

### `virtual void fillRect(int16_t x, int16_t y, uint16_t width, uint16_t height, uint8_t color) override`

Fill rectangle. 

xX coordinate yY coordinate widthRectangle width heightRectangle height colorFill color 

---

### `virtual void drawCircle(int16_t x, int16_t y, uint16_t radius, uint8_t color) override`

Draw circle outline. 

xCenter X coordinate yCenter Y coordinate radiusCircle radius colorCircle color 

---

### `virtual void fillCircle(int16_t x, int16_t y, uint16_t radius, uint8_t color) override`

Fill circle. 

xCenter X coordinate yCenter Y coordinate radiusCircle radius colorFill color 

---

