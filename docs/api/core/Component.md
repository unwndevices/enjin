---
id: Component
title: Component
sidebar_label: Component
---

# Component

Component base class. 


Template base for typed components.All components in the Enjin system derive from this base class. Components provide specific functionality to Objects through composition.TDerived component type
Provides automatic type ID generation and type safety. 

---

**Namespace:** `enjin2`

**Header:** `include/enjin2/core/component.hpp`

## Public Methods

### ` Component(Object *owner)`

Constructor. 

ownerThe object that owns this component 

---

### `virtual  ~Component()`

Virtual destructor — invalidates associated ComponentProxy before component is freed. Sets m_luaProxy-&gt;valid = false so stale Lua proxy access raises a Lua error. 

---

### `void setLuaProxy(ComponentProxy *proxy)`

Register a Lua-side ComponentProxy for destructor invalidation. Called by lua_proxy_get_component_impl when allocating a ComponentProxy userdata. 

proxyNon-owning pointer to the Lua userdata struct; may be nullptr to clear. 

---

### `Object * getOwner() const`

Get the owner object. 

Pointer to owner object 

---

### `bool isEnabled() const`

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

### `virtual void update(float dt)`

Update is called once per frame. 

dtTime since last frame in seconds 

---

### `virtual void lateUpdate(float dt)`

LateUpdate is called after all Update calls. 

dtTime since last frame in seconds 

---

### `virtual void onEnable()`

Called when component is enabled. 

---

### `virtual void onDisable()`

Called when component is disabled. 

---

### `virtual ComponentID getComponentID() const override const`

Get component type ID for this component type. 

Component type identifier 

---

### `static ComponentID getStaticComponentID()`

Get static component type ID. 

Component type identifier 

---

## Protected Methods

### `void assertRequires()`

Assert that a sibling component of type T exists on the same Object. 

In debug builds (NDEBUG not defined): calls assert(false) with a message naming the requirement. The call stack identifies the failing component. No abort on missing dep when the dep IS present (no-op).In release builds (NDEBUG defined): logs once via printf and disables this component. No process abort — safe for ESP32.Call from awake() to declare component dependencies loudly.Naming note: assertRequires&lt;T&gt;() chosen over requires&lt;T&gt;() — 'requires' is a C++20 keyword and causes a compile error (Phase 26 decision).TRequired component type (must derive from Component) 

---

