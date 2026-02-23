---
id: C_Draw
title: C_Draw
sidebar_label: C_Draw
---

# C_Draw

Draw component for lambda-based custom rendering (matches original Enjin ). C_Drawclassenjin2_1_1C__Drawcompound


Allows custom drawing operations to be performed via lambda functions, providing flexibility for procedural graphics and custom visual effects. 

---

**Namespace:** `enjin2`

**Header:** `include/enjin2/components/draw.hpp`

## Public Methods

### ` C_Draw(Object *owner, DrawFunction drawFunc=nullptr)`

Construct a new Draw component. 

paramownerclassenjin2_1_1Component_1a3349b9210509e148f9f7625f6a39b022memberThe object that owns this component drawFuncOptional draw function to execute 

---

### `virtual void draw(ICanvas&lt; uint8_t &gt; &canvas) override`

Draw using the stored draw function. 

paramcanvasThe canvas to draw to 

---

### `virtual bool continueToDraw() const override const`

Check if should continue drawing (matches original Enjin). 

returnTrue if object is not queued for removal 

---

### `void SetDrawFunction(DrawFunction drawFunc)`

Set the draw function. 

paramdrawFuncFunction to execute when drawing 

---

### `const  &DrawFunctiondraw_8hpp_1a330a52b9f21118f8ddf7397f0026ef8cmember GetDrawFunction() const const`

Get the current draw function. 

returnCurrent draw function (may be nullptr) 

---

