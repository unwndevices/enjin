---
id: Primitives
title: Primitives
sidebar_label: Primitives
---

# Primitives

Drawing primitives for geometric shapes. 


TPixelPixel type (e.g., , uint8_t) Pixel4

---

**Namespace:** `enjin2`

**Header:** `include/enjin2/graphics/primitives.hpp`

## Public Methods

### `static void drawLine(ICanvas&lt; TPixel &gt; &canvas, int16_t x0, int16_t y0, int16_t x1, int16_t y1, TPixel color)`

Draw a line using Bresenham's algorithm. 

canvasTarget canvas x0Starting X coordinate y0Starting Y coordinate x1Ending X coordinate y1Ending Y coordinate colorLine color 

---

### `static void drawRect(ICanvas&lt; TPixel &gt; &canvas, const Rect &rect, TPixel color)`

Draw rectangle outline. 

canvasTarget canvas rectRectangle bounds colorOutline color 

---

### `static void fillRect(ICanvas&lt; TPixel &gt; &canvas, const Rect &rect, TPixel color)`

Fill a rectangle. 

canvasTarget canvas rectRectangle bounds colorFill color 

---

### `static void drawCircle(ICanvas&lt; TPixel &gt; &canvas, int16_t cx, int16_t cy, int16_t radius, TPixel color)`

Draw circle outline using midpoint algorithm. 

canvasTarget canvas cxCenter X coordinate cyCenter Y coordinate radiusCircle radius colorOutline color 

---

### `static void fillCircle(ICanvas&lt; TPixel &gt; &canvas, int16_t cx, int16_t cy, int16_t radius, TPixel color)`

Fill circle using scanline algorithm. 

canvasTarget canvas cxCenter X coordinate cyCenter Y coordinate radiusCircle radius colorFill color 

---

### `static void drawTriangle(ICanvas&lt; TPixel &gt; &canvas, int16_t x0, int16_t y0, int16_t x1, int16_t y1, int16_t x2, int16_t y2, TPixel color)`

Draw triangle outline. 

canvasTarget canvas x0First vertex X y0First vertex Y x1Second vertex X y1Second vertex Y x2Third vertex X y2Third vertex Y colorOutline color 

---

### `static void fillTriangle(ICanvas&lt; TPixel &gt; &canvas, int16_t x0, int16_t y0, int16_t x1, int16_t y1, int16_t x2, int16_t y2, TPixel color)`

Fill triangle using scanline algorithm. 

canvasTarget canvas x0First vertex X y0First vertex Y x1Second vertex X y1Second vertex Y x2Third vertex X y2Third vertex Y colorFill color 

---

### `static void drawEllipse(ICanvas&lt; TPixel &gt; &canvas, int16_t cx, int16_t cy, int16_t rx, int16_t ry, TPixel color)`

Draw ellipse using midpoint algorithm. 

canvasTarget canvas cxCenter X coordinate cyCenter Y coordinate rxHorizontal radius ryVertical radius colorOutline color 

---

### `static void drawArc(ICanvas&lt; TPixel &gt; &canvas, int16_t cx, int16_t cy, int16_t radius, float start_angle, float end_angle, TPixel color)`

Draw arc segment. 

canvasTarget canvas cxCenter X coordinate cyCenter Y coordinate radiusArc radius start_angleStart angle in radians end_angleEnd angle in radians colorOutline color 

---

### `static void drawPolygon(ICanvas&lt; TPixel &gt; &canvas, const Point *vertices, size_t vertex_count, TPixel color)`

Draw polygon outline. 

canvasTarget canvas verticesArray of polygon vertices vertex_countNumber of vertices colorOutline color 

---

