---
id: ComponentStorage::Iterator
title: ComponentStorage::Iterator
sidebar_label: ComponentStorage::Iterator
slug: ComponentStorage_Iterator
---

# ComponentStorage::Iterator

Iterator for efficient component iteration. 



---

**Namespace:** `enjin2`

**Header:** `include/enjin2/ui/component.hpp`

## Public Methods

### ` Iterator(const ComponentStorage *s, size_t i)`

Construct iterator at position. 

sStorage to iterate iStarting index 

---

### `std::pair&lt; Entity, T * &gt; operator*() const`

Dereference iterator. 

Pair of entity and component pointer 

---

### `Iterator & operator++()`

Advance iterator. 

Reference to this iterator 

---

### `bool operator!=(const Iterator &other) const`

Inequality comparison. 

otherIterator to compare with true if iterators differ 

---

