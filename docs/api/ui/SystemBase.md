---
id: SystemBase
title: SystemBase
sidebar_label: SystemBase
---

# SystemBase

Base class for all systems in the ECS architecture. 


Systems contain the logic and operate on components. They are stateless and process entities that have required components. 

---

**Namespace:** `enjin2`

**Header:** `include/enjin2/ui/system.hpp`

## Public Methods

### `virtual  ~SystemBase()=default`

Virtual destructor. 


        

---

### `void update(float deltaTime)=0`

Update system with delta time. 

deltaTimeTime since last update in seconds 

---

### `virtual int getPriority() const const`

Get system priority for update ordering. 

Priority value (lower = earlier execution) 

---

### `SystemID getSystemID() const =0 const`

Get unique system ID. 

 identifier System

---

## Protected Methods

### `static SystemID getSystemTypeID()`

Generate unique system type ID. 

T type SystemUnique ID for system type T 

---

