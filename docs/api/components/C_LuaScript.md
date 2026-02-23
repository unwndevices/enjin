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

paramownerclassenjin2_1_1Component_1a3349b9210509e148f9f7625f6a39b022memberOwner object widthclassenjin2_1_1C__Drawable_1ac6e1a43e762d6c1ba2f9b04b981517ffmember width Componentclassenjin2_1_1Componentcompoundheightclassenjin2_1_1C__Drawable_1ae67d8735cdc3935c362d48f4973caf75member height Componentclassenjin2_1_1Componentcompound

---

### ` C_LuaScript(Object *owner, uint16_t width, uint16_t height, ScriptFactory::InterpreterType interpreterType)`

Constructor with specific interpreter type. 

paramownerclassenjin2_1_1Component_1a3349b9210509e148f9f7625f6a39b022memberOwner object widthclassenjin2_1_1C__Drawable_1ac6e1a43e762d6c1ba2f9b04b981517ffmember width Componentclassenjin2_1_1Componentcompoundheightclassenjin2_1_1C__Drawable_1ae67d8735cdc3935c362d48f4973caf75member height Componentclassenjin2_1_1ComponentcompoundinterpreterTypeSpecific interpreter to use 

---

### ` ~C_LuaScript()`

Destructor. 


        

---

### `bool loadScript(const std::string &code)`

Load script from string. 

paramcodeLua script code returnTrue if loaded successfully 

---

### `bool loadScriptFile(const std::string &filename)`

Load script from file. 

paramfilenamePath to Lua script file returnTrue if loaded successfully 

---

### `bool reloadScript()`

Reload current script (useful for development). 

returnTrue if reloaded successfully 

---

### `void clearScript()`

Clear current script. 


        

---

### `bool hasLoadedScript() const const`

Check if script is loaded. 

returnTrue if script is loaded 

---

### `bool hasErrors() const const`

Check if script has errors. 

returnTrue if script has errors 

---

### `const std::string & getErrorMessage() const const`

Get last error message. 

returnError message string 

---

### `void setScriptVar(const std::string &name, double value)`

Set script variable (expose game state to script). 

paramnameVariable name valueVariable value 

---

### `void setScriptVar(const std::string &name, const std::string &value)`

Set script string variable. 

paramnameVariable name valueString value 

---

### `void setScriptVar(const std::string &name, bool value)`

Set script boolean variable. 

paramnameVariable name valueBoolean value 

---

### `double getScriptNumber(const std::string &name, double defaultValue=0.0)`

Get script variable. 

paramnameVariable name defaultValueDefault value if not found returnVariable value 

---

### `std::string getScriptString(const std::string &name, const std::string &defaultValue="")`

Get script string variable. 

paramnameVariable name defaultValueDefault value if not found returnString value 

---

### `bool getScriptBool(const std::string &name, bool defaultValue=false)`

Get script boolean variable. 

paramnameVariable name defaultValueDefault value if not found returnBoolean value 

---

### `bool callScriptFunction(const std::string &functionName)`

Call custom script function. 

paramfunctionNameFunction name to call returnTrue if call was successful 

---

### `virtual void update(uint16_t deltaTime) override`

Update component (calls script update function). 

paramdeltaTimeTime since last update 

---

### `void draw(ICanvas&lt; Pixel4 &gt; &canvas) override`

Draw component using 4-bit canvas. 

paramcanvas4-bit canvas to draw on 

---

### `virtual void draw(ICanvas&lt; uint8_t &gt; &canvas) override`

Draw component using 8-bit canvas. 

paramcanvas8-bit canvas to draw on 

---

### ` *IScriptInterpreterclassenjin2_1_1IScriptInterpretercompound getInterpreter()`

Get script interpreter for advanced operations. 

returnScript interpreter reference 

---

### ` *IScriptGraphicsclassenjin2_1_1IScriptGraphicscompound getGraphics()`

Get graphics interface. 

returnGraphics interface reference 

---

### `const char * getInterpreterType() const const`

Get interpreter type name. 

returnType name string 

---

### `uint32_t getDrawCalls() const const`

Get performance stats. 

returnNumber of draw calls since creation 

---

## Private Methods

### `bool initializeInterpreter(ScriptFactory::InterpreterType interpreterType)`

Initialize script interpreter and graphics. 

paraminterpreterTypeType of interpreter to create returnTrue if initialization successful 

---

### `bool executeScript(const std::string &code)`

Execute script with error handling. 

paramcodeScript code to execute returnTrue if execution successful 

---

### `bool callScriptFunctionSafe(const std::string &functionName)`

Call script function with error handling. 

paramfunctionNameFunction name returnTrue if call successful 

---

### `void setupLuaCanvas(CanvasType &canvas)`

Setup Lua canvas for current drawing context. 

paramcanvasCanvas to wrap 

---

### `void handleScriptError(const ScriptResult &result)`

 script error. Handlestructenjin2_1_1Handlecompound

paramresultScript execution result 

---

