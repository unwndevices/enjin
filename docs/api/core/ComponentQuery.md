---
id: ComponentQuery
title: ComponentQuery
sidebar_label: ComponentQuery
---

# ComponentQuery

Query builder for component-based entity selection. 



Provides efficient iteration over entities with specific component combinations. Components... types to query forComponent

---

**Namespace:** `enjin2`

**Header:** `include/enjin2/ui/system.hpp`

## Public Methods

### ` ComponentQuery(std::function&lt; bool(Entity)&gt; entityFilter)`

Constructor with entity filter. 

entityFilterFunction to test if entity matches query 

---

### `Iterator begin() const const`

Get iterator to beginning of query results. 

 to first matching entity Iterator

---

### `Iterator end() const const`

Get iterator to end of query results. 

Past-the-end iterator 

---

