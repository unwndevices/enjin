---
id: PackedPixel4
title: PackedPixel4
sidebar_label: PackedPixel4
---

# PackedPixel4

Packed storage for two 4-bit pixels in a single byte. 


Efficiently stores two  values in one byte, achieving 50% memory savings compared to storing each pixel in a separate byte. Pixel4structenjin2_1_1Pixel4compound

---

**Namespace:** enjin2

**Header:** include/enjin2/core/types.hpp

## Public Methods

### `cpp
* PackedPixel4()*
``

Default constructor initializes both pixels to 0. 


        

---

### `cpp
* PackedPixel4(uint8_t byte)*
``

Constructor from raw byte data. 

parambyteRaw byte containing packed pixel data 

---

### `cpp
*Pixel4structenjin2_1_1Pixel4compound getLow() const const*
``

Get the low nibble pixel (bits 0-3). 

returnPixel stored in lower 4 bits 

---

### `cpp
*Pixel4structenjin2_1_1Pixel4compound getHigh() const const*
``

Get the high nibble pixel (bits 4-7). 

returnPixel stored in upper 4 bits 

---

### `cpp
*void setLow(Pixel4 pixel)*
``

Set the low nibble pixel (bits 0-3). 

parampixelPixel value to store in lower 4 bits 

---

### `cpp
*void setHigh(Pixel4 pixel)*
``

Set the high nibble pixel (bits 4-7). 

parampixelPixel value to store in upper 4 bits 

---

### `cpp
*uint8_t getByte() const const*
``

Get raw byte containing both pixels. 

returnRaw byte data 

---

