---
id: FillUpGauge
title: FillUpGauge
sidebar_label: FillUpGauge
---

# FillUpGauge

Fill-up gauge component for level display. 


A visual gauge that fills up to represent a value, similar to a VU meter. Supports both unidirectional (0-1) and bidirectional (-1 to 1) modes. 

---

**Namespace:** `enjin2`

**Header:** `include/enjin2/components/fill_up_gauge.hpp`

## Public Methods

### ` FillUpGauge(Object *owner, uint16_t w, uint16_t h, uint16_t gaugeColor, GaugeMode gaugeMode)`

Construct a new FillUpGauge component. 

ownerThe object that owns this component wWidth of the gauge in pixels hHeight of the gauge in pixels gaugeColorColor for the gauge outline and indicator gaugeModeMode of operation (unidirectional or bidirectional) 

---

### `void onCreate() override`

Initialize the gauge. 

---

### `void onUpdate(float dt) override`

Update the gauge state. 

dtTime elapsed since last update in seconds 

---

### `void draw(ICanvas&lt; uint8_t &gt; &canvas)`

Draw the gauge to the canvas. 

canvasThe canvas to draw to 

---

### `void setValue(float value)`

Set the gauge value. 

valueValue to display (0.0-1.0 for unidirectional, -1.0-1.0 for bidirectional) 

---

### `float getValue() const`

Get the current gauge value. 

Current value 

---

### `void setMode(GaugeMode newMode)`

Set the gauge mode. 

newModeNew mode (unidirectional or bidirectional) 

---

### `GaugeMode getMode() const`

Get the current gauge mode. 

Current mode 

---

### `void setColor(uint16_t newColor)`

Set the gauge color. 

newColorColor value (0-15 for 4-bit grayscale) 

---

## Private Methods

### `void drawUnidirectional()`

Draw unidirectional gauge (fills from bottom). 

---

### `void drawBidirectional()`

Draw bidirectional gauge (fills from center). 

---

### `void drawPatternRect(int x, int y, int w, int h)`

Draw a rectangle with dither pattern. 

xX coordinate yY coordinate wWidth hHeight 

---

