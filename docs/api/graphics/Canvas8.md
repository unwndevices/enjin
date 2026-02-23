---
id: Canvas8
title: Canvas8
sidebar_label: Canvas8
---

# Canvas8

8-bit canvas with per-pixel byte storage 


WIDTHCanvas width in pixels HEIGHTCanvas height in pixels 

---

**Namespace:** `enjin2`

**Header:** `include/enjin2/graphics/canvas.hpp`

## Public Methods

### ` Canvas8()`

---

### `virtual uint16_t getWidth() const override const`

Get canvas width in pixels. 

Width in pixels 

---

### `virtual uint16_t getHeight() const override const`

Get canvas height in pixels. 

Height in pixels 

---

### `virtual void setPixel(int16_t x, int16_t y, uint8_t color) override`

Set pixel color at specified coordinates. 

xX coordinate yY coordinate colorPixel color to set 

---

### `virtual uint8_t getPixel(int16_t x, int16_t y) const override const`

Get pixel color at specified coordinates. 

xX coordinate yY coordinate Pixel color at the specified location 

---

### `virtual void clear(uint8_t color=0) override`

Clear entire canvas to specified color. 

colorColor to fill canvas with (default: black/zero) 

---

### `virtual void fill(const Rect &rect, uint8_t color) override`

Fill rectangular region with specified color. 

rectRectangle to fill colorColor to fill with 

---

### `void convertTo4bit(Canvas4&lt; WIDTH, HEIGHT &gt; &dst) const`

Convert canvas contents to 4-bit format. 

dstDestination 4-bit canvas 

---

### `const uint8_t * getBuffer() const`

Get read-only pointer to pixel buffer. 

Pointer to pixel data 

---

### `uint8_t * getBuffer()`

Get mutable pointer to pixel buffer. 

Pointer to pixel data 

---

### `void fillScreen(uint8_t color)`

Fill entire canvas (Adafruit_GFX compatibility). 

colorFill color 

---

### `void drawPixel(int16_t x, int16_t y, uint8_t color)`

Draw single pixel (Adafruit_GFX compatibility). 

xX coordinate yY coordinate colorPixel color 

---

### `uint16_t width() const`

Get canvas width (Adafruit_GFX compatibility). 

Width in pixels 

---

### `uint16_t height() const`

Get canvas height (Adafruit_GFX compatibility). 

Height in pixels 

---

### `void setTextColor(uint16_t color)`

Set text color. 

colorText color value 

---

### `void setTextColor(uint16_t color, uint16_t bg)`

Set text color with background. 

colorText color value bgBackground color value 

---

### `void setCursor(int16_t x, int16_t y)`

Set cursor position. 

xX coordinate yY coordinate 

---

### `int16_t getCursorX() const`

Get cursor X position. 

Current cursor X coordinate 

---

### `int16_t getCursorY() const`

Get cursor Y position. 

Current cursor Y coordinate 

---

### `size_t write(uint8_t c)`

Write a single character (Adafruit_GFX compatible). 

cCharacter to write Number of bytes written 

---

### `void print(const char *text)`

Print text at cursor position (Adafruit_GFX compatible). 

textNull-terminated string to print 

---

### `void println(const char *text)`

Print text with newline (basic implementation). 

textNull-terminated string to print 

---

### `void drawChar(int16_t x, int16_t y, unsigned char c, uint8_t color, uint8_t bg, uint8_t size_x, uint8_t size_y)`

Draw a single character (Adafruit_GFX compatible). 

xX coordinate yY coordinate cCharacter to draw colorText color bgBackground color size_xHorizontal scale factor size_yVertical scale factor 

---

### `void drawChar(int16_t x, int16_t y, unsigned char c, uint8_t color, uint8_t bg, uint8_t size)`

Draw a single character with uniform scaling. 

xX coordinate yY coordinate cCharacter to draw colorText color bgBackground color sizeScale factor for both axes 

---

### `virtual void setTextSize(uint8_t s)`

Set text size scaling. 

sUniform scale factor 

---

### `void setTextSize(uint8_t s_x, uint8_t s_y)`

Set text size scaling with independent axes. 

s_xHorizontal scale factor s_yVertical scale factor 

---

### `void setTextWrap(bool w)`

Set text wrap mode. 

wEnable or disable text wrapping 

---

### `int16_t getTextWidth(const char *text)`

Get text width (6 pixels per character times size). 

textNull-terminated string to measure Width in pixels 

---

### `virtual void fillRect(int16_t x, int16_t y, int16_t w, int16_t h, uint8_t color)`

Fill rectangle. 

xTop-left X coordinate yTop-left Y coordinate wWidth in pixels hHeight in pixels colorFill color 

---

### `virtual void drawRect(int16_t x, int16_t y, int16_t w, int16_t h, uint8_t color)`

Draw rectangle outline. 

xTop-left X coordinate yTop-left Y coordinate wWidth in pixels hHeight in pixels colorOutline color 

---

### `virtual void drawLine(int16_t x0, int16_t y0, int16_t x1, int16_t y1, uint8_t color)`

Draw a line using Bresenham's algorithm. 

x0Start X coordinate y0Start Y coordinate x1End X coordinate y1End Y coordinate colorLine color 

---

### `virtual void fillCircle(int16_t x0, int16_t y0, int16_t radius, uint8_t color)`

