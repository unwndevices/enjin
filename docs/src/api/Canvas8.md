---
id: Canvas8
title: Canvas8
sidebar_label: Canvas8
---

# Canvas8


    



    

---

**Namespace:** enjin2

**Header:** include/enjin2/graphics/canvas.hpp

## Public Methods

### `cpp
* Canvas8()*
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
*virtual void setPixel(int16_t x, int16_t y, uint8_t color) override*
``

Set pixel color at specified coordinates. 

paramxX coordinate yY coordinate colorPixel color to set 

---

### `cpp
*virtual uint8_t getPixel(int16_t x, int16_t y) const override const*
``

Get pixel color at specified coordinates. 

paramxX coordinate yY coordinate returnPixel color at the specified location 

---

### `cpp
*virtual void clear(uint8_t color=0) override*
``

Clear entire canvas to specified color. 

paramcolorColor to fill canvas with (default: black/zero) 

---

### `cpp
*virtual void fill(const Rect &rect, uint8_t color) override*
``

Fill rectangular region with specified color. 

paramrectRectangle to fill colorColor to fill with 

---

### `cpp
*void convertTo4bit(Canvas4&lt; WIDTH, HEIGHT &gt; &dst) const const*
``


        


        

---

### `cpp
*const uint8_t * getBuffer() const const*
``


        


        

---

### `cpp
*uint8_t * getBuffer()*
``


        


        

---

### `cpp
*void fillScreen(uint8_t color)*
``

Fill entire canvas (Adafruit_GFX compatibility). 


        

---

### `cpp
*void drawPixel(int16_t x, int16_t y, uint8_t color)*
``

Draw single pixel (Adafruit_GFX compatibility). 


        

---

### `cpp
*uint16_t width() const const*
``

Get canvas width (Adafruit_GFX compatibility). 


        

---

### `cpp
*uint16_t height() const const*
``

Get canvas height (Adafruit_GFX compatibility). 


        

---

### `cpp
*void setTextColor(uint16_t color)*
``

Set text color. 


        

---

### `cpp
*void setTextColor(uint16_t color, uint16_t bg)*
``

Set text color with background. 


        

---

### `cpp
*void setCursor(int16_t x, int16_t y)*
``

Set cursor position. 


        

---

### `cpp
*int16_t getCursorX() const const*
``

Get cursor X position. 


        

---

### `cpp
*int16_t getCursorY() const const*
``

Get cursor Y position. 


        

---

### `cpp
*size_t write(uint8_t c)*
``

Write a single character (Adafruit_GFX compatible). 


        

---

### `cpp
*void print(const char *text)*
``

Print text at cursor position (Adafruit_GFX compatible). 


        

---

### `cpp
*void println(const char *text)*
``

Print text with newline (basic implementation). 


        

---

### `cpp
*void drawChar(int16_t x, int16_t y, unsigned char c, uint8_t color, uint8_t bg, uint8_t size_x, uint8_t size_y)*
``

Draw a single character (Adafruit_GFX compatible). 


        

---

### `cpp
*void drawChar(int16_t x, int16_t y, unsigned char c, uint8_t color, uint8_t bg, uint8_t size)*
``


        


        

---

### `cpp
*virtual void setTextSize(uint8_t s)*
``

Set text size scaling. 


        

---

### `cpp
*void setTextSize(uint8_t s_x, uint8_t s_y)*
``


        


        

---

### `cpp
*void setTextWrap(bool w)*
``

Set text wrap mode. 


        

---

### `cpp
*int16_t getTextWidth(const char *text)*
``

Get text width (6 pixels per character times size). 


        

---

### `cpp
*virtual void fillRect(int16_t x, int16_t y, int16_t w, int16_t h, uint8_t color)*
``

Fill rectangle. 


        

---

### `cpp
*virtual void drawRect(int16_t x, int16_t y, int16_t w, int16_t h, uint8_t color)*
``

Draw rectangle outline. 


        

---

