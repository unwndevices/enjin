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

**Namespace:** `enjin2`

**Header:** `include/enjin2/core/component.hpp`

## Public Methods

### `` Component(Object *owner)``

Constructor. 

paramownerclassenjin2_1_1Component_1a3349b9210509e148f9f7625f6a39b022memberThe object that owns this component 

---

### ``virtual  ~Component()=default``

Virtual destructor. 


        

---

### `` *Objectclassenjin2_1_1Objectcompound getOwner() const const``

Get the owner object. 

returnPointer to owner object 

---

### ``bool isEnabled() const const``

Check if component is enabled. 

returnTrue if enabled 

---

### ``void setEnabled(bool isEnabled)``

Set component enabled state. 

paramisEnabledclassenjin2_1_1Component_1a9816f093b126432025a7e0316b34ff6amemberNew enabled state 

---

### ``virtual void awake()``

Awake is called when the component is created. 

Use this for initialization that doesn't depend on other components. This is called before Start(). 

---

### ``virtual void start()``

Start is called before the first frame update. 

Use this for initialization that depends on other components or objects being fully set up. 

---

### ``virtual void update(uint16_t deltaTime)``

Update is called once per frame. 

paramdeltaTimeTime since last frame in milliseconds 

---

### ``virtual void lateUpdate(uint16_t deltaTime)``

LateUpdate is called after all Update calls. 

paramdeltaTimeTime since last frame in milliseconds 

---

### ``virtual void onEnable()``

Called when component is enabled. 


        

---

### ``virtual void onDisable()``

Called when component is disabled. 


        

---

### ``virtual ComponentID getComponentID() const override const``

Get component type ID for this component type. 

return type identifier Componentclassenjin2_1_1Componentcompound

---

### ``static ComponentID getStaticComponentID()``

Get static component type ID. 

return type identifier Componentclassenjin2_1_1Componentcompound

---

