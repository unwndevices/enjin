---
id: DrawingHelpers
title: DrawingHelpers
sidebar_label: DrawingHelpers
---

# DrawingHelpers

Drawing helper utilities for advanced shapes and effects. 


Provides optimized drawing functions for complex shapes that aren't covered by the basic canvas primitives. 

---

**Namespace:** `enjin2::DrawingHelpers`

**Header:** `include/enjin2/utils/drawing_helpers.hpp`

## Functions

### `void drawCircleStroke(ICanvas< uint8_t > &canvas, int16_t x0, int16_t y0, int16_t radius, uint8_t color, uint8_t strokeWidth)`

Draw a circle with a stroke of specified width (matches original Enjin). 

paramcanvasThe canvas to draw on x0The x-coordinate of the center of the circle y0The y-coordinate of the center of the circle radiusThe radius of the circle colorThe color of the stroke strokeWidthThe width of the stroke (will be rounded down to nearest odd number) 

---

### `void drawArcStroke(ICanvas< uint8_t > &canvas, int16_t x0, int16_t y0, int16_t radius, float startAngle, float endAngle, uint8_t color, uint8_t strokeWidth)`

Draw an arc with stroke. 

paramcanvasThe canvas to draw on x0Center X coordinate y0Center Y coordinate radiusArc radius startAngleStart angle in radians endAngleEnd angle in radians colorStroke color strokeWidthStroke width 

---

### `void drawPolygon(ICanvas< uint8_t > &canvas, const Point *points, uint8_t numPoints, uint8_t color, bool filled=false)`

Draw a polygon from a set of points. 

paramcanvasThe canvas to draw on pointsArray of points defining the polygon numPointsNumber of points in the array colorLine color filledWhether to fill the polygon 

---

### `void drawRoundedRect(ICanvas< uint8_t > &canvas, int16_t x, int16_t y, uint16_t width, uint16_t height, uint8_t radius, uint8_t color, bool filled=false)`

Draw a rounded rectangle. 

paramcanvasThe canvas to draw on xX coordinate of top-left corner yY coordinate of top-left corner widthRectangle width heightRectangle height radiusCorner radius colorDraw color filledWhether to fill the rectangle 

---

### `void drawThickLine(ICanvas< uint8_t > &canvas, int16_t x0, int16_t y0, int16_t x1, int16_t y1, uint8_t color, uint8_t thickness)`

Draw a thick line with rounded end caps. 

paramcanvasThe canvas to draw on x0Start X coordinate y0Start Y coordinate x1End X coordinate y1End Y coordinate colorLine color thicknessLine thickness 

---

### `void drawBezierCurve(ICanvas< uint8_t > &canvas, int16_t x0, int16_t y0, int16_t x1, int16_t y1, int16_t x2, int16_t y2, int16_t x3, int16_t y3, uint8_t color, uint8_t segments=20)`

Draw a bezier curve. 

paramcanvasThe canvas to draw on x0Start point X y0Start point Y x1Control point 1 X y1Control point 1 Y x2Control point 2 X y2Control point 2 Y x3End point X y3End point Y colorLine color segmentsNumber of line segments to approximate the curve 

---

### `void drawStar(ICanvas< uint8_t > &canvas, int16_t centerX, int16_t centerY, uint8_t outerRadius, uint8_t innerRadius, uint8_t numPoints, uint8_t color, bool filled=false)`

Draw a star shape. 

paramcanvasThe canvas to draw on centerXCenter X coordinate centerYCenter Y coordinate outerRadiusOuter radius of star points innerRadiusInner radius between points numPointsNumber of star points colorDraw color filledWhether to fill the star 

---

