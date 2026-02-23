---
id: ComponentStorage::Iterator
title: ComponentStorage::Iterator
sidebar_label: ComponentStorage::Iterator
---

# ComponentStorage::Iterator

 for efficient component iteration. Iteratorclassenjin2_1_1ComponentStorage_1_1Iteratorcompound



    

---

**Namespace:** `enjin2`

**Header:** `include/enjin2/ui/component.hpp`

## Public Methods

### ` Iterator(const ComponentStorage *s, size_t i)`

Construct iterator at position. 

paramsStorage to iterate iStarting index 

---

### `std::pair&lt; , T * &gt;Entitystructenjin2_1_1Entitycompound operator*() const const`

Dereference iterator. 

returnPair of entity and component pointer 

---

### ` &Iteratorclassenjin2_1_1ComponentStorage_1_1Iterator_1a56daaafb0b37279672174c24a082c172member operator++()`

Advance iterator. 

returnReference to this iterator 

---

### `bool operator!=(const Iterator &other) const const`

Inequality comparison. 

paramother to compare with Iteratorclassenjin2_1_1ComponentStorage_1_1Iteratorcompoundreturntrue if iterators differ 

---

