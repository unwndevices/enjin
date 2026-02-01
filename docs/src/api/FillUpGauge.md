---
id: FillUpGauge
title: FillUpGauge
sidebar_label: FillUpGauge
---

# FillUpGauge

Fill-up gauge component for level display. 


A visual gauge that fills up to represent a value, similar to a VU meter. Supports both unidirectional (0-1) and bidirectional (-1 to 1) modes. 

---

**Namespace:** enjin2

**Header:** include/enjin2/components/fill_up_gauge.hpp

## Public Methods

### `cpp
* FillUpGauge(Object *owner, uint16_t w, uint16_t h, uint16_t gaugeColor, GaugeMode gaugeMode)*
``

Construct a new  component. FillUpGaugeclassenjin2_1_1FillUpGaugecompound

paramownerclassenjin2_1_1Component_1a3349b9210509e148f9f7625f6a39b022memberThe object that owns this component wWidth of the gauge in pixels hHeight of the gauge in pixels gaugeColorColor for the gauge outline and indicator gaugeModeMode of operation (unidirectional or bidirectional) 

---

### `cpp
*void onCreate() override*
``


        


        

---

### `cpp
*void onUpdate(float deltaTime) override*
``


        


        

---

### `cpp
*void draw(ICanvas&lt; uint8_t &gt; &canvas)*
``

Draw the gauge to the canvas. 

paramcanvasThe canvas to draw to 

---

### `cpp
*void setValue(float value)*
``

Set the gauge value. 

paramvalueValue to display (0.0-1.0 for unidirectional, -1.0-1.0 for bidirectional) 

---

### `cpp
*float getValue() const const*
``

Get the current gauge value. 

returnCurrent value 

---

### `cpp
*void setMode(GaugeMode newMode)*
``

Set the gauge mode. 

paramnewModeNew mode (unidirectional or bidirectional) 

---

### `cpp
*GaugeMode getMode() const const*
``

Get the current gauge mode. 

returnCurrent mode 

---

### `cpp
*void setColor(uint16_t newColor)*
``

Set the gauge color. 

paramnewColorColor value (0-15 for 4-bit grayscale) 

---

## Private Methods

### `cpp
*void drawUnidirectional()*
``

Draw unidirectional gauge (fills from bottom). 


        

---

### `cpp
*void drawBidirectional()*
``

Draw bidirectional gauge (fills from center). 


        

---

### `cpp
*void drawPatternRect(int x, int y, int w, int h)*
``

Draw a rectangle with dither pattern. 

paramxX coordinate yY coordinate wWidth hHeight 

---

