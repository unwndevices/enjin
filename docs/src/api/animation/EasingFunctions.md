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

paramtNormalized time (0.0 to 1.0) easeTypeType of easing to apply returnEased value (0.0 to 1.0) 

---

### `static float lerp(float a, float b, float t)`

Linear interpolation between two values. 

paramaStart value bEnd value tInterpolation factor (0.0 to 1.0) returnInterpolated value 

---

### `static Pointstructenjin2_1_1Pointcompound lerp(const Point &a, const Point &b, float t)`

Linear interpolation between two points. 

paramaStart point bEnd point tInterpolation factor (0.0 to 1.0) returnInterpolated point 

---

### `static Pixel4structenjin2_1_1Pixel4compound lerp(const Pixel4 &a, const Pixel4 &b, float t)`

Linear interpolation between two colors. 

paramaStart color bEnd color tInterpolation factor (0.0 to 1.0) returnInterpolated color 

---

## Private Methods

### `static float clamp01(float t)`

Clamp value between 0.0 and 1.0. 


        

---

