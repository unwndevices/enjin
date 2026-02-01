---
id: CanvasExtended
title: CanvasExtended
sidebar_label: CanvasExtended
---

# CanvasExtended

Extended canvas functionality for advanced graphics operations. 


templateparamTCanvasCanvas type that implements  ICanvas<TPixel>classenjin2_1_1ICanvascompound

---

**Namespace:** enjin2

**Header:** include/enjin2/graphics/canvas_extended.hpp

## Public Methods

### `cpp
*static void drawLine(TCanvas &canvas, int16_t x0, int16_t y0, int16_t x1, int16_t y1, PixelType color)*
``

Draw a line using Bresenham's algorithm. 


        

---

### `cpp
*static void drawHLine(TCanvas &canvas, int16_t x, int16_t y, int16_t w, PixelType color)*
``

Draw a horizontal line. 


        

---

### `cpp
*static void drawVLine(TCanvas &canvas, int16_t x, int16_t y, int16_t h, PixelType color)*
``

Draw a vertical line. 


        

---

### `cpp
*static void drawRect(TCanvas &canvas, int16_t x, int16_t y, int16_t w, int16_t h, PixelType color)*
``

Draw a rectangle outline. 


        

---

### `cpp
*static void fillRect(TCanvas &canvas, int16_t x, int16_t y, int16_t w, int16_t h, PixelType color)*
``

Fill a rectangle. 


        

---

### `cpp
*static void drawCircle(TCanvas &canvas, int16_t x0, int16_t y0, int16_t r, PixelType color)*
``

Draw a circle using midpoint circle algorithm. 


        

---

### `cpp
*static void fillCircle(TCanvas &canvas, int16_t x0, int16_t y0, int16_t r, PixelType color)*
``

Fill a circle. 


        

---

### `cpp
*static void fillCircleHelper(TCanvas &canvas, int16_t x0, int16_t y0, int16_t r, uint8_t corners, int16_t delta, PixelType color)*
``

Circle helper for fill operations. 


        

---

### `cpp
*static void drawCircleHelper(TCanvas &canvas, int16_t x0, int16_t y0, int16_t r, uint8_t cornername, PixelType color)*
``

Draw circle helper for rounded rectangles. 


        

---

### `cpp
*static void drawTriangle(TCanvas &canvas, int16_t x0, int16_t y0, int16_t x1, int16_t y1, int16_t x2, int16_t y2, PixelType color)*
``

Draw a triangle outline. 


        

---

### `cpp
*static void fillTriangle(TCanvas &canvas, int16_t x0, int16_t y0, int16_t x1, int16_t y1, int16_t x2, int16_t y2, PixelType color)*
``

Fill a triangle with scanline algorithm. 


        

---

### `cpp
*static void drawRoundRect(TCanvas &canvas, int16_t x, int16_t y, int16_t w, int16_t h, int16_t radius, PixelType color)*
``

Draw a rounded rectangle. 


        

---

### `cpp
*static void fillRoundRect(TCanvas &canvas, int16_t x, int16_t y, int16_t w, int16_t h, int16_t radius, PixelType color)*
``

Fill a rounded rectangle. 


        

---

### `cpp
*static void drawBitmap(TCanvas &canvas, int16_t x, int16_t y, const uint8_t *bitmap, int16_t w, int16_t h, PixelType color)*
``

Draw bitmap at specified location. 


        

---

### `cpp
*static void drawGrayscaleBitmap(TCanvas &canvas, int16_t x, int16_t y, const uint8_t *bitmap, int16_t w, int16_t h)*
``

Draw grayscale bitmap (Enjin-style). 


        

---

### `cpp
*static void drawGrayscaleBitmap(TCanvas &canvas, int16_t x, int16_t y, const uint8_t *bitmap, const uint8_t *mask, int16_t w, int16_t h)*
``

Draw grayscale bitmap with mask. 


        

---

### `cpp
*static void blit(TCanvas &dst, const TSrcCanvas &src, int16_t dx, int16_t dy)*
``

Blit (copy) from one canvas to another. 


        

---

### `cpp
*static void blit(TCanvas &dst, const TSrcCanvas &src, int16_t dx, int16_t dy, int16_t w, int16_t h, int16_t sx=0, int16_t sy=0)*
``

Blit with specified dimensions and source offset. 


        

---

