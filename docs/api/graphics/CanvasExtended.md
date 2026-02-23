---
id: CanvasExtended
title: CanvasExtended
sidebar_label: CanvasExtended
---

# CanvasExtended

Extended canvas functionality for advanced graphics operations. 


TCanvasCanvas type that implements ICanvas<TPixel>

---

**Namespace:** `enjin2`

**Header:** `include/enjin2/graphics/canvas_extended.hpp`

## Public Methods

### `static void drawLine(TCanvas &canvas, int16_t x0, int16_t y0, int16_t x1, int16_t y1, PixelType color)`

Draw a line using Bresenham's algorithm. 

canvasTarget canvas x0Starting X coordinate y0Starting Y coordinate x1Ending X coordinate y1Ending Y coordinate colorLine color 

---

### `static void drawHLine(TCanvas &canvas, int16_t x, int16_t y, int16_t w, PixelType color)`

Draw a horizontal line. 

canvasTarget canvas xStarting X coordinate yY coordinate wLine width in pixels colorLine color 

---

### `static void drawVLine(TCanvas &canvas, int16_t x, int16_t y, int16_t h, PixelType color)`

Draw a vertical line. 

canvasTarget canvas xX coordinate yStarting Y coordinate hLine height in pixels colorLine color 

---

### `static void drawRect(TCanvas &canvas, int16_t x, int16_t y, int16_t w, int16_t h, PixelType color)`

Draw a rectangle outline. 

canvasTarget canvas xX coordinate of top-left corner yY coordinate of top-left corner wWidth in pixels hHeight in pixels colorRectangle outline color 

---

### `static void fillRect(TCanvas &canvas, int16_t x, int16_t y, int16_t w, int16_t h, PixelType color)`

Fill a rectangle. 

canvasTarget canvas xX coordinate of top-left corner yY coordinate of top-left corner wWidth in pixels hHeight in pixels colorFill color 

---

### `static void drawCircle(TCanvas &canvas, int16_t x0, int16_t y0, int16_t r, PixelType color)`

Draw a circle using midpoint circle algorithm. 

canvasTarget canvas x0Circle center X coordinate y0Circle center Y coordinate rCircle radius in pixels colorCircle outline color 

---

### `static void fillCircle(TCanvas &canvas, int16_t x0, int16_t y0, int16_t r, PixelType color)`

Fill a circle. 

canvasTarget canvas x0Circle center X coordinate y0Circle center Y coordinate rCircle radius in pixels colorFill color 

---

### `static void fillCircleHelper(TCanvas &canvas, int16_t x0, int16_t y0, int16_t r, uint8_t corners, int16_t delta, PixelType color)`

Circle helper for fill operations. 

canvasTarget canvas x0Circle center X coordinate y0Circle center Y coordinate rCircle radius cornersBitmask of corners to fill deltaOffset for fill extent colorFill color 

---

### `static void drawCircleHelper(TCanvas &canvas, int16_t x0, int16_t y0, int16_t r, uint8_t cornername, PixelType color)`

Draw circle helper for rounded rectangles. 

canvasTarget canvas x0Circle center X coordinate y0Circle center Y coordinate rCircle radius cornernameBitmask of corners to draw colorOutline color 

---

### `static void drawTriangle(TCanvas &canvas, int16_t x0, int16_t y0, int16_t x1, int16_t y1, int16_t x2, int16_t y2, PixelType color)`

Draw a triangle outline. 

canvasTarget canvas x0First vertex X y0First vertex Y x1Second vertex X y1Second vertex Y x2Third vertex X y2Third vertex Y colorOutline color 

---

### `static void fillTriangle(TCanvas &canvas, int16_t x0, int16_t y0, int16_t x1, int16_t y1, int16_t x2, int16_t y2, PixelType color)`

Fill a triangle with scanline algorithm. 

canvasTarget canvas x0First vertex X y0First vertex Y x1Second vertex X y1Second vertex Y x2Third vertex X y2Third vertex Y colorFill color 

---

### `static void drawRoundRect(TCanvas &canvas, int16_t x, int16_t y, int16_t w, int16_t h, int16_t radius, PixelType color)`

Draw a rounded rectangle. 

canvasTarget canvas xTop-left X coordinate yTop-left Y coordinate wWidth in pixels hHeight in pixels radiusCorner radius colorOutline color 

---

### `static void fillRoundRect(TCanvas &canvas, int16_t x, int16_t y, int16_t w, int16_t h, int16_t radius, PixelType color)`

Fill a rounded rectangle. 

canvasTarget canvas xTop-left X coordinate yTop-left Y coordinate wWidth in pixels hHeight in pixels radiusCorner radius colorFill color 

---

### `static void drawBitmap(TCanvas &canvas, int16_t x, int16_t y, const uint8_t *bitmap, int16_t w, int16_t h, PixelType color)`

Draw bitmap at specified location. 

canvasTarget canvas xDestination X coordinate yDestination Y coordinate bitmapPointer to 1-bit bitmap data wBitmap width hBitmap height colorForeground color 

---

### `static void drawGrayscaleBitmap(TCanvas &canvas, int16_t x, int16_t y, const uint8_t *bitmap, int16_t w, int16_t h)`

Draw grayscale bitmap (Enjin-style). 

canvasTarget canvas xDestination X coordinate yDestination Y coordinate bitmapPointer to grayscale bitmap data wBitmap width hBitmap height 

---

### `static void drawGrayscaleBitmap(TCanvas &canvas, int16_t x, int16_t y, const uint8_t *bitmap, const uint8_t *mask, int16_t w, int16_t h)`

Draw grayscale bitmap with mask. 

canvasTarget canvas xDestination X coordinate yDestination Y coordinate bitmapPointer to grayscale bitmap data maskPointer to mask data wBitmap width hBitmap height 

---

### `static void blit(TCanvas &dst, const TSrcCanvas &src, int16_t dx, int16_t dy)`

Blit (copy) from one canvas to another. 

TSrcCanvasSource canvas type dstDestination canvas srcSource canvas dxDestination X coordinate dyDestination Y coordinate 

---

### `static void blit(TCanvas &dst, const TSrcCanvas &src, int16_t dx, int16_t dy, int16_t w, int16_t h, int16_t sx=0, int16_t sy=0)`

Blit with specified dimensions and source offset. 

TSrcCanvasSource canvas type dstDestination canvas srcSource canvas dxDestination X coordinate dyDestination Y coordinate wWidth to copy hHeight to copy sxSource X offset sySource Y offset 

---

