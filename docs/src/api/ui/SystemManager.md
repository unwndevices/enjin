---
id: SystemManager
title: SystemManager
sidebar_label: SystemManager
---

# SystemManager

 manager for organizing and updating systems. Systemclassenjin2_1_1Systemcompound



Manages system lifecycle and provides ordered updating. Systems are automatically sorted by priority. templateparamMAX_SYSTEMSMaximum number of systems

---

**Namespace:** `enjin2`

**Header:** `include/enjin2/ui/system.hpp`

## Public Methods

### `` SystemManager()``

Constructor initializes empty system manager. 


        

---

### ``bool addSystem(T *system)``

Add system to manager. 

templateparamT type Systemclassenjin2_1_1Systemcompoundparamsystem instance Systemclassenjin2_1_1Systemcompoundreturntrue if system was added successfully 

---

### ``T * removeSystem()``

Remove system from manager. 

templateparamT type Systemclassenjin2_1_1SystemcompoundreturnPointer to removed system, nullptr if not found 

---

### ``T * getSystem()``

Get system by type. 

templateparamT type Systemclassenjin2_1_1SystemcompoundreturnPointer to system, nullptr if not found 

---

### ``void update(float deltaTime)``

Update all systems in priority order. 

paramdeltaTimeTime since last update 

---

### ``size_t getSystemCount() const const``

Get number of active systems. 

return count Systemclassenjin2_1_1Systemcompound

---

## Private Methods

### ``void sortSystems()``

Sort systems by priority (bubble sort for small arrays). 


        

---

