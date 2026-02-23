---
id: ComponentStorage
title: ComponentStorage
sidebar_label: ComponentStorage
---

# ComponentStorage

 storage using static memory pools. Component



Efficient storage for components with O(1) allocation/deallocation. Uses packed arrays for cache-friendly iteration. T type ComponentCAPACITYMaximum number of components

---

**Namespace:** `enjin2`

**Header:** `include/enjin2/ui/component.hpp`

## Public Methods

### ` ComponentStorage()`

Constructor initializes empty storage. 


        

---

### `T * addComponent(Entity entity, Args &&... args)`

Add component for entity. 

entity to add component to EntityargsConstructor arguments for component Pointer to created component, nullptr if pool full 

---

### `T * getComponent(Entity entity) const const`

Get component for entity. 

entity to get component for EntityPointer to component, nullptr if not found 

---

### `bool removeComponent(Entity entity)`

Remove component for entity. 

entity to remove component from Entitytrue if component was removed 

---

### `bool hasComponent(Entity entity) const const`

Check if entity has component. 

entity to check Entitytrue if entity has component 

---

### `size_t size() const const`

Get number of active components. 

 count Component

---

### `bool empty() const const`

Check if storage is empty. 

true if no components 

---

### `Iterator begin() const const`

Get iterator to beginning. 

 to first component Iterator

---

### `Iterator end() const const`

Get iterator to end. 

 past last component Iterator

---

