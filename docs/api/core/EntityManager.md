---
id: EntityManager
title: EntityManager
sidebar_label: EntityManager
---

# EntityManager

 manager for creating and destroying entities. Entity


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

 handle, invalid if no slots available Entity

---

### `void destroyEntity(Entity entity)`

Destroy entity and free its slot. 

entity to destroy Entity

---

### `bool isValid(Entity entity) const const`

Check if entity is valid. 

entity to validate Entitytrue if entity is valid 

---

### `size_t getEntityCount() const const`

Get total number of active entities. 

 count Entity

---

### `size_t getMaxEntities() const const`

Get maximum entity capacity. 

Maximum entities 

---

