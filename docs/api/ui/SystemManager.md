---
id: SystemManager
title: SystemManager
sidebar_label: SystemManager
---

# SystemManager

 manager for organizing and updating systems. System



Manages system lifecycle and provides ordered updating. Systems are automatically sorted by priority. MAX_SYSTEMSMaximum number of systems

---

**Namespace:** `enjin2`

**Header:** `include/enjin2/ui/system.hpp`

## Public Methods

### ` SystemManager()`

Constructor initializes empty system manager. 


        

---

### `bool addSystem(T *system)`

Add system to manager. 

T type Systemsystem instance Systemtrue if system was added successfully 

---

### `T * removeSystem()`

Remove system from manager. 

T type SystemPointer to removed system, nullptr if not found 

---

### `T * getSystem()`

Get system by type. 

T type SystemPointer to system, nullptr if not found 

---

### `void update(float deltaTime)`

Update all systems in priority order. 

deltaTimeTime since last update 

---

### `size_t getSystemCount() const const`

Get number of active systems. 

 count System

---

## Private Methods

### `void sortSystems()`

Sort systems by priority (bubble sort for small arrays). 


        

---

