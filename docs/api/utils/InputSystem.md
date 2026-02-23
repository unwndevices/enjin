---
id: InputSystem
title: InputSystem
sidebar_label: InputSystem
---

# InputSystem

Input system for handling user interaction. 


Processes input events and updates  states. Handles hit testing against entity bounds. InputComponent

---

**Namespace:** `enjin2`

**Header:** `include/enjin2/ui/systems.hpp`

## Public Methods

### ` InputSystem()`

Constructor initializes input state. 


        

---

### `virtual void update(float deltaTime) override`

Update input processing. 

deltaTimeTime since last update 

---

### `void onMouseMove(Point pos)`

 mouse move event. Handle

posNew mouse position 

---

### `void onMousePress(Point pos)`

 mouse button press. Handle

posMouse position 

---

### `void onMouseRelease(Point pos)`

 mouse button release. Handle

posMouse position 

---

### `virtual int getPriority() const override const`

Get system priority (input should run first). 

Priority value 

---

