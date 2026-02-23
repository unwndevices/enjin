---
id: HandlePool
title: HandlePool
sidebar_label: HandlePool
---

# HandlePool

Handle-based object pool with generation tracking. 


Combines  with handle-based access for safe object management. Generation counters prevent use-after-free bugs.StaticPoolTType of objects to manage CAPACITYMaximum number of objects in pool 

---

**Namespace:** `enjin2`

**Header:** `include/enjin2/core/memory.hpp`

## Public Methods

### ` HandlePool()`

Constructor initializes empty pool with generation counters. 


        

---

### `&lt; T &gt;Handle create()`

Create a new object in the pool. 

 to newly created object, or invalid handle if pool is full Handle

---

### `void destroy(Handle&lt; T &gt; handle)`

Destroy object referenced by handle. 

handle to object to destroy (invalid handles are ignored) Handle

---

### `T * get(Handle&lt; T &gt; handle)`

Get pointer to object from handle. 

handle to object HandlePointer to object, or nullptr if handle is invalid 

---

### `const T * get(Handle&lt; T &gt; handle) const const`

Get const pointer to object from handle. 

handle to object HandleConst pointer to object, or nullptr if handle is invalid 

---

### `bool isValid(Handle&lt; T &gt; handle) const const`

Check if handle is valid. 

handle to validate Handletrue if handle is valid and refers to an existing object 

---

