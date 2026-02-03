---
id: LuaInterpreter
title: LuaInterpreter
sidebar_label: LuaInterpreter
---

# LuaInterpreter

Full Lua interpreter implementation. 


Desktop/VCV implementation using full Lua with complete enjin2 bindings. Provides all Lua features and comprehensive graphics API. 

---

**Namespace:** `enjin2`

**Header:** `include/enjin2/scripting/lua_interpreter.hpp`

## Public Methods

### ` LuaInterpreter()`

Constructor. 


        

---

### ` ~LuaInterpreter() override`

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

Set global variable. 

paramnameVariable name valueVariable value 

---

### `virtual void setGlobal(const std::string &name, const std::string &value) override`


        


        

---

### `virtual void setGlobal(const std::string &name, bool value) override`


        


        

---

### `virtual double getGlobalNumber(const std::string &name, double defaultValue=0.0) override`

Get global variable. 

paramnameVariable name defaultValueDefault value if not found returnVariable value 

---

### `virtual std::string getGlobalString(const std::string &name, const std::string &defaultValue="") override`


        


        

---

### `virtual bool getGlobalBool(const std::string &name, bool defaultValue=false) override`


        


        

---

### `virtual size_t getMemoryUsage() const override const`

Get current memory usage. 

returnMemory usage in bytes 

---

### `virtual const char * getTypeName() const override const`

Get interpreter type name. 

returnType name string (e.g., "Lua", "JavaScript", "MicroPython") 

---

### ` *LuaEngineclassenjin2_1_1LuaEnginecompound getLuaEngine()`

Get access to underlying Lua engine (for advanced operations). 

returnLua engine reference 

---

### ` *LuaBindingsclassenjin2_1_1LuaBindingscompound getLuaBindings()`

Get access to Lua bindings. 

returnLua bindings reference 

---

## Private Methods

### `ScriptResultstructenjin2_1_1ScriptResultcompound convertResult(const LuaResult &luaResult)`

Convert  to . LuaResultstructenjin2_1_1LuaResultcompoundScriptResultstructenjin2_1_1ScriptResultcompound

paramluaResultLua execution result returnScript execution result 

---

