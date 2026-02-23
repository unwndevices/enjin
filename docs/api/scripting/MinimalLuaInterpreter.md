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

paramgfxGraphics interface to use 

---

### `virtual bool initialize() override`

Initialize the interpreter. 

returnTrue if initialization successful 

---

### `virtual void shutdown() override`

Shutdown the interpreter. 


        

---

### `virtual bool isInitialized() const override const`

Check if interpreter is initialized. 

returnTrue if initialized 

---

### `virtual ScriptResultstructenjin2_1_1ScriptResultcompound executeString(const std::string &code) override`

Execute script code string. 

paramcodeScript code to execute returnExecution result 

---

### `virtual ScriptResultstructenjin2_1_1ScriptResultcompound executeFile(const std::string &filename) override`

Load and execute script file. 

paramfilenamePath to script file returnExecution result 

---

### `virtual ScriptResultstructenjin2_1_1ScriptResultcompound callFunction(const std::string &functionName) override`

Call script function. 

paramfunctionNameName of function to call returnExecution result 

---

### `virtual void setGlobal(const std::string &name, double value) override`

Set global number variable. 

paramnameVariable name valueNumber value to set returnvoid 

---

### `virtual void setGlobal(const std::string &name, const std::string &value) override`

Set global string variable. 

paramnameVariable name valueString value to set returnvoid 

---

### `virtual void setGlobal(const std::string &name, bool value) override`

Set global boolean variable. 

paramnameVariable name valueBoolean value to set returnvoid 

---

### `virtual double getGlobalNumber(const std::string &name, double defaultValue=0.0) override`

Get global number variable. 

paramnameVariable name defaultValueDefault value if not found returnNumber value (or default if not found) 

---

### `virtual std::string getGlobalString(const std::string &name, const std::string &defaultValue="") override`

Get global string variable. 

paramnameVariable name defaultValueDefault value if not found returnString value (or default if not found) 

---

### `virtual bool getGlobalBool(const std::string &name, bool defaultValue=false) override`

Get global boolean variable. 

paramnameVariable name defaultValueDefault value if not found returnBoolean value (or default if not found) 

---

### `virtual size_t getMemoryUsage() const override const`

Get current memory usage. 

returnMemory usage in bytes 

---

### `virtual const char * getTypeName() const override const`

Get interpreter type name. 

returnType name string (e.g., "Lua", "JavaScript", "MicroPython") 

---

## Private Methods

### `void registerMinimalBindings()`

Register minimal graphics functions. 


        

---

### `void * allocateMemory(size_t size)`

Allocate memory from pool. 

paramsize to allocate Sizestructenjin2_1_1SizecompoundreturnAllocated memory or nullptr 

---

### `void freeMemory(void *ptr)`

Free memory back to pool. 

paramptrMemory to free 

---

