---
id: math::TrigLUT
title: math::TrigLUT
sidebar_label: math::TrigLUT
slug: math_TrigLUT
---

# math::TrigLUT

Fast trigonometry using lookup table. 


Provides sine and cosine functions optimized for embedded systems. Uses 256-step lookup table for 0-2π range. 

---

**Namespace:** `enjin2`

**Header:** `include/enjin2/core/math.hpp`

## Public Methods

### `static int16_t sin(uint16_t angle)`

Fast sine function using 0-255 angle range. 

paramangleAngle value (0-255 representing 0-2π) returnSine value in range [-32767, 32767] (Q15 fixed-point) 

---

### `static int16_t cos(uint16_t angle)`

Fast cosine function using 0-255 angle range. 

paramangleAngle value (0-255 representing 0-2π) returnCosine value in range [-32767, 32767] (Q15 fixed-point) 

---

### `static uint16_t angleToIndex(float radians)`

Convert float radians to lookup table index. 

paramradiansAngle in radians returnIndex in range [0, 255] for lookup table 

---

## Private Methods

### `static int16_t getSineValue(uint8_t index)`


        


        

---

