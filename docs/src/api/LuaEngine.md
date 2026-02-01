---
id: LuaEngine
title: LuaEngine
sidebar_label: LuaEngine
---

# LuaEngine

Lua engine for embedded scripting support. 


Provides a lightweight Lua scripting environment optimized for embedded systems. Features static memory management and love2d.graphics-style API for familiarity. 

---

**Namespace:** enjin2

**Header:** include/enjin2/scripting/lua_engine.hpp

## Public Methods

### `cpp
* LuaEngine()*
``

Constructor initializes Lua state. 


        

---

### `cpp
* ~LuaEngine()*
``

Destructor cleans up Lua state. 


        

---

### `cpp
*bool initialize()*
``

Initialize the Lua engine. 

returnTrue if initialization successful 

---

### `cpp
*void shutdown()*
``

Shutdown the Lua engine. 


        

---

### `cpp
*bool isInitialized() const const*
``

Check if engine is initialized. 

returnTrue if initialized 

---

### `cpp
*LuaResultstructenjin2_1_1LuaResultcompound executeString(const std::string &code)*
``

Execute Lua code string. 

paramcodeLua code to execute returnExecution result 

---

### `cpp
*LuaResultstructenjin2_1_1LuaResultcompound executeFile(const std::string &filename)*
``

Load and execute Lua script file. 

paramfilenamePath to Lua script file returnExecution result 

---

### `cpp
*void registerFunction(const std::string &name, LuaCallback callback)*
``

Register C function with Lua. 

paramnameFunction name in Lua callbackC function callback 

---

### `cpp
*void registerFunction(const std::string &name, lua_CFunction func)*
``

Register C function with Lua (C-style). 

paramnameFunction name in Lua funcC function pointer 

---

### `cpp
*void createTable(const std::string &name)*
``

Create Lua table. 

paramnameTable name 

---

### `cpp
*void setGlobal(const std::string &name, double value)*
``

Set global variable in Lua. 

paramnameVariable name valueVariable value 

---

### `cpp
*void setGlobal(const std::string &name, const std::string &value)*
``


        


        

---

### `cpp
*void setGlobal(const std::string &name, bool value)*
``


        


        

---

### `cpp
*double getGlobalNumber(const std::string &name, double defaultValue=0.0)*
``

Get global variable from Lua. 

paramnameVariable name returnVariable value (or default if not found) 

---

### `cpp
*std::string getGlobalString(const std::string &name, const std::string &defaultValue="")*
``


        


        

---

### `cpp
*bool getGlobalBool(const std::string &name, bool defaultValue=false)*
``


        


        

---

### `cpp
*LuaResultstructenjin2_1_1LuaResultcompound callFunction(const std::string &functionName, Args... args)*
``

Call Lua function. 

paramfunctionNameName of Lua function argsFunction arguments returnExecution result 

---

### `cpp
*size_t getMemoryUsage() const const*
``

Get current memory usage. 

returnMemory usage in bytes 

---

### `cpp
*const std::vector&lt; std::string &gt; & getLoadedScripts() const const*
``

Get list of loaded scripts. 

returnVector of script names 

---

### `cpp
*void clearScripts()*
``

Clear all loaded scripts. 


        

---

### `cpp
*lua_State * getState()*
``

Get Lua state (for advanced operations). 

returnLua state pointer 

---

## Private Methods

### `cpp
*static void * luaAllocator(void *ud, void *ptr, size_t osize, size_t nsize)*
``

Custom Lua allocator using static memory pool. 

paramudUser data ( instance) LuaEngineclassenjin2_1_1LuaEnginecompoundptrPointer to reallocate osizeOriginal size nsizeNew size returnAllocated memory or nullptr 

---

### `cpp
*static int luaPanic(lua_State *L)*
``

 Lua panic. Handlestructenjin2_1_1Handlecompound

paramLLua state returnNever returns 

---

### `cpp
*void pushArg(T &&arg)*
``

Push arguments to Lua stack. 

paramargsArguments to push 

---

### `cpp
*void pushArgs(T &&arg)*
``

Push multiple arguments to Lua stack. 

paramfirstFirst argument restRemaining arguments 

---

### `cpp
*void pushArgs(T &&first, Args &&... rest)*
``


        


        

---

### `cpp
*void pushArgs()*
``

Base case for pushArgs (no arguments). 


        

---

### `cpp
*LuaResultstructenjin2_1_1LuaResultcompound checkResult(int result)*
``

Check Lua execution result. 

paramresultLua function result code return with success/error information LuaResultstructenjin2_1_1LuaResultcompound

---

