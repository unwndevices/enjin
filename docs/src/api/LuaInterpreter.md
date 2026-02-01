---
id: LuaInterpreter
title: LuaInterpreter
sidebar_label: LuaInterpreter
---

# LuaInterpreter

Full Lua interpreter implementation. 


Desktop/VCV implementation using full Lua with complete enjin2 bindings. Provides all Lua features and comprehensive graphics API. 

---

**Namespace:** enjin2

**Header:** include/enjin2/scripting/lua_interpreter.hpp

## Public Methods

### `cpp
* LuaInterpreter()*
``

Constructor. 


        

---

### `cpp
* ~LuaInterpreter() override*
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

### `cpp
* *LuaEngineclassenjin2_1_1LuaEnginecompound getLuaEngine()*
``

Get access to underlying Lua engine (for advanced operations). 

returnLua engine reference 

---

### `cpp
* *LuaBindingsclassenjin2_1_1LuaBindingscompound getLuaBindings()*
``

Get access to Lua bindings. 

returnLua bindings reference 

---

## Private Methods

### `cpp
*ScriptResultstructenjin2_1_1ScriptResultcompound convertResult(const LuaResult &luaResult)*
``

Convert  to . LuaResultstructenjin2_1_1LuaResultcompoundScriptResultstructenjin2_1_1ScriptResultcompound

paramluaResultLua execution result returnScript execution result 

---

