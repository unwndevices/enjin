---
id: LuaBindings
title: LuaBindings
sidebar_label: LuaBindings
---

# LuaBindings

Lua bindings for Enjin graphics and UI. 


Provides love2d.graphics-style API for familiar Lua scripting. All functions are registered as global Lua functions. 

---

**Namespace:** enjin2

**Header:** include/enjin2/scripting/bindings.hpp

## Public Methods

### `cpp
* LuaBindings(LuaEngine *luaEngine)*
``

Constructor. 

paramluaEngineLua engine to bind to 

---

### `cpp
*void registerAll()*
``

Register all bindings with Lua engine. 


        

---

### `cpp
*void setCanvas(LuaCanvas *canvas)*
``

Set current canvas for drawing operations. 

paramcanvasCanvas to draw on 

---

### `cpp
* *LuaCanvasclassenjin2_1_1LuaCanvascompound getCanvas() const const*
``

Get current canvas. 

returnCurrent canvas or nullptr 

---

## Private Methods

### `cpp
*static int lua_getWidth(lua_State *L)*
``


        


        

---

### `cpp
*static int lua_getHeight(lua_State *L)*
``


        


        

---

### `cpp
*static int lua_clear(lua_State *L)*
``


        


        

---

### `cpp
*static int lua_setColor(lua_State *L)*
``


        


        

---

### `cpp
*static int lua_getColor(lua_State *L)*
``


        


        

---

### `cpp
*static int lua_setLineWidth(lua_State *L)*
``


        


        

---

### `cpp
*static int lua_getLineWidth(lua_State *L)*
``


        


        

---

### `cpp
*static int lua_point(lua_State *L)*
``


        


        

---

### `cpp
*static int lua_line(lua_State *L)*
``


        


        

---

### `cpp
*static int lua_rectangle(lua_State *L)*
``


        


        

---

### `cpp
*static int lua_circle(lua_State *L)*
``


        


        

---

### `cpp
*static int lua_triangle(lua_State *L)*
``


        


        

---

### `cpp
*static int lua_setPixel(lua_State *L)*
``


        


        

---

### `cpp
*static int lua_getPixel(lua_State *L)*
``


        


        

---

### `cpp
*static int lua_createEntity(lua_State *L)*
``


        


        

---

### `cpp
*static int lua_destroyEntity(lua_State *L)*
``


        


        

---

### `cpp
*static int lua_addComponent(lua_State *L)*
``


        


        

---

### `cpp
*static int lua_removeComponent(lua_State *L)*
``


        


        

---

### `cpp
*static int lua_getComponent(lua_State *L)*
``


        


        

---

### `cpp
*static int lua_print(lua_State *L)*
``


        


        

---

### `cpp
*static int lua_time(lua_State *L)*
``


        


        

---

### `cpp
*static int lua_fastFillRect(lua_State *L)*
``


        


        

---

### `cpp
*static int lua_fastDrawLine(lua_State *L)*
``


        


        

---

### `cpp
*static  *LuaBindingsclassenjin2_1_1LuaBindings_1ad0fe51d446be4e1b488a3319b421098cmember getBindings(lua_State *L)*
``

Get  instance from Lua state. LuaBindingsclassenjin2_1_1LuaBindingscompound

paramLLua state return instance LuaBindingsclassenjin2_1_1LuaBindingscompound

---

### `cpp
*void registerTable(const std::string &tableName, const std::vector&lt; std::pair&lt; std::string, lua_CFunction &gt; &gt; &functions)*
``

Register bindings in a table. 

paramtableNameName of table to create functionsArray of function bindings 

---

