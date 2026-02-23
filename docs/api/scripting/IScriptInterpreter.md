---
id: IScriptInterpreter
title: IScriptInterpreter
sidebar_label: IScriptInterpreter
---

# IScriptInterpreter

Platform-agnostic script interpreter interface. 


Abstract interface for script interpreters to allow platform-specific implementations (full Lua on desktop, lightweight interpreters on ESP32). 

---

**Namespace:** `enjin2`

**Header:** `include/enjin2/scripting/script_interface.hpp`

## Public Methods

### `virtual  ~IScriptInterpreter()=default`

Virtual destructor. 

---

### `bool initialize()=0`

Initialize the interpreter. 

True if initialization successful 

---

### `void shutdown()=0`

Shutdown the interpreter. 

---

### `bool isInitialized() const =0 const`

Check if interpreter is initialized. 

True if initialized 

---

### `ScriptResult executeString(const std::string &code)=0`

Execute script code string. 

codeScript code to execute Execution result 

---

### `ScriptResult executeFile(const std::string &filename)=0`

Load and execute script file. 

filenamePath to script file Execution result 

---

### `ScriptResult callFunction(const std::string &functionName)=0`

Call script function. 

functionNameName of function to call Execution result 

---

### `void setGlobal(const std::string &name, double value)=0`

Set global number variable. 

nameVariable name valueNumber value to set 

---

### `void setGlobal(const std::string &name, const std::string &value)=0`

Set global string variable. 

nameVariable name valueString value to set 

---

### `void setGlobal(const std::string &name, bool value)=0`

Set global boolean variable. 

nameVariable name valueBoolean value to set 

---

### `double getGlobalNumber(const std::string &name, double defaultValue=0.0)=0`

Get global number variable. 

nameVariable name defaultValueDefault value if not found Number value 

---

### `std::string getGlobalString(const std::string &name, const std::string &defaultValue="")=0`

Get global string variable. 

nameVariable name defaultValueDefault value if not found String value 

---

### `bool getGlobalBool(const std::string &name, bool defaultValue=false)=0`

Get global boolean variable. 

nameVariable name defaultValueDefault value if not found Boolean value 

---

### `size_t getMemoryUsage() const =0 const`

Get current memory usage. 

Memory usage in bytes 

---

### `const char * getTypeName() const =0 const`

Get interpreter type name. 

Type name string (e.g., "Lua", "JavaScript", "MicroPython") 

---

