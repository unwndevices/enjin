---
id: ComponentStorage
title: ComponentStorage
sidebar_label: ComponentStorage
---

# ComponentStorage

 storage using static memory pools. Componentclassenjin2_1_1Componentcompound



Efficient storage for components with O(1) allocation/deallocation. Uses packed arrays for cache-friendly iteration. templateparamT type Componentclassenjin2_1_1ComponentcompoundCAPACITYMaximum number of components

---

**Namespace:** `enjin2`

**Header:** `include/enjin2/ui/component.hpp`

## Public Methods

### `` ComponentStorage()``

Constructor initializes empty storage. 


        

---

### ``T * addComponent(Entity entity, Args &&... args)``

Add component for entity. 

paramentity to add component to Entitystructenjin2_1_1EntitycompoundargsConstructor arguments for component returnPointer to created component, nullptr if pool full 

---

### ``T * getComponent(Entity entity) const const``

Get component for entity. 

paramentity to get component for Entitystructenjin2_1_1EntitycompoundreturnPointer to component, nullptr if not found 

---

### ``bool removeComponent(Entity entity)``

Remove component for entity. 

paramentity to remove component from Entitystructenjin2_1_1Entitycompoundreturntrue if component was removed 

---

### ``bool hasComponent(Entity entity) const const``

Check if entity has component. 

paramentity to check Entitystructenjin2_1_1Entitycompoundreturntrue if entity has component 

---

### ``size_t size() const const``

Get number of active components. 

return count Componentclassenjin2_1_1Componentcompound

---

### ``bool empty() const const``

Check if storage is empty. 

returntrue if no components 

---

### ``Iteratorclassenjin2_1_1ComponentStorage_1_1Iteratorcompound begin() const const``

Get iterator to beginning. 


        

---

### ``Iteratorclassenjin2_1_1ComponentStorage_1_1Iteratorcompound end() const const``

Get iterator to end. 


        

---

