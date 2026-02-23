---
id: ComponentStorage::Iterator
title: ComponentStorage::Iterator
sidebar_label: ComponentStorage::Iterator
slug: ComponentStorage_Iterator
---

# ComponentStorage::Iterator

 for efficient component iteration. Iterator



    

---

**Namespace:** `enjin2`

**Header:** `include/enjin2/ui/component.hpp`

## Public Methods

### ` Iterator(const ComponentStorage *s, size_t i)`

Construct iterator at position. 

sStorage to iterate iStarting index 

---

### `std::pair&lt; , T * &gt;Entity operator*() const const`

Dereference iterator. 

Pair of entity and component pointer 

---

### ` &Iterator operator++()`

Advance iterator. 

Reference to this iterator 

---

### `bool operator!=(const Iterator &other) const const`

Inequality comparison. 

other to compare with Iteratortrue if iterators differ 

---

