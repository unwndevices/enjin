---
id: LuaCanvas
title: LuaCanvas
sidebar_label: LuaCanvas
---

# LuaCanvas

Canvas wrapper for Lua bindings. 


Provides a type-erased canvas interface that can hold either 4-bit or 8-bit canvases for Lua scripting. 

---

**Namespace:** `enjin2`

**Header:** `include/enjin2/scripting/bindings.hpp`

## Public Methods

### ` LuaCanvas(Canvas4< W, H > *canvas)`

Constructor for 4-bit canvas. 

paramcanvas4-bit canvas pointer wCanvas width hCanvas height 

---

### ` LuaCanvas(Canvas8< W, H > *canvas)`

Constructor for 8-bit canvas. 

paramcanvas8-bit canvas pointer wCanvas width hCanvas height 

---

### `uint16_t getWidth() const const`

Get canvas width. 

returnCanvas width in pixels 

---

### `uint16_t getHeight() const const`

Get canvas height. 

returnCanvas height in pixels 

---

### `bool is4BitCanvas() const const`

Check if this is a 4-bit canvas. 

returnTrue if 4-bit, false if 8-bit 

---

### `void clear(uint8_t color)`

Clear canvas with specified color. 

paramcolorClear color (0-15 for 4-bit, 0-255 for 8-bit) 

---

### `void setPixel(int16_t x, int16_t y, uint8_t color)`

Set pixel at coordinates. 

paramxX coordinate yY coordinate colorPixel color 

---

### `uint8_t getPixel(int16_t x, int16_t y) const const`

Get pixel at coordinates. 

paramxX coordinate yY coordinate returnPixel color value 

---

### `void drawLine(int16_t x1, int16_t y1, int16_t x2, int16_t y2, uint8_t color)`

Draw line. 

paramx1Start X coordinate y1Start Y coordinate x2End X coordinate y2End Y coordinate colorLine color 

---

### `void drawRect(int16_t x, int16_t y, uint16_t width, uint16_t height, uint8_t color)`

Draw rectangle outline. 

paramxX coordinate yY coordinate widthRectangle width heightRectangle height colorRectangle color 

---

### `void fillRect(int16_t x, int16_t y, uint16_t width, uint16_t height, uint8_t color)`

Fill rectangle. 

paramxX coordinate yY coordinate widthRectangle width heightRectangle height colorFill color 

---

### `void drawCircle(int16_t x, int16_t y, uint16_t radius, uint8_t color)`

Draw circle outline. 

paramxCenter X coordinate yCenter Y coordinate radiusCircle radius colorCircle color 

---

### `void fillCircle(int16_t x, int16_t y, uint16_t radius, uint8_t color)`

Fill circle. 

paramxCenter X coordinate yCenter Y coordinate radiusCircle radius colorFill color 

---

### `void drawTriangle(int16_t x1, int16_t y1, int16_t x2, int16_t y2, int16_t x3, int16_t y3, uint8_t color)`

Draw triangle outline. 

paramx1First vertex X y1First vertex Y x2Second vertex X y2Second vertex Y x3Third vertex X y3Third vertex Y colorTriangle color 

---

### `void fillTriangle(int16_t x1, int16_t y1, int16_t x2, int16_t y2, int16_t x3, int16_t y3, uint8_t color)`

Fill triangle. 

paramx1First vertex X y1First vertex Y x2Second vertex X y2Second vertex Y x3Third vertex X y3Third vertex Y colorFill color 

---

