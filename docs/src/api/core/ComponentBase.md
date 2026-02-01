---
id: ComponentBase
title: ComponentBase
sidebar_label: ComponentBase
---

# ComponentBase

Base class for all components in the ECS system. 


Components are pure data containers without behavior. This base class provides RTTI and basic lifecycle management. 

---

**Namespace:** `enjin2`

**Header:** `include/enjin2/ui/component.hpp`

## Public Methods

### ``ComponentID getComponentID() const =0 const``

Get unique component type ID. 

return type identifier Componentclassenjin2_1_1Componentcompound

---

### ``virtual  ~ComponentBase()=default``

Virtual destructor for proper cleanup. 


        

---

## Protected Methods

### ``static ComponentID getComponentTypeID()``

Generate unique component type ID. 

templateparamT type Componentclassenjin2_1_1ComponentcompoundreturnUnique ID for component type T 

---

