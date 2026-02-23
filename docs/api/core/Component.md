---
id: Component
title: Component
sidebar_label: Component
---

# Component

 base class. Component


Template base for typed components.All components in the Enjin system derive from this base class. Components provide specific functionality to Objects through composition.
Provides automatic type ID generation and type safety. TDerived component type

---

**Namespace:** `enjin2`

**Header:** `include/enjin2/core/component.hpp`

## Public Methods

### ` Component(Object *owner)`

Constructor. 

ownerThe object that owns this component 

---

### `virtual  ~Component()=default`

Virtual destructor. 


        

---

### ` *Object getOwner() const const`

Get the owner object. 

Pointer to owner object 

---

### `bool isEnabled() const const`

Check if component is enabled. 

True if enabled 

---

### `void setEnabled(bool isEnabled)`

Set component enabled state. 

isEnabledNew enabled state 

---

### `virtual void awake()`

Awake is called when the component is created. 

Use this for initialization that doesn't depend on other components. This is called before Start(). 

---

### `virtual void start()`

Start is called before the first frame update. 

Use this for initialization that depends on other components or objects being fully set up. 

---

### `virtual void update(uint16_t deltaTime)`

Update is called once per frame. 

deltaTimeTime since last frame in milliseconds 

---

### `virtual void lateUpdate(uint16_t deltaTime)`

LateUpdate is called after all Update calls. 

deltaTimeTime since last frame in milliseconds 

---

### `virtual void onEnable()`

Called when component is enabled. 


        

---

### `virtual void onDisable()`

Called when component is disabled. 


        

---

### `virtual ComponentID getComponentID() const override const`

Get component type ID for this component type. 

 type identifier Component

---

### `static ComponentID getStaticComponentID()`

Get static component type ID. 

 type identifier Component

---

