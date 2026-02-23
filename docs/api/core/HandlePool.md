---
id: HandlePool
title: HandlePool
sidebar_label: HandlePool
---

# HandlePool

Handle-based object pool with generation tracking. 


Combines  with handle-based access for safe object management. Generation counters prevent use-after-free bugs.StaticPoolclassenjin2_1_1StaticPoolcompoundtemplateparamTType of objects to manage CAPACITYMaximum number of objects in pool 

---

**Namespace:** `enjin2`

**Header:** `include/enjin2/core/memory.hpp`

## Public Methods

### ` HandlePool()`

Constructor initializes empty pool with generation counters. 


        

---

### `&lt; T &gt;Handlestructenjin2_1_1Handlecompound create()`

Create a new object in the pool. 

return to newly created object, or invalid handle if pool is full Handlestructenjin2_1_1Handlecompound

---

### `void destroy(Handle&lt; T &gt; handle)`

Destroy object referenced by handle. 

paramhandle to object to destroy (invalid handles are ignored) Handlestructenjin2_1_1Handlecompound

---

### `T * get(Handle&lt; T &gt; handle)`

Get pointer to object from handle. 

paramhandle to object Handlestructenjin2_1_1HandlecompoundreturnPointer to object, or nullptr if handle is invalid 

---

### `const T * get(Handle&lt; T &gt; handle) const const`

Get const pointer to object from handle. 

paramhandle to object Handlestructenjin2_1_1HandlecompoundreturnConst pointer to object, or nullptr if handle is invalid 

---

### `bool isValid(Handle&lt; T &gt; handle) const const`

Check if handle is valid. 

paramhandle to validate Handlestructenjin2_1_1Handlecompoundreturntrue if handle is valid and refers to an existing object 

---

