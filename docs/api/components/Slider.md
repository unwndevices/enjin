---
id: Slider
title: Slider
sidebar_label: Slider
---

# Slider

Linear slider component for parameter control. 


A drawable slider component that displays a linear slider with a filled portion indicating the current value. Can be used for controlling continuous parameters. 

---

**Namespace:** `enjin2`

**Header:** `include/enjin2/components/slider.hpp`

## Public Methods

### ` Slider(Object *owner, uint8_t width, uint8_t height)`

Construct a new Slider component. 

ownerThe object that owns this component widthWidth of the slider in pixels heightHeight of the slider in pixels 

---

### `void onCreate() override`

Initialize the slider. 

---

### `void onUpdate(float deltaTime) override`

Update the slider state. 

deltaTimeTime elapsed since last update in seconds 

---

### `void draw(ICanvas&lt; uint8_t &gt; &canvas)`

Draw the slider to the canvas. 

canvasThe canvas to draw to 

---

### `void setValue(float newValue)`

Set the slider value. 

newValueValue between 0.0 and 1.0 

---

### `float getValue() const`

Get the current slider value. 

Value between 0.0 and 1.0 

---

### `void setColor(uint8_t newColor)`

Set the slider color. 

newColorColor value (0-15 for 4-bit grayscale) 

---

