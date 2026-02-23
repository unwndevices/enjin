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

gfxGraphics interface to use 

---

### `virtual bool initialize() override`

Initialize the Lua interpreter. 

True if initialization successful 

---

### `virtual void shutdown() override`

Shutdown the Lua interpreter. 


        

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

### ` *LuaEngine getLuaEngine()`

Get access to underlying Lua engine (for advanced operations). 

Lua engine reference 

---

### ` *LuaBindings getLuaBindings()`

Get access to Lua bindings. 

Lua bindings reference 

---

## Private Methods

### `ScriptResult convertResult(const LuaResult &luaResult)`

Convert  to . LuaResultScriptResult

luaResultLua execution result Script execution result 

---

