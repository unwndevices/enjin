---
id: ButtonDial
title: ButtonDial
sidebar_label: ButtonDial
---

# ButtonDial

Circular button dial component for discrete parameter selection. 


A drawable dial component with multiple buttons arranged in a circle. Used for selecting discrete values or modes. 

---

**Namespace:** `enjin2`

**Header:** `include/enjin2/components/button_dial.hpp`

## Public Methods

### ` ButtonDial(Object *owner, uint8_t outerRadius, uint8_t innerRadius, uint8_t buttonCount)`

Construct a new ButtonDial component. 

ownerThe object that owns this component outerRadiusOuter radius of the dial innerRadiusInner radius of the dial buttonCountNumber of buttons around the circumference 

---

### `void onCreate() override`

Initialize the button dial. 

---

### `void onUpdate(float deltaTime) override`

Update the button dial state. 

deltaTimeTime elapsed since last update in seconds 

---

### `void draw(ICanvas&lt; uint8_t &gt; &canvas)`

Draw the button dial to the canvas. 

canvasThe canvas to draw to 

---

### `void setSelectedButton(int id)`

Set the selected button. 

idButton ID (0 to buttonCount-1, -1 for no selection) 

---

### `int getSelectedButton() const`

Get the currently selected button. 

Button ID (-1 if no selection) 

---

### `void setColor(uint8_t newColor)`

Set the dial color. 

newColorColor value (0-15 for 4-bit grayscale) 

---

### `uint8_t getButtonCount() const`

Get the number of buttons. 

Number of buttons around the circumference 

---

