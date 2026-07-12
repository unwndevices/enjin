---
id: Object
title: Object
sidebar_label: Object
---

# Object

Object base class for game entities. 


The Object class is the base class for all game entities in the Enjin system. It manages components using static allocation and provides lifecycle methods. 

---

**Namespace:** `enjin2`

**Header:** `include/enjin2/core/object.hpp`

## Public Methods

### ` Object()`

Constructor. 

---

### `virtual  ~Object()`

Destructor — invalidates associated ObjectProxy before Object is freed. Sets m_luaProxy-&gt;valid = false so stale Lua proxy access raises a Lua error. 

---

### `virtual void awake()`

Awake is called when object is created. 

Use this to ensure required components are present and initialize component relationships. 

---

### `virtual void start()`

Start is called before the first frame update. 

Use this for initialization that depends on other objects being fully set up. 

---

### `virtual void update(float dt)`

Update is called once per frame. 

dtTime since last frame in seconds 

---

### `virtual void lateUpdate(float dt)`

LateUpdate is called after all Update calls. 

dtTime since last frame in seconds 

---

### `bool isQueuedForRemoval() const`

Check if object is queued for removal (matches original Enjin). 

True if object should be removed 

---

### `T * addComponent(Args &&... args)`

Add a component to this object. 

TComponent type (must derive from Component) ArgsConstructor argument types argsConstructor arguments Pointer to the created component or nullptr if failed 

---

### `T * getComponent()`

Get a component of specified type. 

TComponent type Pointer to component or nullptr if not found 

---

### `const T * getComponent() const`

Get a component of specified type (const overload — called by hasComponent() const and read-only contexts). 

TComponent type Const pointer to component or nullptr if not found 

---

### `size_t getComponents(T **out, size_t maxOut) const`

Get all components of specified type. 

TComponent type (must derive from Component) outCaller-provided array to write matching component pointers into maxOutMaximum number of results to write Number of components written into out 

---

### `bool hasComponent() const`

Check if object has a component of specified type. 

TComponent type True if component exists 

---

### `bool removeComponent()`

Remove a component of specified type. 

TComponent type True if component was removed 

---

### `C_Position * getPosition() const`

Get position component (cached for performance). 

Position component pointer or nullptr 

---

### `bool isActive() const`

Check if object is active. 

True if active 

---

### `void setActive(bool isActive)`

Set object active state. 

isActiveNew active state 

---

### `void setLuaProxy(ObjectProxy *proxy)`

Register the Lua ObjectProxy associated with this Object. Called by engine.scene.find() when it wraps this Object in an ObjectProxy userdata. The destructor will set proxy-&gt;valid = false when the Object is freed. Only one proxy should be active per Object at a time — the last call overwrites any previous registration. 

proxyNon-owning pointer to the ObjectProxy userdata; nullptr to clear. 

---

### `size_t getComponentCount() const`

Get total number of components. 

Component count 

---

### `void setName(const char *n)`

Set object name (stores pointer — caller owns lifetime). 

nNull-terminated name string or nullptr to clear 

---

### `const char * getName() const`

Get object name. 

Pointer to name string or nullptr if not set 

---

### `bool addTag(const char *tag)`

Add a tag to this object (up to MAX_TAGS = 8). 

tagNull-terminated tag string (caller owns lifetime) true if tag was added, false if tag array is full 

---

### `bool hasTag(const char *tag) const`

Check if this object has a given tag. 

tagTag string to look for true if tag is present 

---

### `void clearTags()`

Clear all tags from this object. 

---

### `size_t getTagCount() const`

Get current number of tags. 

Tag count 

---

## Private Methods

### `std::enable_if&lt; std::is_same&lt; T, C_Position &gt;::value &gt;::type cachePositionIfType(T *componentPtr)`

Helper to cache position component only if T is C_Position using SFINAE. 

---

### `std::enable_if&lt;!std::is_same&lt; T, C_Position &gt;::value &gt;::type cachePositionIfType(T *componentPtr)`

---

### `void initializeComponentCache()`

Initialize cached component pointers. 

---

