---
id: StaticPool
title: StaticPool
sidebar_label: StaticPool
---

# StaticPool

Static memory pool for embedded systems. 


Pre-allocates storage for a fixed number of objects of type T. All allocations come from the pre-allocated pool, avoiding dynamic memory.templateparamTType of objects to allocate CAPACITYMaximum number of objects that can be allocated 

---

**Namespace:** `enjin2`

**Header:** `include/enjin2/core/memory.hpp`

## Public Methods

### ` StaticPool()`

Constructor initializes empty pool. 


        

---

### `T * allocate()`

Allocate memory for an object of type T. 

returnPointer to allocated memory, or nullptr if pool is exhausted 

---

### `void deallocate(T *ptr)`

Deallocate memory and call object destructor. 

paramptrPointer to object to deallocate (may be nullptr) 

---

### `size_t size() const const`

Get current number of allocated objects. 

returnCurrent allocation count 

---

### `size_t capacity() const const`

Get maximum capacity of the pool. 

returnTotal capacity (CAPACITY template parameter) 

---

### `bool empty() const const`

Check if pool has no allocations. 

returntrue if pool is empty, false otherwise 

---

### `bool full() const const`

Check if pool is at maximum capacity. 

returntrue if pool is full, false otherwise 

---

