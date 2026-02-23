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

### `static std::unique_ptr&lt; IScriptInterpreter &gt; createInterpreter(InterpreterType type)`

Create interpreter for current platform. 

typeInterpreter type to create Unique pointer to interpreter instance 

---

### `static InterpreterType getRecommendedInterpreter()`

Get recommended interpreter for current platform. 

Recommended interpreter type 

---

### `static bool isInterpreterAvailable(InterpreterType type)`

Check if interpreter type is available on current platform. 

typeInterpreter type to check True if available 

---

