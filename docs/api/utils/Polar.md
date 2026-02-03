---
id: Polar
title: Polar
sidebar_label: Polar
---

# Polar

 coordinate utilities (matches original Enjin  utilities). Polarnamespaceenjin2_1_1PolarcompoundPolarnamespaceenjin2_1_1Polarcompound


Provides functions for converting between polar and cartesian coordinates, useful for circular UI elements and orbital motion. 

---

**Namespace:** `enjin2::Polar`

**Header:** `include/enjin2/utils/polar.hpp`

## Functions

### `Pointstructenjin2_1_1Pointcompound RadialToCartesian(float phase, uint8_t radius, Point center=Point(63, 63))`

Convert radial coordinates to cartesian coordinates (matches original Enjin). 

paramphasePhase value from 0.0 to 1.0 (0.0 = 0 degrees, 1.0 = 360 degrees) radiusDistance from center centerCenter point for conversion (default is 63,63 for 128x128 displays) returnCartesian coordinates as  Pointstructenjin2_1_1Pointcompound

---

### `void CartesianToRadial(Point point, Point center, float &phase, uint8_t &radius)`

Convert cartesian coordinates to polar coordinates. 

parampointCartesian point centerCenter point for conversion phaseOutput phase value (0.0 to 1.0) radiusOutput radius value 

---

### `Pointstructenjin2_1_1Pointcompound GetCirclePoint(int16_t centerX, int16_t centerY, float angle, uint8_t radius)`

Get a point on a circle at specified angle and radius. 

paramcenterXCenter X coordinate centerYCenter Y coordinate angleAngle in radians radiusDistance from center return on the circle Pointstructenjin2_1_1Pointcompound

---

### `Pointstructenjin2_1_1Pointcompound GetEllipsePoint(int16_t centerX, int16_t centerY, float angle, uint8_t radiusX, uint8_t radiusY)`

Get a point on an ellipse. 

paramcenterXCenter X coordinate centerYCenter Y coordinate angleAngle in radians radiusXHorizontal radius radiusYVertical radius return on the ellipse Pointstructenjin2_1_1Pointcompound

---

### `float CalculateDistance(Point p1, Point p2)`

Calculate distance between two points. 

paramp1First point p2Second point returnDistance between points 

---

### `float CalculateAngle(Point p1, Point p2)`

Calculate angle between two points. 

paramp1First point (usually center) p2Second point returnAngle in radians 

---

### `float NormalizePhase(float phase)`

Normalize phase value to 0.0-1.0 range. 

paramphasePhase value to normalize returnNormalized phase (0.0 to 1.0) 

---

### `float PhaseToRadians(float phase)`

Convert phase (0.0-1.0) to radians. 

paramphasePhase value returnAngle in radians 

---

### `float RadiansToPhase(float radians)`

Convert radians to phase (0.0-1.0). 

paramradiansAngle in radians returnPhase value 

---