Draw a filled circle using midpoint circle algorithm. 

x0Center X coordinate y0Center Y coordinate radiusCircle radius colorFill color 

---

### `virtual void drawCircle(int16_t x0, int16_t y0, int16_t radius, uint8_t color)`

Draw circle outline using midpoint circle algorithm. 

x0Center X coordinate y0Center Y coordinate radiusCircle radius colorOutline color 

---

### `void drawRoundRect(int16_t x, int16_t y, int16_t w, int16_t h, int16_t radius, uint8_t color)`

Draw rounded rectangle outline. 

xTop-left X coordinate yTop-left Y coordinate wWidth in pixels hHeight in pixels radiusCorner radius colorOutline color 

---

### `void fillRoundRect(int16_t x, int16_t y, int16_t w, int16_t h, int16_t radius, uint8_t color)`

Fill rounded rectangle. 

xTop-left X coordinate yTop-left Y coordinate wWidth in pixels hHeight in pixels radiusCorner radius colorFill color 

---

### `void fillRectWithPattern(int16_t x, int16_t y, int16_t w, int16_t h, const uint8_t *pattern, int16_t patternWidth, int16_t patternHeight)`

Fill rectangle with repeating pattern. 

xTop-left X coordinate yTop-left Y coordinate wWidth in pixels hHeight in pixels patternPointer to pattern data patternWidthPattern width in pixels patternHeightPattern height in pixels 

---

### `void drawGrayscaleBitmap(int16_t x, int16_t y, const uint8_t *bitmap, int16_t w, int16_t h)`

Draw grayscale bitmap with basic parameters. 

xDestination X coordinate yDestination Y coordinate bitmapPointer to bitmap data wBitmap width hBitmap height 

---

### `void drawGrayscaleBitmap(int16_t x, int16_t y, const uint8_t *bitmap, uint8_t matte, uint8_t w, uint8_t h)`

Draw grayscale bitmap with matte threshold. 

xDestination X coordinate yDestination Y coordinate bitmapPointer to bitmap data matteTransparent color value to skip wBitmap width hBitmap height 

---

### `void drawGrayscaleBitmap(int16_t x, int16_t y, const uint8_t *bitmap, const uint8_t *mask, uint8_t w, uint8_t h)`

Draw grayscale bitmap with mask. 

xDestination X coordinate yDestination Y coordinate bitmapPointer to bitmap data maskPointer to mask data (non-zero values allow drawing) wBitmap width hBitmap height 

---

### `void drawGrayscaleBitmap(int16_t x, int16_t y, const uint8_t *bitmap, uint8_t matte, uint8_t w, uint8_t h, uint8_t opacity_divisor)`

Draw grayscale bitmap with opacity. 

xDestination X coordinate yDestination Y coordinate bitmapPointer to bitmap data matteTransparent color value to skip wBitmap width hBitmap height opacity_divisorOpacity divisor (higher = more transparent) 

---

### `void add(Canvas8 *over)`

Add blending operation with another canvas. 

overCanvas to blend over this one 

---

### `void add(const uint8_t *texture)`

Add blending operation with texture data. 

texturePointer to texture data 

---

### `void subtract(Canvas8 *over)`

Subtract blending operation with another canvas. 

overCanvas to subtract from this one 

---

### `void subtract(const uint8_t *texture)`

Subtract blending operation with texture data. 

texturePointer to texture data 

---

### `void difference(int16_t x, int16_t y, const uint8_t *texture, uint8_t w, uint8_t h)`

Difference blending operation with texture data. 

xDestination X coordinate yDestination Y coordinate texturePointer to texture data wTexture width hTexture height 

---

### `void exportToPGM(const char *filename) const`

Export canvas to PGM format with proper color scaling. 

filenameOutput filename 

---

### `void exportToBMP(const char *filename) const`

Export canvas to BMP format (24-bit RGB, grayscale as gray=R=G=B). 

filenameOutput filename 

---

### `void setFont(const GFXfont *font=nullptr)`

Set GFX font for text rendering. 

fontPointer to GFXfont structure (nullptr for built-in font) 

---

### `void charBounds(unsigned char c, int16_t *x, int16_t *y, int16_t *minx, int16_t *miny, int16_t *maxx, int16_t *maxy)`

Helper to determine character bounds (Adafruit_GFX compatible). 

cCharacter to measure xCurrent X cursor position (updated) yCurrent Y cursor position (updated) minxMinimum X bound (updated) minyMinimum Y bound (updated) maxxMaximum X bound (updated) maxyMaximum Y bound (updated) 

---

### `void getTextBounds(const char *str, int16_t x, int16_t y, int16_t *x1, int16_t *y1, uint16_t *w, uint16_t *h)`

Get text bounds (Adafruit_GFX compatible). 

strNull-terminated string to measure xStarting X position yStarting Y position x1Output minimum X bound y1Output minimum Y bound wOutput text width hOutput text height 

---

### `void fillTriangle(int16_t x0, int16_t y0, int16_t x1, int16_t y1, int16_t x2, int16_t y2, uint8_t color)`

Fill triangle (basic implementation). 

x0First vertex X y0First vertex Y x1Second vertex X y1Second vertex Y x2Third vertex X y2Third vertex Y colorFill color 

---