### `cpp
*virtual void drawLine(int16_t x0, int16_t y0, int16_t x1, int16_t y1, uint8_t color)*
``

Draw a line using Bresenham's algorithm. 


        

---

### `cpp
*virtual void fillCircle(int16_t x0, int16_t y0, int16_t radius, uint8_t color)*
``

Draw a filled circle using midpoint circle algorithm. 


        

---

### `cpp
*virtual void drawCircle(int16_t x0, int16_t y0, int16_t radius, uint8_t color)*
``

Draw circle outline using midpoint circle algorithm. 


        

---

### `cpp
*void drawRoundRect(int16_t x, int16_t y, int16_t w, int16_t h, int16_t radius, uint8_t color)*
``

Draw rounded rectangle outline. 


        

---

### `cpp
*void fillRoundRect(int16_t x, int16_t y, int16_t w, int16_t h, int16_t radius, uint8_t color)*
``

Fill rounded rectangle. 


        

---

### `cpp
*void fillRectWithPattern(int16_t x, int16_t y, int16_t w, int16_t h, const uint8_t *pattern, int16_t patternWidth, int16_t patternHeight)*
``

Fill rectangle with repeating pattern. 


        

---

### `cpp
*void drawGrayscaleBitmap(int16_t x, int16_t y, const uint8_t *bitmap, int16_t w, int16_t h)*
``

Draw grayscale bitmap with basic parameters. 


        

---

### `cpp
*void drawGrayscaleBitmap(int16_t x, int16_t y, const uint8_t *bitmap, uint8_t matte, uint8_t w, uint8_t h)*
``

Draw grayscale bitmap with matte threshold. 


        

---

### `cpp
*void drawGrayscaleBitmap(int16_t x, int16_t y, const uint8_t *bitmap, const uint8_t *mask, uint8_t w, uint8_t h)*
``

Draw grayscale bitmap with mask. 


        

---

### `cpp
*void drawGrayscaleBitmap(int16_t x, int16_t y, const uint8_t *bitmap, uint8_t matte, uint8_t w, uint8_t h, uint8_t opacity_divisor)*
``

Draw grayscale bitmap with opacity. 


        

---

### `cpp
*void add(Canvas8 *over)*
``

Add blending operation with another canvas. 


        

---

### `cpp
*void add(const uint8_t *texture)*
``

Add blending operation with texture data. 


        

---

### `cpp
*void subtract(Canvas8 *over)*
``

Subtract blending operation with another canvas. 


        

---

### `cpp
*void subtract(const uint8_t *texture)*
``

Subtract blending operation with texture data. 


        

---

### `cpp
*void difference(int16_t x, int16_t y, const uint8_t *texture, uint8_t w, uint8_t h)*
``

Difference blending operation with texture data. 


        

---

### `cpp
*void exportToPGM(const char *filename) const const*
``

Export canvas to PGM format with proper color scaling. 

paramfilenameOutput filename 

---

### `cpp
*void exportToBMP(const char *filename) const const*
``

Export canvas to BMP format (24-bit RGB, grayscale as gray=R=G=B). 

paramfilenameOutput filename 

---

### `cpp
*void setFont(const GFXfont *font=nullptr)*
``

Set GFX font for text rendering. 

paramfontPointer to  structure (nullptr for built-in font) GFXfontstructenjin2_1_1GFXfontcompound

---

### `cpp
*void charBounds(unsigned char c, int16_t *x, int16_t *y, int16_t *minx, int16_t *miny, int16_t *maxx, int16_t *maxy)*
``

Helper to determine character bounds (Adafruit_GFX compatible). 


        

---

### `cpp
*void getTextBounds(const char *str, int16_t x, int16_t y, int16_t *x1, int16_t *y1, uint16_t *w, uint16_t *h)*
``

Get text bounds (Adafruit_GFX compatible). 


        

---

### `cpp
*void fillTriangle(int16_t x0, int16_t y0, int16_t x1, int16_t y1, int16_t x2, int16_t y2, uint8_t color)*
``

Fill triangle (basic implementation). 


        

---

