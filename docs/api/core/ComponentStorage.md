---
id: ComponentStorage
title: ComponentStorage
sidebar_label: ComponentStorage
---

# ComponentStorage

Component storage using static memory pools. 


TComponent type CAPACITYMaximum number of components
Efficient storage for components with O(1) allocation/deallocation. Uses packed arrays for cache-friendly iteration. 

---

**Namespace:** `enjin2`

**Header:** `include/enjin2/ui/component.hpp`

## Public Methods

### ` ComponentStorage()`

Constructor initializes empty storage. 

---

### `T * addComponent(Entity entity, Args &&... args)`

Add component for entity. 

entityEntity to add component to argsConstructor arguments for component Pointer to created component, nullptr if pool full 

---

### `T * getComponent(Entity entity) const`

Get component for entity. 

entityEntity to get component for Pointer to component, nullptr if not found 

---

### `bool removeComponent(Entity entity)`

Remove component for entity. 

entityEntity to remove component from true if component was removed 

---

### `bool hasComponent(Entity entity) const`

Check if entity has component. 

entityEntity to check true if entity has component 

---

### `size_t size() const`

Get number of active components. 

Component count 

---

### `bool empty() const`

Check if storage is empty. 

true if no components 

---

### `Iterator begin() const`

Get iterator to beginning. 

Iterator to first component 

---

### `Iterator end() const`

Get iterator to end. 

Iterator past last component 

---

