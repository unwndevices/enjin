---
id: Primitives
title: Primitives
sidebar_label: Primitives
---

# Primitives

Drawing primitives for geometric shapes. 


templateparamTPixelPixel type (e.g., , uint8_t) Pixel4structenjin2_1_1Pixel4compound

---

**Namespace:** `enjin2`

**Header:** `include/enjin2/graphics/primitives.hpp`

## Public Methods

### `static void drawLine(ICanvas&lt; TPixel &gt; &canvas, int16_t x0, int16_t y0, int16_t x1, int16_t y1, TPixel color)`

Draw a line using Bresenham's algorithm. 

paramcanvasTarget canvas x0Starting X coordinate y0Starting Y coordinate x1Ending X coordinate y1Ending Y coordinate colorLine color 

---

### `static void drawRect(ICanvas&lt; TPixel &gt; &canvas, const Rect &rect, TPixel color)`


        


        

---

### `static void fillRect(ICanvas&lt; TPixel &gt; &canvas, const Rect &rect, TPixel color)`


        


        

---

### `static void drawCircle(ICanvas&lt; TPixel &gt; &canvas, int16_t cx, int16_t cy, int16_t radius, TPixel color)`


        


        

---

### `static void fillCircle(ICanvas&lt; TPixel &gt; &canvas, int16_t cx, int16_t cy, int16_t radius, TPixel color)`


        


        

---

### `static void drawTriangle(ICanvas&lt; TPixel &gt; &canvas, int16_t x0, int16_t y0, int16_t x1, int16_t y1, int16_t x2, int16_t y2, TPixel color)`


        


        

---

### `static void fillTriangle(ICanvas&lt; TPixel &gt; &canvas, int16_t x0, int16_t y0, int16_t x1, int16_t y1, int16_t x2, int16_t y2, TPixel color)`


        


        

---

### `static void drawEllipse(ICanvas&lt; TPixel &gt; &canvas, int16_t cx, int16_t cy, int16_t rx, int16_t ry, TPixel color)`


        


        

---

### `static void drawArc(ICanvas&lt; TPixel &gt; &canvas, int16_t cx, int16_t cy, int16_t radius, float start_angle, float end_angle, TPixel color)`


        


        

---

### `static void drawPolygon(ICanvas&lt; TPixel &gt; &canvas, const Point *vertices, size_t vertex_count, TPixel color)`


        


        

---

