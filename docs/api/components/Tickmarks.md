---
id: Tickmarks
title: Tickmarks
sidebar_label: Tickmarks
---

# Tickmarks

Tickmarks component for drawing measurement scales. 


A component that draws tickmarks around a circular arc, useful for creating dial scales, meters, and other measurement indicators. 

---

**Namespace:** `enjin2`

**Header:** `include/enjin2/components/tickmarks.hpp`

## Public Methods

### ` Tickmarks(Object *owner, Vector2 centerPoint, int16_t startAngle, int16_t stopAngle, uint8_t tickSpacing, uint8_t tickLength, uint8_t arcRadius)`

Construct a new Tickmarks component. 

ownerThe object that owns this component centerPointCenter point for the tickmarks arc startAngleStarting angle in degrees stopAngleEnding angle in degrees tickSpacingSpacing between tickmarks in degrees tickLengthLength of the tickmarks in pixels arcRadiusRadius of the arc on which tickmarks are drawn 

---

### `void onCreate() override`

Initialize the tickmarks. 

---

### `void onUpdate(float deltaTime) override`

Update the tickmarks state. 

deltaTimeTime elapsed since last update in seconds 

---

### `void draw(ICanvas&lt; uint8_t &gt; &canvas)`

Draw the tickmarks to the canvas. 

canvasThe canvas to draw to 

---

### `void setValue(float value)`

Set the current value for tickmark positioning. 

valueValue that affects tickmark positioning (typically 0.0-1.0) 

---

### `float getValue() const`

Get the current value. 

Current value 

---

### `void setCenter(Vector2 newCenter)`

Set the center point of the tickmarks. 

newCenterNew center point 

---

### `void setAngleRange(int16_t startAngle, int16_t stopAngle)`

Set the angle range for the tickmarks. 

startAngleStarting angle in degrees stopAngleEnding angle in degrees 

---

### `void setSpacing(uint8_t newSpacing)`

Set the tickmark spacing. 

newSpacingSpacing between tickmarks in degrees 

---

### `void setLength(uint8_t newLength)`

Set the tickmark length. 

newLengthLength of the tickmarks in pixels 

---

### `void setRadius(uint8_t newRadius)`

Set the arc radius. 

newRadiusRadius of the arc on which tickmarks are drawn 

---

