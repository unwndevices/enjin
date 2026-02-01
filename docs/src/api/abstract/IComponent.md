---
id: IComponent
title: IComponent
sidebar_label: IComponent
---

# IComponent

Abstract component interface for component lifecycle. 


Both enjin1 and enjin2 can implement this interface for compile-time polymorphism. Provides the standard component lifecycle methods (awake, start, update, etc.). 

---

**Namespace:** `enjin2`

**Header:** `include/enjin2/abstract/icomponent.hpp`

## Public Methods

### `virtual  ~IComponent()=default`

Virtual destructor for proper cleanup through base pointer. 


        

---

### `void awake()=0`

Awake is called when component is created. 

Use this for initialization that doesn't depend on other components. This is called before Start(). 

---

### `void start()=0`

Start is called before first frame update. 

Use this for initialization that depends on other components or objects being fully set up. 

---

### `void update(uint16_t deltaTime)=0`

Update is called once per frame. 

paramdeltaTimeTime since last frame in milliseconds 

---

### `void lateUpdate(uint16_t deltaTime)=0`

LateUpdate is called after all Update calls. 

paramdeltaTimeTime since last frame in milliseconds 

---

### `void onEnable()=0`

Called when component is enabled. 


        

---

### `void onDisable()=0`

Called when component is disabled. 


        

---

### ` *Objectclassenjin2_1_1Objectcompound getOwner() const =0 const`

Get owner object. 

returnPointer to owner object 

---

### `bool isEnabled() const =0 const`

Check if component is enabled. 

returnTrue if enabled 

---

### `void setEnabled(bool isEnabled)=0`

Set component enabled state. 

paramisEnabledclassenjin2_1_1IComponent_1a269f00448b8fed57fc53a3c25b74ed13memberNew enabled state 

---

