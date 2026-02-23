---
id: C_Planet
title: C_Planet
sidebar_label: C_Planet
---

# C_Planet

Planet component for orbital visualization. 


Represents a central planet or control hub in the space-themed UI. Can have satellites orbiting around it and supports various visual effects like pulsing, rotation, and atmospheric effects. 

---

**Namespace:** `enjin2`

**Header:** `include/enjin2/components/planet.hpp`

## Public Methods

### ` C_Planet(Object *owner, float planetRadius, Pixel4 color=Pixel4(12))`

Constructor. 

ownerOwner object planetRadiusPlanet radius in pixels colorPlanet core color 

---

### `virtual void update(uint16_t deltaTime) override`

Update planet animations. 

deltaTimeTime elapsed since last update in milliseconds 

---

### `void draw(ICanvas&lt; Pixel4 &gt; &canvas) override`

Draw planet to 4-bit canvas. 

canvasTarget 4-bit canvas 

---

### `virtual void draw(ICanvas&lt; uint8_t &gt; &canvas) override`

Draw planet to 8-bit canvas. 

canvasTarget 8-bit canvas 

---

### `void setCoreColor(Pixel4 color)`

Set planet core color. 

colorNew core color 

---

### `void setAtmosphere(bool enabled, float radiusMultiplier=1.5f, Pixel4 color=Pixel4(0))`

Set atmosphere properties. 

enabledTrue to enable atmosphere rendering radiusMultiplierAtmosphere radius as multiple of core radius colorAtmosphere color (0 = derive from core color) 

---

### `void setPulsing(bool enabled, float speed=2.0f, float amount=0.2f)`

Enable pulsing animation. 

enabledTrue to enable pulsing speedPulse frequency multiplier amountPulse intensity (0.0 to 1.0) 

---

### `void setRotation(bool enabled, float speed=0.5f)`

Enable rotation animation. 

enabledTrue to enable rotation speedRotation speed in radians per second 

---

### `void setRings(bool enabled, float innerRadius=1.3f, float outerRadius=1.7f, Pixel4 color=Pixel4(8))`

Add rings around planet. 

enabledTrue to enable rings innerRadiusInner ring radius as multiple of planet radius outerRadiusOuter ring radius as multiple of planet radius colorRing color 

---

### `float getRadius() const`

Get planet radius. 

Planet radius in pixels 

---

### `Point getCenterPosition() const`

Get planet center position in world coordinates. 

Center point of the planet 

---

## Private Methods

### `void drawCircle(ICanvas&lt; PixelType &gt; &canvas, Point center, float r, PixelType color, bool filled)`

Draw filled circle. 

---

### `void drawRing(ICanvas&lt; PixelType &gt; &canvas, Point center, float innerR, float outerR, PixelType color)`

Draw ring (hollow circle). 

---

### `void drawSurfaceDetails(ICanvas&lt; PixelType &gt; &canvas, Point center, float r)`

Draw surface details that rotate with the planet. 

---

