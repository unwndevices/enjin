---
id: C_Probe
title: C_Probe
sidebar_label: C_Probe
---

# C_Probe

Probe component for dynamic indicators and effects. 


Represents small probes, particles, or indicators that can move freely in space, show status information, or create visual effects like particle trails, scanners, or data flows. 

---

**Namespace:** `enjin2`

**Header:** `include/enjin2/components/probe.hpp`

## Public Methods

### ` C_Probe(Object *owner, ProbeType type=ProbeType::DOT, float size=2.0f)`

Constructor. 

paramownerclassenjin2_1_1Component_1a3349b9210509e148f9f7625f6a39b022memberOwner object typeProbe visual type sizeProbe size 

---

### `virtual void update(uint16_t deltaTime) override`

Update probe movement and animation. 

paramdeltaTimeTime elapsed since last update in milliseconds 

---

### `void draw(ICanvas&lt; Pixel4 &gt; &canvas) override`

Draw probe to 4-bit canvas. 

paramcanvasTarget 4-bit canvas 

---

### `virtual void draw(ICanvas&lt; uint8_t &gt; &canvas) override`

Draw probe to 8-bit canvas. 

paramcanvasTarget 8-bit canvas 

---

### `void setAppearance(ProbeType type, float size, Pixel4 color, Pixel4 accent=Pixel4(10))`

Set probe appearance. 

paramtypeVisual type of probe sizeProbe size in pixels colorPrimary probe color accentSecondary accent color 

---

### `void setMovement(MovementPattern pattern, float speed=1.0f)`

Set movement pattern. 

parampatternMovement pattern type speedMovement speed multiplier 

---

### `void setVelocity(Point vel)`

Set linear velocity (for LINEAR movement). 

paramvelVelocity vector 

---

### `void setPulsing(bool enabled, float speed=2.0f)`

Enable pulsing effect. 

paramenabledclassenjin2_1_1Component_1ad05479377756fa07b8de93b1a4687d84memberTrue to enable pulsing speedPulse frequency multiplier 

---

### `void setTrail(bool enabled)`

Enable particle trail. 

paramenabledclassenjin2_1_1Component_1ad05479377756fa07b8de93b1a4687d84memberTrue to enable trail rendering 

---

### `void setMovementBounds(const Rect &bounds, bool constrain=true)`

Set movement bounds. 

paramboundsBounding rectangle for movement constrainTrue to constrain movement within bounds 

---

### `void setScanner(float radius, float speed=1.0f)`

Set scanner properties (for SCANNER type). 

paramradiusScan beam radius speedScan rotation speed 

---

## Private Methods

### `void updateMovement(float deltaTime)`

Update movement based on pattern. 


        

---

### `void updateTrail(Point newPoint)`

Update trail points. 


        

---

### `void drawTrail(ICanvas&lt; PixelType &gt; &canvas)`

Draw trail. 


        

---

### `void drawDot(ICanvas&lt; PixelType &gt; &canvas, Point center, float size, PixelType color)`

Draw different probe shapes. 


        

---

### `void drawDiamond(ICanvas&lt; PixelType &gt; &canvas, Point center, float size, PixelType color)`


        


        

---

### `void drawCross(ICanvas&lt; PixelType &gt; &canvas, Point center, float size, PixelType color)`


        


        

---

### `void drawTriangle(ICanvas&lt; PixelType &gt; &canvas, Point center, float size, PixelType color)`


        


        

---

### `void drawScanner(ICanvas&lt; PixelType &gt; &canvas, Point center, float size, PixelType color)`


        


        

---

### `void drawParticle(ICanvas&lt; PixelType &gt; &canvas, Point center, float size, PixelType color)`


        


        

---

### `void drawLine(ICanvas&lt; PixelType &gt; &canvas, Point from, Point to, PixelType color)`


        


        

---

