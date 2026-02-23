---
id: math
title: math
sidebar_label: math
---

# math

Fast integer math utilities namespace. 



    

---

**Namespace:** `enjin2::math`

**Header:** `include/enjin2/core/math.hpp`

## Functions

### `uint16_t isqrt(uint32_t n)`

Fast integer square root using Newton's method. 

nNon-negative integer to compute square root of Integer square root of n 

---

### `constexpr T abs(T value)`

Fast absolute value for signed types. 

TSigned numeric type valueValue to get absolute value of Absolute value of input 

---

### `constexpr T clamp(T value, T min_val, T max_val)`

Clamp value to specified range. 

TNumeric type valueValue to clamp min_valMinimum allowed value max_valMaximum allowed value Clamped value within [min_val, max_val] 

---

### `constexpr T lerp(T a, T b, float t)`

Linear interpolation between two values. 

TNumeric type aStart value (when t=0) bEnd value (when t=1) tInterpolation factor (0-1) Interpolated value between a and b 

---

### `constexpr T map(T value, T in_min, T in_max, T out_min, T out_max)`

Map value from one range to another. 

TNumeric type valueInput value to map in_minMinimum of input range in_maxMaximum of input range out_minMinimum of output range out_maxMaximum of output range Value mapped from input range to output range 

---

### `uint16_t distance(int16_t x1, int16_t y1, int16_t x2, int16_t y2)`

Calculate Euclidean distance between two points. 

x1X coordinate of first point y1Y coordinate of first point x2X coordinate of second point y2Y coordinate of second point Euclidean distance between points 

---

