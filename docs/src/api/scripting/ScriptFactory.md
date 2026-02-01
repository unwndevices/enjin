---
id: ScriptFactory
title: ScriptFactory
sidebar_label: ScriptFactory
---

# ScriptFactory

Script system factory for creating platform-specific interpreters. 



    

---

**Namespace:** `enjin2`

**Header:** `include/enjin2/scripting/script_interface.hpp`

## Public Methods

### `static std::unique_ptr<  >IScriptInterpreterclassenjin2_1_1IScriptInterpretercompound createInterpreter(InterpreterType type)`

Create interpreter for current platform. 

paramtypeInterpreter type to create returnUnique pointer to interpreter instance 

---

### `static InterpreterTypeclassenjin2_1_1ScriptFactory_1a93634331864cee4a0924b2a1982b702amember getRecommendedInterpreter()`

Get recommended interpreter for current platform. 

returnRecommended interpreter type 

---

### `static bool isInterpreterAvailable(InterpreterType type)`

Check if interpreter type is available on current platform. 

paramtypeInterpreter type to check returnTrue if available 

---

