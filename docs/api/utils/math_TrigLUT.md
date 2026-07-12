---
id: math::TrigLUT
title: math::TrigLUT
sidebar_label: math::TrigLUT
slug: math_TrigLUT
---

# math::TrigLUT

Fast trigonometry using lookup table. 


Uses 256-step table for 0-2pi range. Backed by a precomputed constexpr int16_t[256] sine table. Avoids std::sin/cos at runtime — suitable for embedded targets (ESP32). 

---

**Namespace:** `enjin2`

**Header:** `include/enjin2/core/math.hpp`

## Public Methods

### `static int16_t sin(uint16_t angle)`

Fixed-point sine of a 256-step angle. 

angleAngle in 256ths of a full turn (0-255, wraps) Sine value scaled to [-32767, 32767] 

---

### `static int16_t cos(uint16_t angle)`

Fixed-point cosine of a 256-step angle. 

cos(x) = sin(x + 64) (quarter-turn phase offset)angleAngle in 256ths of a full turn (0-255, wraps) Cosine value scaled to [-32767, 32767] 

---

### `static uint16_t angleToIndex(float radians)`

Convert radians to 256-step LUT index. 

radiansAngle in radians LUT index (0-255) 

---

## Private Methods

### `static int16_t getSineValue(uint8_t index)`

Look up sine value for a given 8-bit table index. 

indexTable index (0-255, wraps automatically) Sine value scaled to [-32767, 32767] 

---

