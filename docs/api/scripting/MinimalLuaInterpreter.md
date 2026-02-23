---
id: MinimalLuaInterpreter
title: MinimalLuaInterpreter
sidebar_label: MinimalLuaInterpreter
---

# MinimalLuaInterpreter

Minimal Lua interpreter for ESP32. 


Lightweight Lua implementation for ESP32 with reduced memory footprint and essential graphics functions only. 

---

**Namespace:** `enjin2`

**Header:** `include/enjin2/scripting/lua_interpreter.hpp`

## Public Methods

### ` MinimalLuaInterpreter()`

Constructor. 


        

---

### ` ~MinimalLuaInterpreter() override`

Destructor. 


        

---

### `void setGraphics(IScriptGraphics *gfx)`

Set graphics interface. 

gfxGraphics interface to use 

---

### `virtual bool initialize() override`

Initialize the minimal Lua interpreter. 

True if initialization successful 

---

### `virtual void shutdown() override`

Shutdown the minimal Lua interpreter. 


        

---

### `virtual bool isInitialized() const override const`

Check if interpreter is initialized. 

True if initialized 

---

### `virtual ScriptResult executeString(const std::string &code) override`

Execute Lua code string. 

codeLua code to execute Execution result 

---

### `virtual ScriptResult executeFile(const std::string &filename) override`

Execute Lua script file. 

filenamePath to script file Execution result 

---

### `virtual ScriptResult callFunction(const std::string &functionName) override`

Call named Lua function. 

functionNameName of function to call Execution result 

---

### `virtual void setGlobal(const std::string &name, double value) override`

Set global number variable. 

nameVariable name valueNumber value to set 

---

### `virtual void setGlobal(const std::string &name, const std::string &value) override`

Set global string variable. 

nameVariable name valueString value to set 

---

### `virtual void setGlobal(const std::string &name, bool value) override`

Set global boolean variable. 

nameVariable name valueBoolean value to set 

---

### `virtual double getGlobalNumber(const std::string &name, double defaultValue=0.0) override`

Get global number variable. 

nameVariable name defaultValueDefault value if not found Number value (or default if not found) 

---

### `virtual std::string getGlobalString(const std::string &name, const std::string &defaultValue="") override`

Get global string variable. 

nameVariable name defaultValueDefault value if not found String value (or default if not found) 

---

### `virtual bool getGlobalBool(const std::string &name, bool defaultValue=false) override`

Get global boolean variable. 

nameVariable name defaultValueDefault value if not found Boolean value (or default if not found) 

---

### `virtual size_t getMemoryUsage() const override const`

Get current memory usage. 

Memory usage in bytes 

---

### `virtual const char * getTypeName() const override const`

Get interpreter type name. 

Type name string 

---

## Private Methods

### `void registerMinimalBindings()`

Register minimal graphics functions. 


        

---

### `void * allocateMemory(size_t size)`

Allocate memory from pool. 

size to allocate SizeAllocated memory or nullptr 

---

### `void freeMemory(void *ptr)`

Free memory back to pool. 

ptrMemory to free 

---

