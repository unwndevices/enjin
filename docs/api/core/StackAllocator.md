---
id: StackAllocator
title: StackAllocator
sidebar_label: StackAllocator
---

# StackAllocator

Stack-based allocator for temporary allocations. 


Allocates memory linearly from a fixed buffer. All allocations can be freed at once with , ideal for frame-based allocations. reset()

---

**Namespace:** `enjin2`

**Header:** `include/enjin2/core/memory.hpp`

## Public Methods

### ` StackAllocator(uint8_t *mem, size_t cap)`

Construct stack allocator with provided buffer. 

memPointer to pre-allocated memory buffer capTotal capacity of buffer in bytes 

---

### `void * allocate(size_t size, size_t alignment=sizeof(void *))`

Allocate memory from stack. 

sizeNumber of bytes to allocate alignmentAlignment requirement in bytes (default: pointer size) Pointer to allocated memory, or nullptr if out of space 

---

### `void reset()`

Reset allocator, freeing all allocations. 


        

---

### `size_t getUsed() const const`

Get currently used memory. 

Number of bytes currently allocated 

---

### `size_t getRemaining() const const`

Get remaining free memory. 

Number of bytes available for allocation 

---

