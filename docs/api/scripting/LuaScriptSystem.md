---
id: LuaScriptSystem
title: LuaScriptSystem
sidebar_label: LuaScriptSystem
---

# LuaScriptSystem

High-level Lua scripting interface. 


Combines LuaEngine and LuaBindings for easy script execution with graphics and UI capabilities. 

---

**Namespace:** `enjin2`

**Header:** `include/enjin2/scripting/bindings.hpp`

## Public Methods

### ` LuaScriptSystem()`

Constructor. 

---

### `bool initialize()`

Initialize the script system. 

True if successful 

---

### `void shutdown()`

Shutdown the script system. 

---

### `void setCanvas(LuaCanvas *canvas)`

Set canvas for drawing operations. 

canvasCanvas to use 

---

### `LuaResult executeScript(const std::string &code)`

Execute Lua script string. 

codeLua code to execute Execution result 

---

### `LuaResult loadScript(const std::string &filename)`

Load and execute Lua script file. 

filenameScript file path Execution result 

---

### `LuaResult callFunction(const std::string &functionName, Args... args)`

Call Lua function. 

functionNameFunction name argsFunction arguments Execution result 

---

### `size_t getMemoryUsage() const`

Get script system memory usage. 

Memory usage in bytes 

---

### `LuaEngine & getEngine()`

Get Lua engine reference. 

Lua engine 

---

### `LuaBindings & getBindings()`

Get bindings reference. 

Lua bindings 

---

