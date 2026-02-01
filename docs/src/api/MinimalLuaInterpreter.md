---
id: MinimalLuaInterpreter
title: MinimalLuaInterpreter
sidebar_label: MinimalLuaInterpreter
---

# MinimalLuaInterpreter

Minimal Lua interpreter for ESP32. 


Lightweight Lua implementation for ESP32 with reduced memory footprint and essential graphics functions only. 

---

**Namespace:** enjin2

**Header:** include/enjin2/scripting/lua_interpreter.hpp

## Public Methods

### `cpp
* MinimalLuaInterpreter()*
``

Constructor. 


        

---

### `cpp
* ~MinimalLuaInterpreter() override*
``

Destructor. 


        

---

### `cpp
*void setGraphics(IScriptGraphics *gfx)*
``

Set graphics interface. 

paramgfxGraphics interface to use 

---

### `cpp
*virtual bool initialize() override*
``

Initialize the interpreter. 

returnTrue if initialization successful 

---

### `cpp
*virtual void shutdown() override*
``

Shutdown the interpreter. 


        

---

### `cpp
*virtual bool isInitialized() const override const*
``

Check if interpreter is initialized. 

returnTrue if initialized 

---

### `cpp
*virtual ScriptResultstructenjin2_1_1ScriptResultcompound executeString(const std::string &code) override*
``

Execute script code string. 

paramcodeScript code to execute returnExecution result 

---

### `cpp
*virtual ScriptResultstructenjin2_1_1ScriptResultcompound executeFile(const std::string &filename) override*
``

Load and execute script file. 

paramfilenamePath to script file returnExecution result 

---

### `cpp
*virtual ScriptResultstructenjin2_1_1ScriptResultcompound callFunction(const std::string &functionName) override*
``

Call script function. 

paramfunctionNameName of function to call returnExecution result 

---

### `cpp
*virtual void setGlobal(const std::string &name, double value) override*
``

Set global variable. 

paramnameVariable name valueVariable value 

---

### `cpp
*virtual void setGlobal(const std::string &name, const std::string &value) override*
``


        


        

---

### `cpp
*virtual void setGlobal(const std::string &name, bool value) override*
``


        


        

---

### `cpp
*virtual double getGlobalNumber(const std::string &name, double defaultValue=0.0) override*
``

Get global variable. 

paramnameVariable name defaultValueDefault value if not found returnVariable value 

---

### `cpp
*virtual std::string getGlobalString(const std::string &name, const std::string &defaultValue="") override*
``


        


        

---

### `cpp
*virtual bool getGlobalBool(const std::string &name, bool defaultValue=false) override*
``


        


        

---

### `cpp
*virtual size_t getMemoryUsage() const override const*
``

Get current memory usage. 

returnMemory usage in bytes 

---

### `cpp
*virtual const char * getTypeName() const override const*
``

Get interpreter type name. 

returnType name string (e.g., "Lua", "JavaScript", "MicroPython") 

---

## Private Methods

### `cpp
*void registerMinimalBindings()*
``

Register minimal graphics functions. 


        

---

### `cpp
*void * allocateMemory(size_t size)*
``

Allocate memory from pool. 

paramsize to allocate Sizestructenjin2_1_1SizecompoundreturnAllocated memory or nullptr 

---

### `cpp
*void freeMemory(void *ptr)*
``

Free memory back to pool. 

paramptrMemory to free 

---

