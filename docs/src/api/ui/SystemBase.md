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

paramdeltaTimeTime since last update in seconds 

---

### `virtual int getPriority() const const`

Get system priority for update ordering. 

returnPriority value (lower = earlier execution) 

---

### `SystemID getSystemID() const =0 const`

Get unique system ID. 

return identifier Systemclassenjin2_1_1Systemcompound

---

## Protected Methods

### `static SystemID getSystemTypeID()`

Generate unique system type ID. 

templateparamT type Systemclassenjin2_1_1SystemcompoundreturnUnique ID for system type T 

---

