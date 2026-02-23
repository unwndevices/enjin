---
id: ComponentQuery::Iterator
title: ComponentQuery::Iterator
sidebar_label: ComponentQuery::Iterator
slug: ComponentQuery_Iterator
---

# ComponentQuery::Iterator

 for query results. Iterator



    

---

**Namespace:** `enjin2`

**Header:** `include/enjin2/ui/system.hpp`

## Public Methods

### ` Iterator(std::function&lt; bool(Entity)&gt; f, Entity start, size_t index)`

Construct iterator with filter and starting position. 

f filter function EntitystartStarting entity indexStarting entity index 

---

### `Entity operator*() const const`

Dereference to get current entity. 

Current entity 

---

### ` &Iterator operator++()`

Advance to next matching entity. 

Reference to this iterator 

---

### `bool operator!=(const Iterator &other) const const`

Inequality comparison. 

other to compare with IteratorTrue if iterators are at different positions 

---

## Private Methods

### `void findNext()`


        


        

---

