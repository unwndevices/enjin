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

paramownerclassenjin2_1_1Component_1a3349b9210509e148f9f7625f6a39b022memberOwner object 

---

### ` C_Position(Object *owner, int16_t x, int16_t y)`

Constructor with initial position. 

paramownerclassenjin2_1_1Component_1a3349b9210509e148f9f7625f6a39b022memberOwner object xInitial X position yInitial Y position 

---

### ` C_Position(Object *owner, const Point &pos)`

Constructor with . Pointstructenjin2_1_1Pointcompound

paramownerclassenjin2_1_1Component_1a3349b9210509e148f9f7625f6a39b022memberOwner object posInitial position 

---

### `void setPosition(int16_t x, int16_t y)`

Set position. 

paramxX coordinate yY coordinate 

---

### `void setPosition(const Point &pos)`

Set position. 

paramposNew position 

---

### `const  &Pointstructenjin2_1_1Pointcompound getPosition() const const`

Get position. 

returnCurrent position 

---

### `void move(int16_t dx, int16_t dy)`

Move position by offset. 

paramdxX offset dyY offset 

---

### `void move(const Point &offset)`

Move position by offset. 

paramoffsetOffset vector 

---

### `void setAnchor(Anchor anchor)`

Set anchor point. 

paramanchorNew anchor point 

---

### `Anchor getAnchor() const const`

Get anchor point. 

returnCurrent anchor point 

---

### `void setAnchorOffset(const Point &offset)`

Set anchor offset. 

paramoffsetOffset from anchor point 

---

### `const  &Pointstructenjin2_1_1Pointcompound getAnchorOffset() const const`

Get anchor offset. 

returnCurrent anchor offset 

---

### `Pointstructenjin2_1_1Pointcompound calculateRenderPosition(const Size &size) const const`

Calculate final rendering position based on anchor and size. 

paramsize of the object for anchor calculation Sizestructenjin2_1_1SizecompoundreturnFinal rendering position 

---

### `void lerp(const Point &target, float t)`

Linear interpolation to target position. 

paramtargetTarget position tInterpolation factor (0.0 to 1.0) 

---

### `float distanceTo(const Point &other) const const`

Calculate distance to another position. 

paramotherOther position returnDistance in pixels 

---

### `int32_t distanceSquaredTo(const Point &other) const const`

Calculate squared distance to another position (faster than distanceTo). 

paramotherOther position returnSquared distance 

---

