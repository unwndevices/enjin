---
id: Polar
title: Polar
sidebar_label: Polar
---

# Polar

Polar coordinate utilities (matches original Enjin Polar utilities). 


Provides functions for converting between polar and cartesian coordinates, useful for circular UI elements and orbital motion. 

---

**Namespace:** `enjin2::Polar`

**Header:** `include/enjin2/utils/polar.hpp`

## Functions

### `Point RadialToCartesian(float phase, uint8_t radius, Point center=Point(SCREEN_CENTER_X, SCREEN_CENTER_Y))`

Convert radial coordinates to cartesian coordinates (matches original Enjin). 

phasePhase value from 0.0 to 1.0 (0.0 = 0 degrees, 1.0 = 360 degrees) radiusDistance from center centerCenter point for conversion (default is 63,63 for 128x128 displays) Cartesian coordinates as Point

---

### `void CartesianToRadial(Point point, Point center, float &phase, uint8_t &radius)`

Convert cartesian coordinates to polar coordinates. 

pointCartesian point centerCenter point for conversion phaseOutput phase value (0.0 to 1.0) radiusOutput radius value 

---

### `Point GetCirclePoint(int16_t centerX, int16_t centerY, float angle, uint8_t radius)`

Get a point on a circle at specified angle and radius. 

centerXCenter X coordinate centerYCenter Y coordinate angleAngle in radians radiusDistance from center Point on the circle 

---

### `Point GetEllipsePoint(int16_t centerX, int16_t centerY, float angle, uint8_t radiusX, uint8_t radiusY)`

Get a point on an ellipse. 

centerXCenter X coordinate centerYCenter Y coordinate angleAngle in radians radiusXHorizontal radius radiusYVertical radius Point on the ellipse 

---

### `float CalculateDistance(Point p1, Point p2)`

Calculate distance between two points. 

p1First point p2Second point Distance between points 

---

### `float CalculateAngle(Point p1, Point p2)`

Calculate angle between two points. 

p1First point (usually center) p2Second point Angle in radians 

---

### `float NormalizePhase(float phase)`

Normalize phase value to 0.0-1.0 range. 

phasePhase value to normalize Normalized phase (0.0 to 1.0) 

---

### `float PhaseToRadians(float phase)`

Convert phase (0.0-1.0) to radians. 

phasePhase value Angle in radians 

---

### `float RadiansToPhase(float radians)`

Convert radians to phase (0.0-1.0). 

radiansAngle in radians Phase value 

---

