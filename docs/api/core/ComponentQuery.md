---
id: ComponentQuery
title: ComponentQuery
sidebar_label: ComponentQuery
---

# ComponentQuery

Query builder for component-based entity selection. 


Components...Component types to query for
Provides efficient iteration over entities with specific component combinations. 

---

**Namespace:** `enjin2`

**Header:** `include/enjin2/ui/system.hpp`

## Public Methods

### ` ComponentQuery(std::function&lt; bool(Entity)&gt; entityFilter)`

Constructor with entity filter. 

entityFilterFunction to test if entity matches query 

---

### `Iterator begin() const`

Get iterator to beginning of query results. 

Iterator to first matching entity 

---

### `Iterator end() const`

Get iterator to end of query results. 

Past-the-end iterator 

---

