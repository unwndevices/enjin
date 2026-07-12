---
id: InputSystem
title: InputSystem
sidebar_label: InputSystem
---

# InputSystem

Input system for handling user interaction. 


Processes input events and updates InputComponent states. Handles hit testing against entity bounds. 

---

**Namespace:** `enjin2`

**Header:** `include/enjin2/ui/systems.hpp`

## Public Methods

### ` InputSystem()`

Constructor initializes input state. 

---

### `virtual void update(float dt) override`

Update input processing. 

dtTime since last update in seconds 

---

### `void onMouseMove(Point pos)`

Handle mouse move event. 

posNew mouse position 

---

### `void onMousePress(Point pos)`

Handle mouse button press. 

posMouse position 

---

### `void onMouseRelease(Point pos)`

Handle mouse button release. 

posMouse position 

---

### `virtual int getPriority() const override const`

Get system priority (input should run first). 

Priority value 

---

