---
id: Tickmarks
title: Tickmarks
sidebar_label: Tickmarks
---

# Tickmarks

 component for drawing measurement scales. Tickmarksclassenjin2_1_1Tickmarkscompound


A component that draws tickmarks around a circular arc, useful for creating dial scales, meters, and other measurement indicators. 

---

**Namespace:** `enjin2`

**Header:** `include/enjin2/components/tickmarks.hpp`

## Public Methods

### ` Tickmarks(Object *owner, Vector2 centerPoint, int16_t startAngle, int16_t stopAngle, uint8_t tickSpacing, uint8_t tickLength, uint8_t arcRadius)`

Construct a new  component. Tickmarksclassenjin2_1_1Tickmarkscompound

paramownerclassenjin2_1_1Component_1a3349b9210509e148f9f7625f6a39b022memberThe object that owns this component centerPointCenter point for the tickmarks arc startAngleStarting angle in degrees stopAngleEnding angle in degrees tickSpacingSpacing between tickmarks in degrees tickLengthLength of the tickmarks in pixels arcRadiusRadius of the arc on which tickmarks are drawn 

---

### `void onCreate() override`


        


        

---

### `void onUpdate(float deltaTime) override`


        


        

---

### `void draw(ICanvas< uint8_t > &canvas)`

Draw the tickmarks to the canvas. 

paramcanvasThe canvas to draw to 

---

### `void setValue(float value)`

Set the current value for tickmark positioning. 

paramvalueValue that affects tickmark positioning (typically 0.0-1.0) 

---

### `float getValue() const const`

Get the current value. 

returnCurrent value 

---

### `void setCenter(Vector2 newCenter)`

Set the center point of the tickmarks. 

paramnewCenterNew center point 

---

### `void setAngleRange(int16_t startAngle, int16_t stopAngle)`

Set the angle range for the tickmarks. 

paramstartAngleStarting angle in degrees stopAngleEnding angle in degrees 

---

### `void setSpacing(uint8_t newSpacing)`

Set the tickmark spacing. 

paramnewSpacingSpacing between tickmarks in degrees 

---

### `void setLength(uint8_t newLength)`

Set the tickmark length. 

paramnewLengthLength of the tickmarks in pixels 

---

### `void setRadius(uint8_t newRadius)`

Set the arc radius. 

paramnewRadiusRadius of the arc on which tickmarks are drawn 

---

