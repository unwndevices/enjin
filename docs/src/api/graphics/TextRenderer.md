---
id: TextRenderer
title: TextRenderer
sidebar_label: TextRenderer
---

# TextRenderer

Text renderer for drawing text to canvas. 



    

---

**Namespace:** `enjin2`

**Header:** `include/enjin2/graphics/text_renderer.hpp`

## Public Methods

### ` TextRenderer()`

Construct a new . TextRendererclassenjin2_1_1TextRenderercompound


        

---

### `void setFont(const GFXfont *font)`

Set the GFX font. 

paramfontPointer to GFX font structure (nullptr for default built-in font) 

---

### `void setTextColor(TPixel color)`

Set text color (transparent background). 

paramcolorText color 

---

### `void setTextColor(TPixel color, TPixel bgcolor)`

Set text color with background. 

paramcolorText color bgcolorBackground color 

---

### `void setTextSize(uint8_t size)`

Set text size. 

paramsize multiplier (1 = normal, 2 = double, etc.) Sizestructenjin2_1_1Sizecompound

---

### `void setTextSize(uint8_t sx, uint8_t sy)`

Set text size with separate X/Y scaling. 

paramsxX size multiplier syY size multiplier 

---

### `void setCursor(int16_t x, int16_t y)`

Set cursor position. 

paramxX coordinate yY coordinate 

---

### `void setTextWrap(bool wrap)`

Set text wrapping. 

paramwrapTrue to enable text wrapping 

---

### `void drawChar(ICanvas< TPixel > &canvas, int16_t x, int16_t y, unsigned char c)`

Draw a single character (Adafruit GFX style). 

paramcanvasCanvas to draw to xX position yY position cCharacter to draw 

---

### `void drawString(ICanvas< TPixel > &canvas, int16_t x, int16_t y, const char *str)`

Draw a string. 

paramcanvasCanvas to draw to xX position yY position strString to draw 

---

### `void drawStringWrapped(ICanvas< TPixel > &canvas, int16_t x, int16_t y, uint16_t width, const char *str)`

Draw a string with automatic wrapping. 

paramcanvasCanvas to draw to xX position yY position widthMaximum width before wrapping strString to draw 

---

### `void getTextBounds(const char *str, int16_t x, int16_t y, int16_t *x1, int16_t *y1, uint16_t *w, uint16_t *h)`

Get text bounds for a string. 

paramstrString to measure xX position yY position x1Output: left bound y1Output: top bound wOutput: width hOutput: height 

---

### `uint16_t getTextWidth(const char *str)`

Get width of a string in pixels. 

paramstrString to measure returnWidth in pixels 

---

### `void writeChar(ICanvas< TPixel > &canvas, unsigned char c)`

Write a character at current cursor position. 

paramcanvasCanvas to draw to cCharacter to write 

---

### `uint16_t getCharWidth(unsigned char c)`

Get character width. 


        

---

### `uint8_t getCharHeight()`

Get character height. 


        

---

