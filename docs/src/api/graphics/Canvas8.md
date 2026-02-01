---
id: Canvas8
title: Canvas8
sidebar_label: Canvas8
---

# Canvas8


    



    

---

**Namespace:** `enjin2`

**Header:** `include/enjin2/graphics/canvas.hpp`

## Public Methods

### `` Canvas8()``


        


        

---

### ``virtual uint16_t getWidth() const override const``

Get canvas width in pixels. 

returnWidth in pixels 

---

### ``virtual uint16_t getHeight() const override const``

Get canvas height in pixels. 

returnHeight in pixels 

---

### ``virtual void setPixel(int16_t x, int16_t y, uint8_t color) override``

Set pixel color at specified coordinates. 

paramxX coordinate yY coordinate colorPixel color to set 

---

### ``virtual uint8_t getPixel(int16_t x, int16_t y) const override const``

Get pixel color at specified coordinates. 

paramxX coordinate yY coordinate returnPixel color at the specified location 

---

### ``virtual void clear(uint8_t color=0) override``

Clear entire canvas to specified color. 

paramcolorColor to fill canvas with (default: black/zero) 

---

### ``virtual void fill(const Rect &rect, uint8_t color) override``

Fill rectangular region with specified color. 

paramrectRectangle to fill colorColor to fill with 

---

### ``void convertTo4bit(Canvas4&lt; WIDTH, HEIGHT &gt; &dst) const const``


        


        

---

### ``const uint8_t * getBuffer() const const``


        


        

---

### ``uint8_t * getBuffer()``


        


        

---

### ``void fillScreen(uint8_t color)``

Fill entire canvas (Adafruit_GFX compatibility). 


        

---

### ``void drawPixel(int16_t x, int16_t y, uint8_t color)``

Draw single pixel (Adafruit_GFX compatibility). 


        

---

### ``uint16_t width() const const``

Get canvas width (Adafruit_GFX compatibility). 


        

---

### ``uint16_t height() const const``

Get canvas height (Adafruit_GFX compatibility). 


        

---

### ``void setTextColor(uint16_t color)``

Set text color. 


        

---

### ``void setTextColor(uint16_t color, uint16_t bg)``

Set text color with background. 


        

---

### ``void setCursor(int16_t x, int16_t y)``

Set cursor position. 


        

---

### ``int16_t getCursorX() const const``

Get cursor X position. 


        

---

### ``int16_t getCursorY() const const``

Get cursor Y position. 


        

---

### ``size_t write(uint8_t c)``

Write a single character (Adafruit_GFX compatible). 


        

---

### ``void print(const char *text)``

Print text at cursor position (Adafruit_GFX compatible). 


        

---

### ``void println(const char *text)``

Print text with newline (basic implementation). 


        

---

### ``void drawChar(int16_t x, int16_t y, unsigned char c, uint8_t color, uint8_t bg, uint8_t size_x, uint8_t size_y)``

Draw a single character (Adafruit_GFX compatible). 


        

---

### ``void drawChar(int16_t x, int16_t y, unsigned char c, uint8_t color, uint8_t bg, uint8_t size)``


        


        

---

### ``virtual void setTextSize(uint8_t s)``

Set text size scaling. 


        

---

### ``void setTextSize(uint8_t s_x, uint8_t s_y)``


        


        

---

### ``void setTextWrap(bool w)``

Set text wrap mode. 


        

---

### ``int16_t getTextWidth(const char *text)``

Get text width (6 pixels per character times size). 


        

---

### ``virtual void fillRect(int16_t x, int16_t y, int16_t w, int16_t h, uint8_t color)``

Fill rectangle. 


        

---

### ``virtual void drawRect(int16_t x, int16_t y, int16_t w, int16_t h, uint8_t color)``

Draw rectangle outline. 


        

---

### ``virtual void drawLine(int16_t x0, int16_t y0, int16_t x1, int16_t y1, uint8_t color)``

Draw a line using Bresenham's algorithm. 


        

---

### ``virtual void fillCircle(int16_t x0, int16_t y0, int16_t radius, uint8_t color)``

Draw a filled circle using midpoint circle algorithm. 


        

---

### ``virtual void drawCircle(int16_t x0, int16_t y0, int16_t radius, uint8_t color)``

Draw circle outline using midpoint circle algorithm. 


        

---

### ``void drawRoundRect(int16_t x, int16_t y, int16_t w, int16_t h, int16_t radius, uint8_t color)``

Draw rounded rectangle outline. 


        

---

### ``void fillRoundRect(int16_t x, int16_t y, int16_t w, int16_t h, int16_t radius, uint8_t color)``

Fill rounded rectangle. 


        

---

### ``void fillRectWithPattern(int16_t x, int16_t y, int16_t w, int16_t h, const uint8_t *pattern, int16_t patternWidth, int16_t patternHeight)``

Fill rectangle with repeating pattern. 


        

---

### ``void drawGrayscaleBitmap(int16_t x, int16_t y, const uint8_t *bitmap, int16_t w, int16_t h)``

Draw grayscale bitmap with basic parameters. 


        

---

### ``void drawGrayscaleBitmap(int16_t x, int16_t y, const uint8_t *bitmap, uint8_t matte, uint8_t w, uint8_t h)``

Draw grayscale bitmap with matte threshold. 


        

---

### ``void drawGrayscaleBitmap(int16_t x, int16_t y, const uint8_t *bitmap, const uint8_t *mask, uint8_t w, uint8_t h)``

Draw grayscale bitmap with mask. 


        

---

### ``void drawGrayscaleBitmap(int16_t x, int16_t y, const uint8_t *bitmap, uint8_t matte, uint8_t w, uint8_t h, uint8_t opacity_divisor)``

Draw grayscale bitmap with opacity. 


        

---

### ``void add(Canvas8 *over)``

Add blending operation with another canvas. 


        

---

### ``void add(const uint8_t *texture)``

Add blending operation with texture data. 


        

---

### ``void subtract(Canvas8 *over)``

Subtract blending operation with another canvas. 


        

---

### ``void subtract(const uint8_t *texture)``

Subtract blending operation with texture data. 


        

---

### ``void difference(int16_t x, int16_t y, const uint8_t *texture, uint8_t w, uint8_t h)``

Difference blending operation with texture data. 


        

---

### ``void exportToPGM(const char *filename) const const``

Export canvas to PGM format with proper color scaling. 

paramfilenameOutput filename 

---

### ``void exportToBMP(const char *filename) const const``

Export canvas to BMP format (24-bit RGB, grayscale as gray=R=G=B). 

paramfilenameOutput filename 

---

### ``void setFont(const GFXfont *font=nullptr)``

Set GFX font for text rendering. 

paramfontPointer to  structure (nullptr for built-in font) GFXfontstructenjin2_1_1GFXfontcompound

---

### ``void charBounds(unsigned char c, int16_t *x, int16_t *y, int16_t *minx, int16_t *miny, int16_t *maxx, int16_t *maxy)``

Helper to determine character bounds (Adafruit_GFX compatible). 


        

---

### ``void getTextBounds(const char *str, int16_t x, int16_t y, int16_t *x1, int16_t *y1, uint16_t *w, uint16_t *h)``

Get text bounds (Adafruit_GFX compatible). 


        

---

### ``void fillTriangle(int16_t x0, int16_t y0, int16_t x1, int16_t y1, int16_t x2, int16_t y2, uint8_t color)``

Fill triangle (basic implementation). 


        

---

