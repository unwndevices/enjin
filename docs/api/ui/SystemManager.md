---
id: SystemManager
title: SystemManager
sidebar_label: SystemManager
---

# SystemManager

System manager for organizing and updating systems. 


MAX_SYSTEMSMaximum number of systems
Manages system lifecycle and provides ordered updating. Systems are automatically sorted by priority. 

---

**Namespace:** `enjin2`

**Header:** `include/enjin2/ui/system.hpp`

## Public Methods

### ` SystemManager()`

Constructor initializes empty system manager. 

---

### `bool addSystem(T *system)`

Add system to manager. 

TSystem type systemSystem instance true if system was added successfully 

---

### `T * removeSystem()`

Remove system from manager. 

TSystem type Pointer to removed system, nullptr if not found 

---

### `T * getSystem()`

Get system by type. 

TSystem type Pointer to system, nullptr if not found 

---

### `void update(float deltaTime)`

Update all systems in priority order. 

deltaTimeTime since last update 

---

### `size_t getSystemCount() const`

Get number of active systems. 

System count 

---

## Private Methods

### `void sortSystems()`

Sort systems by priority (bubble sort for small arrays). 

---

