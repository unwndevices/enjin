---
id: C_LuaScript
title: C_LuaScript
sidebar_label: C_LuaScript
---

# C_LuaScript

Platform-agnostic script-driven UI component. 


A drawable component that executes Lua scripts for custom UI rendering. Uses LuaScriptSystem for script execution and LuaCanvas for drawing. A ScriptProxy userdata is stored in the Lua registry and passed as the first argument to update(self, dt) and draw(self). 

---

**Namespace:** `enjin2`

**Header:** `include/enjin2/components/lua_script.hpp`

## Public Methods

### ` C_LuaScript(Object *owner, uint16_t width, uint16_t height)`

Constructor with automatic Lua interpreter selection. 

ownerOwner object widthComponent width heightComponent height 

---

### ` ~C_LuaScript()`

Destructor — invalidates ScriptProxy before closing Lua state. 

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

### `const char * getErrorMessage() const`

Get last error message. 

Error message string 

---

### `void setErrorPolicy(ScriptErrorPolicy policy)`

Set the error handling policy for this script. 

policyDisable (default), Log, or Panic 

---

### `ScriptErrorPolicy getErrorPolicy() const`

Get the current error handling policy. 

Current ScriptErrorPolicy

---

### `void setScriptVar(const std::string &name, double value)`

Set script number variable (expose game state to script). 

nameVariable name valueNumeric value 

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

Get script number variable. 

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

Call custom script function by name. 

functionNameFunction name to call True if call was successful 

---

### `virtual void update(float dt) override`

Update component (calls Lua update(self, dt)). 

dtTime since last update in seconds 

---

### `virtual void draw(ICanvas&lt; Pixel4 &gt; &canvas) override`

Draw component using 4-bit canvas (calls Lua draw(self)). 

canvas4-bit canvas to draw on 

---

### `uint32_t getDrawCalls() const`

Get performance stats. 

Number of draw calls since creation 

---

### `void setInput(InputState *input)`

Inject InputState for this frame (delegates to scriptSystem-&gt;getBindings().setInput()) Used by tests and host code to provide input state before calling update(). 

inputPointer to current frame's InputState; may be nullptr to clear 

---

### `LuaScriptSystem & getScriptSystem()`

Get the script system (for testing and host integration). 

Reference to the LuaScriptSystem

---

## Private Methods

### `bool initializeScriptSystem()`

Initialize LuaScriptSystem and expose component dimensions. 

True if initialization successful 

---

### `bool executeScript(const std::string &code)`

Execute script code with error handling. 

codeScript code to execute True if execution successful 

---

### `bool callScriptFunctionSafe(const std::string &functionName)`

Call a script lifecycle function with error handling (no proxy argument). 

functionNameFunction name True if call successful 

---

### `bool callWithProxy(const char *funcName, float dt, bool passDt)`

Push stored ScriptProxy userdata as first arg, then call Lua function via pcall. 

funcNameLua global function name ("init", "update", "draw") dtDelta time in seconds — only pushed if passDt is true passDtWhether to push dt as second argument (true for update, false for init/draw) true if function was found and called without error, false otherwise 

---

### `bool callWithProxyAndBtn(const char *funcName, int btn)`

Push stored ScriptProxy userdata as first arg, push btn integer as second arg, call Lua function. Used for on_button_pressed(self, btn) / on_button_released(self, btn) callbacks. Optional callback: if the function is not defined in the script, returns false silently. Error handling follows ScriptErrorPolicy (identical to callWithProxy). 

funcNameLua global function name btnButton index (0-15) true if function was found and called without Lua error, false otherwise 

---

### `void dispatchInputCallbacks(const InputState &input)`

Fire on_button_pressed / on_button_released callbacks for all button edges this frame. Called at the top of update() before callWithProxy(UPDATE_FUNCTION, ...) — satisfies INPUT-03. Skips dispatch if hasScript is false, scriptError is true, or scriptSystem is null. 

inputCurrent frame's InputState (buttons / prev_buttons already set) 

---

### `void setupLuaCanvas(CanvasType &canvas)`

Setup LuaCanvas wrapper for the current draw canvas. 

canvasCanvas to wrap 

---

### `void handleScriptError(const LuaResult &result)`

Handle script error — sets scriptError flag and records message. 

resultLuaResult from a failed execute or load call 

---

