---
id: StaticPool
title: StaticPool
sidebar_label: StaticPool
---

# StaticPool

Static memory pool for embedded systems. 


Pre-allocates storage for a fixed number of objects of type T. All allocations come from the pre-allocated pool, avoiding dynamic memory.TType of objects to allocate CAPACITYMaximum number of objects that can be allocated 

---

**Namespace:** `enjin2`

**Header:** `include/enjin2/core/memory.hpp`

## Public Methods

### ` StaticPool()`

Constructor initializes empty pool. 

---

### `T * allocate()`

Allocate memory for an object of type T. 

Pointer to allocated memory, or nullptr if pool is exhausted 

---

### `void deallocate(T *ptr)`

Deallocate memory and call object destructor. 

ptrPointer to object to deallocate (may be nullptr) 

---

### `size_t size() const`

Get current number of allocated objects. 

Current allocation count 

---

### `size_t capacity() const`

Get maximum capacity of the pool. 

Total capacity (CAPACITY template parameter) 

---

### `bool empty() const`

Check if pool has no allocations. 

true if pool is empty, false otherwise 

---

### `bool full() const`

Check if pool is at maximum capacity. 

true if pool is full, false otherwise 

---

