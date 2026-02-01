---
id: CanvasGraphicsAdapter
title: CanvasGraphicsAdapter
sidebar_label: CanvasGraphicsAdapter
---

# CanvasGraphicsAdapter

Graphics adapter for enjin2 canvases. 


Implements  interface using enjin2 canvas types. Allows scripts to draw on any enjin2 canvas through the common interface. IScriptGraphicsclassenjin2_1_1IScriptGraphicscompound

---

**Namespace:** enjin2

**Header:** include/enjin2/scripting/lua_interpreter.hpp

## Public Methods

### `cpp
* CanvasGraphicsAdapter(CanvasType *targetCanvas)*
``

Constructor. 

paramtargetCanvasCanvas to draw on 

---

### `cpp
*void setCanvas(CanvasType *targetCanvas)*
``

Set target canvas. 

paramtargetCanvasNew canvas to draw on 

---

### `cpp
*void setColor(uint8_t color)*
``

Set current drawing color. 

paramcolorColor to use for drawing 

---

### `cpp
*virtual uint16_t getWidth() const override const*
``

Get canvas width. 

returnCanvas width in pixels 

---

### `cpp
*virtual uint16_t getHeight() const override const*
``

Get canvas height. 

returnCanvas height in pixels 

---

### `cpp
*virtual void clear(uint8_t color) override*
``

Clear canvas with specified color. 

paramcolorClear color 

---

### `cpp
*virtual void setPixel(int16_t x, int16_t y, uint8_t color) override*
``

Set pixel at coordinates. 

paramxX coordinate yY coordinate colorPixel color 

---

### `cpp
*virtual uint8_t getPixel(int16_t x, int16_t y) const override const*
``

Get pixel at coordinates. 

paramxX coordinate yY coordinate returnPixel color value 

---

### `cpp
*virtual void drawLine(int16_t x1, int16_t y1, int16_t x2, int16_t y2, uint8_t color) override*
``

Draw line. 

paramx1Start X coordinate y1Start Y coordinate x2End X coordinate y2End Y coordinate colorLine color 

---

### `cpp
*virtual void drawRect(int16_t x, int16_t y, uint16_t width, uint16_t height, uint8_t color) override*
``

Draw rectangle outline. 

paramxX coordinate yY coordinate widthRectangle width heightRectangle height colorRectangle color 

---

### `cpp
*virtual void fillRect(int16_t x, int16_t y, uint16_t width, uint16_t height, uint8_t color) override*
``

Fill rectangle. 

paramxX coordinate yY coordinate widthRectangle width heightRectangle height colorFill color 

---

### `cpp
*virtual void drawCircle(int16_t x, int16_t y, uint16_t radius, uint8_t color) override*
``

Draw circle outline. 

paramxCenter X coordinate yCenter Y coordinate radiusCircle radius colorCircle color 

---

### `cpp
*virtual void fillCircle(int16_t x, int16_t y, uint16_t radius, uint8_t color) override*
``

Fill circle. 

paramxCenter X coordinate yCenter Y coordinate radiusCircle radius colorFill color 

---

