---
id: C_Draw
title: C_Draw
sidebar_label: C_Draw
---

# C_Draw

Draw component for lambda-based custom rendering (matches original Enjin ). C_Draw


Allows custom drawing operations to be performed via lambda functions, providing flexibility for procedural graphics and custom visual effects. 

---

**Namespace:** `enjin2`

**Header:** `include/enjin2/components/draw.hpp`

## Public Methods

### ` C_Draw(Object *owner, DrawFunction drawFunc=nullptr)`

Construct a new Draw component. 

ownerThe object that owns this component drawFuncOptional draw function to execute 

---

### `virtual void draw(ICanvas&lt; uint8_t &gt; &canvas) override`

Draw using the stored draw function. 

canvasThe canvas to draw to 

---

### `virtual bool continueToDraw() const override const`

Check if should continue drawing (matches original Enjin). 

True if object is not queued for removal 

---

### `void SetDrawFunction(DrawFunction drawFunc)`

Set the draw function. 

drawFuncFunction to execute when drawing 

---

### `const  &DrawFunction GetDrawFunction() const const`

Get the current draw function. 

Current draw function (may be nullptr) 

---

