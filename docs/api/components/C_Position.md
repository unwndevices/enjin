---
id: C_Position
title: C_Position
sidebar_label: C_Position
---

# C_Position

Position component for object positioning. 


Manages the position and anchor point of an object in 2D space. This is a fundamental component used by most drawable objects. 

---

**Namespace:** `enjin2`

**Header:** `include/enjin2/components/position.hpp`

## Public Methods

### ` C_Position(Object *owner)`

Constructor with default position. 

ownerOwner object 

---

### ` C_Position(Object *owner, int16_t x, int16_t y)`

Constructor with initial position. 

ownerOwner object xInitial X position yInitial Y position 

---

### ` C_Position(Object *owner, const Point &pos)`

Constructor with Point. 

ownerOwner object posInitial position 

---

### `void setPosition(int16_t x, int16_t y)`

Set position. 

xX coordinate yY coordinate 

---

### `void setPosition(const Point &pos)`

Set position. 

posNew position 

---

### `const Point & getPosition() const`

Get position. 

Current position 

---

### `void move(int16_t dx, int16_t dy)`

Move position by offset. 

dxX offset dyY offset 

---

### `void move(const Point &offset)`

Move position by offset. 

offsetOffset vector 

---

### `void setAnchor(Anchor anchor)`

Set anchor point. 

anchorNew anchor point 

---

### `Anchor getAnchor() const`

Get anchor point. 

Current anchor point 

---

### `void setAnchorOffset(const Point &offset)`

Set anchor offset. 

offsetOffset from anchor point 

---

### `const Point & getAnchorOffset() const`

Get anchor offset. 

Current anchor offset 

---

### `Point calculateRenderPosition(const Size &size) const`

Calculate final rendering position based on anchor and size. 

sizeSize of the object for anchor calculation Final rendering position 

---

### `void lerp(const Point &target, float t)`

Linear interpolation to target position. 

targetTarget position tInterpolation factor (0.0 to 1.0) 

---

### `float distanceTo(const Point &other) const`

Calculate distance to another position. 

otherOther position Distance in pixels 

---

### `int32_t distanceSquaredTo(const Point &other) const`

Calculate squared distance to another position (faster than distanceTo). 

otherOther position Squared distance 

---

