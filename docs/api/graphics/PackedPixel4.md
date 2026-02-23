---
id: PackedPixel4
title: PackedPixel4
sidebar_label: PackedPixel4
---

# PackedPixel4

Packed storage for two 4-bit pixels in a single byte. 


Efficiently stores two  values in one byte, achieving 50% memory savings compared to storing each pixel in a separate byte. Pixel4

---

**Namespace:** `enjin2`

**Header:** `include/enjin2/core/types.hpp`

## Public Methods

### ` PackedPixel4()`

Default constructor initializes both pixels to 0. 


        

---

### ` PackedPixel4(uint8_t byte)`

Constructor from raw byte data. 

byteRaw byte containing packed pixel data 

---

### `Pixel4 getLow() const const`

Get the low nibble pixel (bits 0-3). 

Pixel stored in lower 4 bits 

---

### `Pixel4 getHigh() const const`

Get the high nibble pixel (bits 4-7). 

Pixel stored in upper 4 bits 

---

### `void setLow(Pixel4 pixel)`

Set the low nibble pixel (bits 0-3). 

pixelPixel value to store in lower 4 bits 

---

### `void setHigh(Pixel4 pixel)`

Set the high nibble pixel (bits 4-7). 

pixelPixel value to store in upper 4 bits 

---

### `uint8_t getByte() const const`

Get raw byte containing both pixels. 

Raw byte data 

---

