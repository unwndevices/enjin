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

Construct a new  component. Sliderclassenjin2_1_1Slidercompound

paramownerclassenjin2_1_1Component_1a3349b9210509e148f9f7625f6a39b022memberThe object that owns this component widthWidth of the slider in pixels heightHeight of the slider in pixels 

---

### `void onCreate() override`


        


        

---

### `void onUpdate(float deltaTime) override`


        


        

---

### `void draw(ICanvas< uint8_t > &canvas)`

Draw the slider to the canvas. 

paramcanvasThe canvas to draw to 

---

### `void setValue(float newValue)`

Set the slider value. 

paramnewValueValue between 0.0 and 1.0 

---

### `float getValue() const const`

Get the current slider value. 

returnValue between 0.0 and 1.0 

---

### `void setColor(uint8_t newColor)`

Set the slider color. 

paramnewColorColor value (0-15 for 4-bit grayscale) 

---

