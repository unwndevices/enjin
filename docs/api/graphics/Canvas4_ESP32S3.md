---
id: Canvas4_ESP32S3
title: Canvas4_ESP32S3
sidebar_label: Canvas4_ESP32S3
---

# Canvas4_ESP32S3

ESP32-S3 optimized 4-bit canvas. 


WIDTHCanvas width in pixels HEIGHTCanvas height in pixels 

---

**Namespace:** `enjin2`

**Header:** `include/enjin2/graphics/canvas_esp32s3.hpp`

## Public Methods

### ` Canvas4_ESP32S3()`


        


        

---

### ` ~Canvas4_ESP32S3()`


        


        

---

### `IRAM_ATTR void setPixel(int16_t x, int16_t y, Pixel4 color)`

Fast pixel setting using lookup tables. 

xX coordinate yY coordinate colorPixel color 

---

### `IRAM_ATTR Pixel4 getPixel(int16_t x, int16_t y) const const`

Fast pixel reading using lookup tables. 

xX coordinate yY coordinate Pixel color at the specified location 

---

### `IRAM_ATTR void drawHorizontalLine(int16_t x1, int16_t x2, int16_t y, Pixel4 color)`

Vectorized horizontal line drawing. 

x1Start X coordinate x2End X coordinate yY coordinate colorLine color 

---

### `IRAM_ATTR void drawVerticalLine(int16_t x, int16_t y1, int16_t y2, Pixel4 color)`

Optimized vertical line drawing. 

xX coordinate y1Start Y coordinate y2End Y coordinate colorLine color 

---

### `IRAM_ATTR void fillRect(int16_t x, int16_t y, int16_t w, int16_t h, Pixel4 color)`

Fast rectangle filling using vectorized horizontal lines. 

xTop-left X coordinate yTop-left Y coordinate wWidth in pixels hHeight in pixels colorFill color 

---

### `IRAM_ATTR void fillCircle(int16_t cx, int16_t cy, int16_t radius, Pixel4 color)`

Optimized circle filling using scanline algorithm. 

cxCenter X coordinate cyCenter Y coordinate radiusCircle radius colorFill color 

---

### `IRAM_ATTR void clear(Pixel4 color)`

Fast clear using optimized memory operations. 

colorColor to fill canvas with 

---

### `const uint8_t * getData() const const`

Get raw data pointer for DMA transfers. 

Pointer to pixel buffer 

---

### `constexpr size_t getDataSize() const const`

Get data size in bytes. 

Data size 

---

## Private Methods

### `IRAM_ATTR void setPixelDirect(uint8_t *row, int16_t x, uint8_t color)`

Direct pixel setting without bounds checking. 


        

---

