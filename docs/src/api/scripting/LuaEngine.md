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

### `` LuaEngine()``

Constructor initializes Lua state. 


        

---

### `` ~LuaEngine()``

Destructor cleans up Lua state. 


        

---

### ``bool initialize()``

Initialize the Lua engine. 

returnTrue if initialization successful 

---

### ``void shutdown()``

Shutdown the Lua engine. 


        

---

### ``bool isInitialized() const const``

Check if engine is initialized. 

returnTrue if initialized 

---

### ``LuaResultstructenjin2_1_1LuaResultcompound executeString(const std::string &code)``

Execute Lua code string. 

paramcodeLua code to execute returnExecution result 

---

### ``LuaResultstructenjin2_1_1LuaResultcompound executeFile(const std::string &filename)``

Load and execute Lua script file. 

paramfilenamePath to Lua script file returnExecution result 

---

### ``void registerFunction(const std::string &name, LuaCallback callback)``

Register C function with Lua. 

paramnameFunction name in Lua callbackC function callback 

---

### ``void registerFunction(const std::string &name, lua_CFunction func)``

Register C function with Lua (C-style). 

paramnameFunction name in Lua funcC function pointer 

---

### ``void createTable(const std::string &name)``

Create Lua table. 

paramnameTable name 

---

### ``void setGlobal(const std::string &name, double value)``

Set global variable in Lua. 

paramnameVariable name valueVariable value 

---

### ``void setGlobal(const std::string &name, const std::string &value)``


        


        

---

### ``void setGlobal(const std::string &name, bool value)``


        


        

---

### ``double getGlobalNumber(const std::string &name, double defaultValue=0.0)``

Get global variable from Lua. 

paramnameVariable name returnVariable value (or default if not found) 

---

### ``std::string getGlobalString(const std::string &name, const std::string &defaultValue="")``


        


        

---

### ``bool getGlobalBool(const std::string &name, bool defaultValue=false)``


        


        

---

### ``LuaResultstructenjin2_1_1LuaResultcompound callFunction(const std::string &functionName, Args... args)``

Call Lua function. 

paramfunctionNameName of Lua function argsFunction arguments returnExecution result 

---

### ``size_t getMemoryUsage() const const``

Get current memory usage. 

returnMemory usage in bytes 

---

### ``const std::vector< std::string > & getLoadedScripts() const const``

Get list of loaded scripts. 

returnVector of script names 

---

### ``void clearScripts()``

Clear all loaded scripts. 


        

---

### ``lua_State * getState()``

Get Lua state (for advanced operations). 

returnLua state pointer 

---

## Private Methods

### ``static void * luaAllocator(void *ud, void *ptr, size_t osize, size_t nsize)``

Custom Lua allocator using static memory pool. 

paramudUser data ( instance) LuaEngineclassenjin2_1_1LuaEnginecompoundptrPointer to reallocate osizeOriginal size nsizeNew size returnAllocated memory or nullptr 

---

### ``static int luaPanic(lua_State *L)``

 Lua panic. Handlestructenjin2_1_1Handlecompound

paramLLua state returnNever returns 

---

### ``void pushArg(T &&arg)``

Push arguments to Lua stack. 

paramargsArguments to push 

---

### ``void pushArgs(T &&arg)``

Push multiple arguments to Lua stack. 

paramfirstFirst argument restRemaining arguments 

---

### ``void pushArgs(T &&first, Args &&... rest)``


        


        

---

### ``void pushArgs()``

Base case for pushArgs (no arguments). 


        

---

### ``LuaResultstructenjin2_1_1LuaResultcompound checkResult(int result)``

Check Lua execution result. 

paramresultLua function result code return with success/error information LuaResultstructenjin2_1_1LuaResultcompound

---

