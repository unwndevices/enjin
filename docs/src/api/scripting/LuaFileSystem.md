---
id: LuaFileSystem
title: LuaFileSystem
sidebar_label: LuaFileSystem
---

# LuaFileSystem

Platform-specific file system interface for Lua scripts. 



    

---

**Namespace:** `enjin2`

**Header:** `include/enjin2/scripting/lua_platform.hpp`

## Public Methods

### ``static bool isFileIOSupported()``

Check if file operations are supported on this platform. 

returnTrue if file I/O is available 

---

### ``static bool readScriptFile(const std::string &filename, std::string &content)``

Read script file content (platform-specific). 

paramfilenameScript file path contentOutput buffer for file content returnTrue if successful 

---

### ``static std::vector< std::string > listScriptFiles(const std::string &path="")``

List available script files (platform-specific). 

parampathDirectory path to search returnVector of script filenames 

---

