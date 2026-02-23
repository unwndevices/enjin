---
id: C_Satellite
title: C_Satellite
sidebar_label: C_Satellite
---

# C_Satellite

Satellite component for orbiting parameter controls. 


Represents a small satellite that orbits around a planet. Used for audio parameter visualization and control in the space-themed UI. Can represent things like filter frequency, resonance, gain, etc. 

---

**Namespace:** `enjin2`

**Header:** `include/enjin2/components/satellite.hpp`

## Public Methods

### ` C_Satellite(Object *owner, C_Planet *planet=nullptr, float radius=30.0f, float startAngle=0.0f)`

Constructor. 

ownerOwner object planetPlanet to orbit (can be nullptr) radiusOrbit radius startAngleInitial orbital angle 

---

### `virtual void update(uint16_t deltaTime) override`

Update satellite position and animation. 

deltaTimeTime elapsed since last update in milliseconds 

---

### `void draw(ICanvas&lt; Pixel4 &gt; &canvas) override`

Draw satellite to 4-bit canvas. 

canvasTarget 4-bit canvas 

---

### `virtual void draw(ICanvas&lt; uint8_t &gt; &canvas) override`

Draw satellite to 8-bit canvas. 

canvasTarget 8-bit canvas 

---

### `void setParameterValue(float value)`

Set parameter value (0.0 to 1.0). 

valueNormalized parameter value, clamped to [0, 1] 

---

### `float getParameterValue() const const`

Get parameter value. 

Normalized parameter value (0.0 to 1.0) 

---

### `void setParameterRange(float min, float max)`

Set parameter range. 

minMinimum value maxMaximum value 

---

### `float getScaledValue() const const`

Get scaled parameter value in range. 

Value scaled to [minValue, maxValue] 

---

### `void setScaledValue(float scaledValue)`

Set from scaled value. 

scaledValueValue in [minValue, maxValue] range 

---

### `void setOrbit(float radius, float speed, bool clockwiseDirection=true)`

Set orbital properties. 

radiusOrbit radius in pixels speedOrbital speed in radians per second clockwiseDirectionOrbit direction 

---

### `void setAppearance(float radius, Pixel4 color, bool showOrbitalTrail=true)`

Set visual properties. 

radiusSatellite display radius colorSatellite color showOrbitalTrailWhether to display orbital trail 

---

### `void setParameterEffects(bool radiusEffect, bool speedEffect, bool colorEffect)`

Enable parameter-based effects. 

radiusEffectControl orbit radius from parameter speedEffectControl orbit speed from parameter colorEffectControl color from parameter 

---

### `void setTrail(bool enabled, Pixel4 color=Pixel4(8))`

Set trail appearance. 

enabledWhether to show trail colorTrail color 

---

### `void setConnection(bool enabled)`

Set connection line to planet. 

enabledWhether to show connection line 

---

### `float getAngle() const const`

Get current orbital angle. 

Current angle in radians 

---

### `void setAngle(float angle)`

Set orbital angle. 

angleAngle in radians 

---

## Private Methods

### `void updateTrail(Point newPoint)`

Update trail points. 


        

---

### `void drawTrail(ICanvas&lt; PixelType &gt; &canvas)`

Draw orbital trail. 


        

---

### `void drawLine(ICanvas&lt; PixelType &gt; &canvas, Point from, Point to, PixelType color)`

Draw line between two points. 


        

---

### `void drawCircle(ICanvas&lt; PixelType &gt; &canvas, Point center, float r, PixelType color, bool filled)`

Draw filled circle. 


        

---

