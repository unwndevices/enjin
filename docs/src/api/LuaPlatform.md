---
id: LuaPlatform
title: LuaPlatform
sidebar_label: LuaPlatform
---

# LuaPlatform

Platform abstraction for Lua initialization. 



    

---

**Namespace:** enjin2

**Header:** include/enjin2/scripting/lua_platform.hpp

## Public Methods

### `cpp
*static lua_State * createState(lua_Alloc allocator=nullptr, void *ud=nullptr)*
``

Create platform-appropriate Lua state. 

paramallocatorCustom allocator function (nullptr for default) udUser data for allocator returnLua state or nullptr on failure 

---

### `cpp
*static void openLibraries(lua_State *L)*
``

Open platform-appropriate standard libraries. 

paramLLua state 

---

### `cpp
*static void configureSecurityRestrictions(lua_State *L)*
``

Configure platform-specific security restrictions. 

paramLLua state 

---

### `cpp
*static size_t getMemoryUsage(lua_State *L)*
``

Get platform memory statistics. 

paramLLua state returnMemory usage in bytes 

---

### `cpp
*static void tuneGarbageCollector(lua_State *L)*
``

Platform-specific garbage collection tuning. 

paramLLua state 

---

