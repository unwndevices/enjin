---
id: C_LuaScript
title: C_LuaScript
sidebar_label: C_LuaScript
---

# C_LuaScript

Platform-agnostic script-driven UI component. 


A drawable component that executes scripts for custom UI rendering. Uses platform-specific interpreters (full Lua on desktop, minimal on ESP32). Perfect for prototyping UI elements, data visualization, and custom effects. 

---

**Namespace:** `enjin2`

**Header:** `include/enjin2/components/lua_script.hpp`

## Public Methods

### ` C_LuaScript(Object *owner, uint16_t width, uint16_t height)`

Constructor with automatic interpreter selection. 

ownerOwner object widthComponent width heightComponent height 

---

### ` C_LuaScript(Object *owner, uint16_t width, uint16_t height, ScriptFactory::InterpreterType interpreterType)`

Constructor with specific interpreter type. 

ownerOwner object widthComponent width heightComponent height interpreterTypeSpecific interpreter to use 

---

### ` ~C_LuaScript()`

Destructor. 

---

### `bool loadScript(const std::string &code)`

Load script from string. 

codeLua script code True if loaded successfully 

---

### `bool loadScriptFile(const std::string &filename)`

Load script from file. 

filenamePath to Lua script file True if loaded successfully 

---

### `bool reloadScript()`

Reload current script (useful for development). 

True if reloaded successfully 

---

### `void clearScript()`

Clear current script. 

---

### `bool hasLoadedScript() const`

Check if script is loaded. 

True if script is loaded 

---

### `bool hasErrors() const`

Check if script has errors. 

True if script has errors 

---

### `const std::string & getErrorMessage() const`

Get last error message. 

Error message string 

---

### `void setScriptVar(const std::string &name, double value)`

Set script variable (expose game state to script). 

nameVariable name valueVariable value 

---

### `void setScriptVar(const std::string &name, const std::string &value)`

Set script string variable. 

nameVariable name valueString value 

---

### `void setScriptVar(const std::string &name, bool value)`

Set script boolean variable. 

nameVariable name valueBoolean value 

---

### `double getScriptNumber(const std::string &name, double defaultValue=0.0)`

Get script variable. 

nameVariable name defaultValueDefault value if not found Variable value 

---

### `std::string getScriptString(const std::string &name, const std::string &defaultValue="")`

Get script string variable. 

nameVariable name defaultValueDefault value if not found String value 

---

### `bool getScriptBool(const std::string &name, bool defaultValue=false)`

Get script boolean variable. 

nameVariable name defaultValueDefault value if not found Boolean value 

---

### `bool callScriptFunction(const std::string &functionName)`

Call custom script function. 

functionNameFunction name to call True if call was successful 

---

### `virtual void update(uint16_t deltaTime) override`

Update component (calls script update function). 

deltaTimeTime since last update 

---

### `void draw(ICanvas&lt; Pixel4 &gt; &canvas) override`

Draw component using 4-bit canvas. 

canvas4-bit canvas to draw on 

---

### `virtual void draw(ICanvas&lt; uint8_t &gt; &canvas) override`

Draw component using 8-bit canvas. 

canvas8-bit canvas to draw on 

---

### `IScriptInterpreter * getInterpreter()`

Get script interpreter for advanced operations. 

Script interpreter reference 

---

### `IScriptGraphics * getGraphics()`

Get graphics interface. 

Graphics interface reference 

---

### `const char * getInterpreterType() const`

Get interpreter type name. 

Type name string 

---

### `uint32_t getDrawCalls() const`

Get performance stats. 

Number of draw calls since creation 

---

## Private Methods

### `bool initializeInterpreter(ScriptFactory::InterpreterType interpreterType)`

Initialize script interpreter and graphics. 

interpreterTypeType of interpreter to create True if initialization successful 

---

### `bool executeScript(const std::string &code)`

Execute script with error handling. 

codeScript code to execute True if execution successful 

---

### `bool callScriptFunctionSafe(const std::string &functionName)`

Call script function with error handling. 

functionNameFunction name True if call successful 

---

### `void setupLuaCanvas(CanvasType &canvas)`

Setup Lua canvas for current drawing context. 

canvasCanvas to wrap 

---

### `void handleScriptError(const ScriptResult &result)`

Handle script error. 

resultScript execution result 

---

