---
id: HandlePool
title: HandlePool
sidebar_label: HandlePool
---

# HandlePool

Handle-based object pool with generation tracking. 


Combines StaticPool with handle-based access for safe object management. Generation counters prevent use-after-free bugs.TType of objects to manage CAPACITYMaximum number of objects in pool 

---

**Namespace:** `enjin2`

**Header:** `include/enjin2/core/memory.hpp`

## Public Methods

### ` HandlePool()`

Constructor initializes empty pool with generation counters. 

---

### `Handle&lt; T &gt; create()`

Create a new object in the pool. 

Handle to newly created object, or invalid handle if pool is full 

---

### `void destroy(Handle&lt; T &gt; handle)`

Destroy object referenced by handle. 

handleHandle to object to destroy (invalid handles are ignored) 

---

### `T * get(Handle&lt; T &gt; handle)`

Get pointer to object from handle. 

handleHandle to object Pointer to object, or nullptr if handle is invalid 

---

### `const T * get(Handle&lt; T &gt; handle) const`

Get const pointer to object from handle. 

handleHandle to object Const pointer to object, or nullptr if handle is invalid 

---

### `bool isValid(Handle&lt; T &gt; handle) const`

Check if handle is valid. 

handleHandle to validate true if handle is valid and refers to an existing object 

---

