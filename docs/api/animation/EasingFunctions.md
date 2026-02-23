---
id: EasingFunctions
title: EasingFunctions
sidebar_label: EasingFunctions
---

# EasingFunctions

Easing function utilities. 



    

---

**Namespace:** `enjin2`

**Header:** `include/enjin2/animation/keyframe.hpp`

## Public Methods

### `static float ease(float t, EaseType easeType)`

Apply easing function to normalized time (0.0 to 1.0). 

tNormalized time (0.0 to 1.0) easeTypeType of easing to apply Eased value (0.0 to 1.0) 

---

### `static float lerp(float a, float b, float t)`

Linear interpolation between two values. 

aStart value bEnd value tInterpolation factor (0.0 to 1.0) Interpolated value 

---

### `static Point lerp(const Point &a, const Point &b, float t)`

Linear interpolation between two points. 

aStart point bEnd point tInterpolation factor (0.0 to 1.0) Interpolated point 

---

### `static Pixel4 lerp(const Pixel4 &a, const Pixel4 &b, float t)`

Linear interpolation between two colors. 

aStart color bEnd color tInterpolation factor (0.0 to 1.0) Interpolated color 

---

## Private Methods

### `static float clamp01(float t)`

Clamp value between 0.0 and 1.0. 


        

---

