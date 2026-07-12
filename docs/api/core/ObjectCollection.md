---
id: ObjectCollection
title: ObjectCollection
sidebar_label: ObjectCollection
---

# ObjectCollection

Collection for managing multiple objects. 


Provides static allocation-based management of objects with lifecycle control and organized updates. 

---

**Namespace:** `enjin2`

**Header:** `include/enjin2/core/object_collection.hpp`

## Public Methods

### ` ObjectCollection()`

Constructor. 

---

### ` ~ObjectCollection()=default`

Destructor. 

---

### `void initialize()`

Initialize the collection. 

Calls awake() on all objects in the collection. 

---

### `void start()`

Start the collection. 

Calls start() on all objects in the collection. 

---

### `void update(float dt)`

Update all objects in the collection (owned + externals). 

dtTime since last frame in seconds 

---

### `void lateUpdate(float dt)`

Late update all objects in the collection (owned + externals). 

dtTime since last frame in seconds 

---

### `T * addObject(Args &&... args)`

Add an object to the collection. 

TObject type (must derive from Object) ArgsConstructor argument types argsConstructor arguments Pointer to created object or nullptr if failed 

---

### `bool removeObject(Object *object)`

Remove an object from the collection (destroys the object). 

objectObject to remove True if object was removed DESTRUCTIVE — do NOT use for persistence; use extractObject() instead 

---

### `std::unique_ptr&lt; Object &gt; extractObject(Object *object)`

Extract an object from the collection without destroying it. 

Removes the object from the owned array and returns unique_ptr ownership to the caller. The object is NOT destroyed. Use this before calling PersistentObjectRegistry::add() to transfer ownership without invalidating any associated Lua proxy.objectRaw pointer to the object to extract unique_ptr owning the object, or nullptr if not found 

---

### `void injectExternal(Object *obj)`

Inject a non-owning external (persistent) object pointer. 

Adds the object to m_external[] so it participates in update/lateUpdate/ findByName/forEach iterations alongside owned objects. Does NOT transfer ownership. Ownership remains with PersistentObjectRegistry.objNon-owning pointer to inject (nullptr-safe, silently ignored) 

---

### `void clearExternal()`

Clear all external (persistent) object injection slots. 

Zeros all m_external[] pointers and resets m_externalCount to 0. Must be called before a scene transition clears the scene, so the registry retains sole ownership of persistent objects. 

---

### `T * findObject()`

Find first object of specified type. 

TObject type Pointer to object or nullptr if not found 

---

### `size_t findObjects(T **results, size_t maxResults)`

Find all objects of specified type. 

TObject type resultsArray to store results maxResultsMaximum number of results Number of objects found 

---

### `Object * findObjectWithComponent()`

Find object with component of specified type. 

TComponent type Pointer to object or nullptr if not found 

---

### `void forEach(std::function&lt; void(Object *)&gt; func)`

Apply function to all objects (owned + externals). 

funcFunction to apply (takes Object* parameter) 

---

### `Object * findByName(const char *name)`

Find object by name (searches owned then externals). 

nameName to search for (nullptr returns nullptr immediately) Pointer to first matching Object or nullptr if not found 

---

### `size_t findAllWithTag(const char *tag, Object **results, size_t maxResults)`

Find all objects with a given tag (owned + externals). 

tagTag to search for resultsCaller-provided buffer to receive Object pointers maxResultsMaximum number of results to write into buffer Number of matching objects written into buffer 

---

### `void forEachActive(std::function&lt; void(Object *)&gt; func)`

Apply function to all active objects (owned + externals). 

funcFunction to apply (takes Object* parameter) 

---

### `void clear()`

Clear all objects from the collection. 

---

### `size_t size() const`

Get number of objects in collection. 

Object count 

---

### `bool empty() const`

Check if collection is empty. 

True if empty 

---

### `Object * getObject(size_t index)`

Get object at index. 

indexObject index Pointer to object or nullptr if invalid index 

---

### `const Object * getObject(size_t index) const`

Get const object at index. 

indexObject index Pointer to object or nullptr if invalid index 

---

### `void removeInactiveObjects()`

Remove inactive objects from the collection. 

This compacts the array by removing objects that are not active, which can help with performance. 

---

