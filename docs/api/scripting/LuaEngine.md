---
id: LuaEngine
title: LuaEngine
sidebar_label: LuaEngine
---

# LuaEngine

Lua engine for embedded scripting support. 


Provides a lightweight Lua scripting environment optimized for embedded systems. Features static memory management and love2d.graphics-style API for familiarity. 

---

**Namespace:** `enjin2`

**Header:** `include/enjin2/scripting/lua_engine.hpp`

## Public Methods

### ` LuaEngine()`

Constructor initializes Lua state. 


        

---

### ` ~LuaEngine()`

Destructor cleans up Lua state. 


        

---

### `bool initialize()`

Initialize the Lua engine. 

True if initialization successful 

---

### `void shutdown()`

Shutdown the Lua engine. 


        

---

### `bool isInitialized() const const`

Check if engine is initialized. 

True if initialized 

---

### `LuaResult executeString(const std::string &code)`

Execute Lua code string. 

codeLua code to execute Execution result 

---

### `LuaResult executeFile(const std::string &filename)`

Load and execute Lua script file. 

filenamePath to Lua script file Execution result 

---

### `void registerFunction(const std::string &name, LuaCallback callback)`

Register C function with Lua. 

nameFunction name in Lua callbackC function callback 

---

### `void registerFunction(const std::string &name, lua_CFunction func)`

Register C function with Lua (C-style). 

nameFunction name in Lua funcC function pointer 

---

### `void createTable(const std::string &name)`

Create Lua table. 

nameTable name 

---

### `void setGlobal(const std::string &name, double value)`

Set global number variable in Lua. 

nameVariable name valueNumber value to set 

---

### `void setGlobal(const std::string &name, const std::string &value)`

Set global string variable in Lua. 

nameVariable name valueString value to set 

---

### `void setGlobal(const std::string &name, bool value)`

Set global boolean variable in Lua. 

nameVariable name valueBoolean value to set 

---

### `double getGlobalNumber(const std::string &name, double defaultValue=0.0)`

Get global number variable from Lua. 

nameVariable name defaultValueDefault value if variable not found Number value (or default if not found) 

---

### `std::string getGlobalString(const std::string &name, const std::string &defaultValue="")`

Get global string variable from Lua. 

nameVariable name defaultValueDefault value if variable not found String value (or default if not found) 

---

### `bool getGlobalBool(const std::string &name, bool defaultValue=false)`

Get global boolean variable from Lua. 

nameVariable name defaultValueDefault value if variable not found Boolean value (or default if not found) 

---

### `LuaResult callFunction(const std::string &functionName, Args... args)`

Call Lua function. 

functionNameName of Lua function argsFunction arguments Execution result 

---

### `size_t getMemoryUsage() const const`

Get current memory usage. 

Memory usage in bytes 

---

### `const std::vector&lt; std::string &gt; & getLoadedScripts() const const`

Get list of loaded scripts. 

Vector of script names 

---

### `void clearScripts()`

Clear all loaded scripts. 


        

---

### `lua_State * getState()`

Get Lua state (for advanced operations). 

Lua state pointer 

---

## Private Methods

### `static void * luaAllocator(void *ud, void *ptr, size_t osize, size_t nsize)`

Custom Lua allocator using static memory pool. 

udUser data ( instance) LuaEngineptrPointer to reallocate osizeOriginal size nsizeNew size Allocated memory or nullptr 

---

### `static int luaPanic(lua_State *L)`

 Lua panic. Handle

LLua state Never returns 

---

### `void pushArg(T &&arg)`

Push single argument to Lua stack. 

argArgument to push 

---

### `void pushArgs(T &&arg)`

Push single argument to Lua stack (base case). 

argArgument to push 

---

### `void pushArgs(T &&first, Args &&... rest)`


        


        

---

### `void pushArgs()`

Base case for pushArgs (no arguments). 


        

---

### `LuaResult checkResult(int result)`

Check Lua execution result. 

resultLua function result code  with success/error information LuaResult

---

