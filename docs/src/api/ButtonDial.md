---
id: ButtonDial
title: ButtonDial
sidebar_label: ButtonDial
---

# ButtonDial

Circular button dial component for discrete parameter selection. 


A drawable dial component with multiple buttons arranged in a circle. Used for selecting discrete values or modes. 

---

**Namespace:** enjin2

**Header:** include/enjin2/components/button_dial.hpp

## Public Methods

### `cpp
* ButtonDial(Object *owner, uint8_t outerRadius, uint8_t innerRadius, uint8_t buttonCount)*
``

Construct a new  component. ButtonDialclassenjin2_1_1ButtonDialcompound

paramownerclassenjin2_1_1Component_1a3349b9210509e148f9f7625f6a39b022memberThe object that owns this component outerRadiusOuter radius of the dial innerRadiusInner radius of the dial buttonCountNumber of buttons around the circumference 

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

Draw the button dial to the canvas. 

paramcanvasThe canvas to draw to 

---

### `cpp
*void setSelectedButton(int id)*
``

Set the selected button. 

paramidButton ID (0 to buttonCount-1, -1 for no selection) 

---

### `cpp
*int getSelectedButton() const const*
``

Get the currently selected button. 

returnButton ID (-1 if no selection) 

---

### `cpp
*void setColor(uint8_t newColor)*
``

Set the dial color. 

paramnewColorColor value (0-15 for 4-bit grayscale) 

---

### `cpp
*uint8_t getButtonCount() const const*
``

Get the number of buttons. 

returnNumber of buttons around the circumference 

---

