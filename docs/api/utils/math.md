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

Integer square root via Newton's method. 

nValue to take the square root of Floored integer square root 

---

### `constexpr T abs(T value)`

Absolute value. 

valueInput value Non-negative absolute value 

---

### `constexpr T clamp(T value, T min_val, T max_val)`

Clamp value to [min_val, max_val]. 

valueValue to clamp min_valLower bound max_valUpper bound Clamped value 

---

### `constexpr T lerp(T a, T b, float t)`

Linear interpolation between a and b. 

aStart value bEnd value tInterpolation factor (0.0 = a, 1.0 = b) Interpolated value 

---

### `constexpr T map(T value, T in_min, T in_max, T out_min, T out_max)`

Re-map a value from one range to another. 

valueInput value in_minInput range lower bound in_maxInput range upper bound out_minOutput range lower bound out_maxOutput range upper bound Value mapped to output range 

---

### `constexpr T sign(T value)`

Returns -1, 0, or 1 depending on the sign of value. 

valueInput value Sign of the value 

---

### `float smoothstep(float edge0, float edge1, float x)`

Hermite smoothstep (clamped, order-3). 

edge0Lower edge edge1Upper edge xInput value Smooth interpolation result in [0, 1] 

---

### `uint16_t distance(int16_t x1, int16_t y1, int16_t x2, int16_t y2)`

Integer Euclidean distance between two points. 

x1First point X y1First point Y x2Second point X y2Second point Y Distance (integer square root) 

---

