---
id: InputSystem
title: InputSystem
sidebar_label: InputSystem
---

# InputSystem

Input system for handling user interaction. 


Processes input events and updates  states. Handles hit testing against entity bounds. InputComponentstructenjin2_1_1InputComponentcompound

---

**Namespace:** `enjin2`

**Header:** `include/enjin2/ui/systems.hpp`

## Public Methods

### ` InputSystem()`

Constructor initializes input state. 


        

---

### `virtual void update(float deltaTime) override`

Update input processing. 

paramdeltaTimeTime since last update 

---

### `void onMouseMove(Point pos)`

 mouse move event. Handlestructenjin2_1_1Handlecompound

paramposNew mouse position 

---

### `void onMousePress(Point pos)`

 mouse button press. Handlestructenjin2_1_1Handlecompound

paramposMouse position 

---

### `void onMouseRelease(Point pos)`

 mouse button release. Handlestructenjin2_1_1Handlecompound

paramposMouse position 

---

### `virtual int getPriority() const override const`

Get system priority (input should run first). 

returnPriority value 

---

