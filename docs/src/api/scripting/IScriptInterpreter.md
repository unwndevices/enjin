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

### ``virtual  ~IScriptInterpreter()=default``

Virtual destructor. 


        

---

### ``bool initialize()=0``

Initialize the interpreter. 

returnTrue if initialization successful 

---

### ``void shutdown()=0``

Shutdown the interpreter. 


        

---

### ``bool isInitialized() const =0 const``

Check if interpreter is initialized. 

returnTrue if initialized 

---

### ``ScriptResultstructenjin2_1_1ScriptResultcompound executeString(const std::string &code)=0``

Execute script code string. 

paramcodeScript code to execute returnExecution result 

---

### ``ScriptResultstructenjin2_1_1ScriptResultcompound executeFile(const std::string &filename)=0``

Load and execute script file. 

paramfilenamePath to script file returnExecution result 

---

### ``ScriptResultstructenjin2_1_1ScriptResultcompound callFunction(const std::string &functionName)=0``

Call script function. 

paramfunctionNameName of function to call returnExecution result 

---

### ``void setGlobal(const std::string &name, double value)=0``

Set global variable. 

paramnameVariable name valueVariable value 

---

### ``void setGlobal(const std::string &name, const std::string &value)=0``


        


        

---

### ``void setGlobal(const std::string &name, bool value)=0``


        


        

---

### ``double getGlobalNumber(const std::string &name, double defaultValue=0.0)=0``

Get global variable. 

paramnameVariable name defaultValueDefault value if not found returnVariable value 

---

### ``std::string getGlobalString(const std::string &name, const std::string &defaultValue="")=0``


        


        

---

### ``bool getGlobalBool(const std::string &name, bool defaultValue=false)=0``


        


        

---

### ``size_t getMemoryUsage() const =0 const``

Get current memory usage. 

returnMemory usage in bytes 

---

### ``const char * getTypeName() const =0 const``

Get interpreter type name. 

returnType name string (e.g., "Lua", "JavaScript", "MicroPython") 

---

