---
id: EntityManager
title: EntityManager
sidebar_label: EntityManager
---

# EntityManager

 manager for creating and destroying entities. Entitystructenjin2_1_1Entitycompound


Manages entity lifecycle and component associations. Uses generation counters to prevent accessing destroyed entities. 

---

**Namespace:** `enjin2`

**Header:** `include/enjin2/ui/system.hpp`

## Public Methods

### `` EntityManager()``

Constructor initializes entity manager. 


        

---

### ``Entitystructenjin2_1_1Entitycompound createEntity()``

Create new entity. 

return handle, invalid if no slots available Entitystructenjin2_1_1Entitycompound

---

### ``void destroyEntity(Entity entity)``

Destroy entity and free its slot. 

paramentity to destroy Entitystructenjin2_1_1Entitycompound

---

### ``bool isValid(Entity entity) const const``

Check if entity is valid. 

paramentity to validate Entitystructenjin2_1_1Entitycompoundreturntrue if entity is valid 

---

### ``size_t getEntityCount() const const``

Get total number of active entities. 

return count Entitystructenjin2_1_1Entitycompound

---

### ``size_t getMaxEntities() const const``

Get maximum entity capacity. 

returnMaximum entities 

---

