---
id: EntityManager
title: EntityManager
sidebar_label: EntityManager
---

# EntityManager

Entity manager for creating and destroying entities. 


Manages entity lifecycle and component associations. Uses generation counters to prevent accessing destroyed entities. 

---

**Namespace:** `enjin2`

**Header:** `include/enjin2/ui/system.hpp`

## Public Methods

### ` EntityManager()`

Constructor initializes entity manager. 

---

### `Entity createEntity()`

Create new entity. 

Entity handle, invalid if no slots available 

---

### `void destroyEntity(Entity entity)`

Destroy entity and free its slot. 

entityEntity to destroy 

---

### `bool isValid(Entity entity) const`

Check if entity is valid. 

entityEntity to validate true if entity is valid 

---

### `size_t getEntityCount() const`

Get total number of active entities. 

Entity count 

---

### `size_t getMaxEntities() const`

Get maximum entity capacity. 

Maximum entities 

---

