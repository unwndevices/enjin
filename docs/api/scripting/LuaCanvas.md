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

### ` LuaCanvas(Canvas4&lt; W, H &gt; *canvas)`

Constructor for 4-bit canvas. 

WCanvas width HCanvas height canvas4-bit canvas pointer 

---

### ` LuaCanvas(Canvas8&lt; W, H &gt; *canvas)`

Constructor for 8-bit canvas. 

WCanvas width HCanvas height canvas8-bit canvas pointer 

---

### ` LuaCanvas(ICanvas&lt; Pixel4 &gt; *canvas)`

Constructor for abstract 4-bit canvas interface Used when only an ICanvas&lt;Pixel4&gt;& is available (e.g., C_Drawable::draw()). Width and height are read from the interface at construction time. 

canvasAbstract 4-bit canvas pointer (non-owning) 

---

### `uint16_t getWidth() const`

Get canvas width. 

Canvas width in pixels 

---

### `uint16_t getHeight() const`

Get canvas height. 

Canvas height in pixels 

---

### `bool is4BitCanvas() const`

Check if this is a 4-bit canvas. 

True if 4-bit, false if 8-bit 

---

### `void clear(uint8_t color)`

Clear canvas with specified color. 

colorClear color (0-15 for 4-bit, 0-255 for 8-bit) 

---

### `void setPixel(int16_t x, int16_t y, uint8_t color)`

Set pixel at coordinates. 

xX coordinate yY coordinate colorPixel color 

---

### `uint8_t getPixel(int16_t x, int16_t y) const`

Get pixel at coordinates. 

xX coordinate yY coordinate Pixel color value 

---

### `void drawLine(int16_t x1, int16_t y1, int16_t x2, int16_t y2, uint8_t color)`

Draw line. 

x1Start X coordinate y1Start Y coordinate x2End X coordinate y2End Y coordinate colorLine color 

---

### `void drawRect(int16_t x, int16_t y, uint16_t width, uint16_t height, uint8_t color)`

Draw rectangle outline. 

xX coordinate yY coordinate widthRectangle width heightRectangle height colorRectangle color 

---

### `void fillRect(int16_t x, int16_t y, uint16_t width, uint16_t height, uint8_t color)`

Fill rectangle. 

xX coordinate yY coordinate widthRectangle width heightRectangle height colorFill color 

---

### `void drawCircle(int16_t x, int16_t y, uint16_t radius, uint8_t color)`

Draw circle outline. 

xCenter X coordinate yCenter Y coordinate radiusCircle radius colorCircle color 

---

### `void fillCircle(int16_t x, int16_t y, uint16_t radius, uint8_t color)`

Fill circle. 

xCenter X coordinate yCenter Y coordinate radiusCircle radius colorFill color 

---

### `void drawTriangle(int16_t x1, int16_t y1, int16_t x2, int16_t y2, int16_t x3, int16_t y3, uint8_t color)`

Draw triangle outline. 

x1First vertex X y1First vertex Y x2Second vertex X y2Second vertex Y x3Third vertex X y3Third vertex Y colorTriangle color 

---

### `void fillTriangle(int16_t x1, int16_t y1, int16_t x2, int16_t y2, int16_t x3, int16_t y3, uint8_t color)`

Fill triangle. 

x1First vertex X y1First vertex Y x2Second vertex X y2Second vertex Y x3Third vertex X y3Third vertex Y colorFill color 

---

### `void drawText(const char *str, int16_t x, int16_t y, uint8_t color, uint8_t size, const GFXfont *font)`

Draw text at position using given color, size, and font. 

strNull-terminated string to draw xX coordinate yY coordinate colorText color (0-15 for 4-bit, 0-255 for 8-bit) sizeSize multiplier (1=normal, 2=double, etc.) fontGFXfont pointer (nullptr = built-in 5x7) 

---

### `void drawTextWrapped(const char *str, int16_t x, int16_t y, uint16_t maxWidth, uint8_t color, uint8_t size, const GFXfont *font)`

Draw text with word wrapping within maxWidth. 

strNull-terminated string to draw xX coordinate yY coordinate maxWidthMaximum width in pixels before wrapping colorText color sizeSize multiplier fontGFXfont pointer (nullptr = built-in 5x7) 

---

### `uint16_t measureTextWidth(const char *str, uint8_t size, const GFXfont *font)`

Measure width of string in pixels with given size and font. 

strNull-terminated string to measure sizeSize multiplier fontGFXfont pointer (nullptr = built-in 5x7) Width in pixels 

---

### `uint8_t measureTextHeight(uint8_t size, const GFXfont *font)`

Measure character height in pixels with given size and font. 

sizeSize multiplier fontGFXfont pointer (nullptr = built-in 5x7) Height in pixels 

---

