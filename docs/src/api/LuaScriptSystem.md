---
id: LuaScriptSystem
title: LuaScriptSystem
sidebar_label: LuaScriptSystem
---

# LuaScriptSystem

High-level Lua scripting interface. 


Combines  and  for easy script execution with graphics and UI capabilities. LuaEngineclassenjin2_1_1LuaEnginecompoundLuaBindingsclassenjin2_1_1LuaBindingscompound

---

**Namespace:** enjin2

**Header:** include/enjin2/scripting/bindings.hpp

## Public Methods

### `cpp
* LuaScriptSystem()*
``

Constructor. 


        

---

### `cpp
*bool initialize()*
``

Initialize the script system. 

returnTrue if successful 

---

### `cpp
*void shutdown()*
``

Shutdown the script system. 


        

---

### `cpp
*void setCanvas(LuaCanvas *canvas)*
``

Set canvas for drawing operations. 

paramcanvasCanvas to use 

---

### `cpp
*LuaResultstructenjin2_1_1LuaResultcompound executeScript(const std::string &code)*
``

Execute Lua script string. 

paramcodeLua code to execute returnExecution result 

---

### `cpp
*LuaResultstructenjin2_1_1LuaResultcompound loadScript(const std::string &filename)*
``

Load and execute Lua script file. 

paramfilenameScript file path returnExecution result 

---

### `cpp
*LuaResultstructenjin2_1_1LuaResultcompound callFunction(const std::string &functionName, Args... args)*
``

Call Lua function. 

paramfunctionNameFunction name argsFunction arguments returnExecution result 

---

### `cpp
*size_t getMemoryUsage() const const*
``

Get script system memory usage. 

returnMemory usage in bytes 

---

### `cpp
* &LuaEngineclassenjin2_1_1LuaEnginecompound getEngine()*
``

Get Lua engine reference. 

returnLua engine 

---

### `cpp
* &LuaBindingsclassenjin2_1_1LuaBindingscompound getBindings()*
``

Get bindings reference. 

returnLua bindings 

---

