---
id: LuaPlatform
title: LuaPlatform
sidebar_label: LuaPlatform
---

# LuaPlatform

Platform abstraction for Lua initialization. 



    

---

**Namespace:** `enjin2`

**Header:** `include/enjin2/scripting/lua_platform.hpp`

## Public Methods

### `static lua_State * createState(lua_Alloc allocator=nullptr, void *ud=nullptr)`

Create platform-appropriate Lua state. 

allocatorCustom allocator function (nullptr for default) udUser data for allocator Lua state or nullptr on failure 

---

### `static void openLibraries(lua_State *L)`

Open platform-appropriate standard libraries. 

LLua state 

---

### `static void configureSecurityRestrictions(lua_State *L)`

Configure platform-specific security restrictions. 

LLua state 

---

### `static size_t getMemoryUsage(lua_State *L)`

Get platform memory statistics. 

LLua state Memory usage in bytes 

---

### `static void tuneGarbageCollector(lua_State *L)`

Platform-specific garbage collection tuning. 

LLua state 

---

