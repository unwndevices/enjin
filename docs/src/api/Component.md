---
id: Component
title: Component
sidebar_label: Component
---

# Component

 base class. Componentclassenjin2_1_1Componentcompound


Template base for typed components.All components in the Enjin system derive from this base class. Components provide specific functionality to Objects through composition.
Provides automatic type ID generation and type safety. templateparamTDerived component type

---

**Namespace:** enjin2

**Header:** include/enjin2/core/component.hpp

## Public Methods

### `cpp
* Component(Object *owner)*
``

Constructor. 

paramownerclassenjin2_1_1Component_1a3349b9210509e148f9f7625f6a39b022memberThe object that owns this component 

---

### `cpp
*virtual  ~Component()=default*
``

Virtual destructor. 


        

---

### `cpp
* *Objectclassenjin2_1_1Objectcompound getOwner() const const*
``

Get the owner object. 

returnPointer to owner object 

---

### `cpp
*bool isEnabled() const const*
``

Check if component is enabled. 

returnTrue if enabled 

---

### `cpp
*void setEnabled(bool isEnabled)*
``

Set component enabled state. 

paramisEnabledclassenjin2_1_1Component_1a9816f093b126432025a7e0316b34ff6amemberNew enabled state 

---

### `cpp
*virtual void awake()*
``

Awake is called when the component is created. 

Use this for initialization that doesn't depend on other components. This is called before Start(). 

---

### `cpp
*virtual void start()*
``

Start is called before the first frame update. 

Use this for initialization that depends on other components or objects being fully set up. 

---

### `cpp
*virtual void update(uint16_t deltaTime)*
``

Update is called once per frame. 

paramdeltaTimeTime since last frame in milliseconds 

---

### `cpp
*virtual void lateUpdate(uint16_t deltaTime)*
``

LateUpdate is called after all Update calls. 

paramdeltaTimeTime since last frame in milliseconds 

---

### `cpp
*virtual void onEnable()*
``

Called when component is enabled. 


        

---

### `cpp
*virtual void onDisable()*
``

Called when component is disabled. 


        

---

### `cpp
*virtual ComponentID getComponentID() const override const*
``

Get component type ID for this component type. 

return type identifier Componentclassenjin2_1_1Componentcompound

---

### `cpp
*static ComponentID getStaticComponentID()*
``

Get static component type ID. 

return type identifier Componentclassenjin2_1_1Componentcompound

---

