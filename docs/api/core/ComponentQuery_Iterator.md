---
id: ComponentQuery::Iterator
title: ComponentQuery::Iterator
sidebar_label: ComponentQuery::Iterator
slug: ComponentQuery_Iterator
---

# ComponentQuery::Iterator

 for query results. Iteratorclassenjin2_1_1ComponentQuery_1_1Iteratorcompound



    

---

**Namespace:** `enjin2`

**Header:** `include/enjin2/ui/system.hpp`

## Public Methods

### ` Iterator(std::function&lt; bool(Entity)&gt; f, Entity start, size_t index)`

Construct iterator with filter and starting position. 

paramf filter function Entitystructenjin2_1_1EntitycompoundstartStarting entity indexStarting entity index 

---

### `Entitystructenjin2_1_1Entitycompound operator*() const const`

Dereference to get current entity. 

returnCurrent entity 

---

### ` &Iteratorclassenjin2_1_1ComponentQuery_1_1Iterator_1a5e3129905c0b001f80c94fb7cbf49707member operator++()`

Advance to next matching entity. 

returnReference to this iterator 

---

### `bool operator!=(const Iterator &other) const const`

Inequality comparison. 

paramother to compare with Iteratorclassenjin2_1_1ComponentQuery_1_1IteratorcompoundreturnTrue if iterators are at different positions 

---

## Private Methods

### `void findNext()`


        


        

---

