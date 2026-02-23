---
id: C_Canvas
title: C_Canvas
sidebar_label: C_Canvas
---

# C_Canvas

Canvas component for custom drawing operations. 


A drawable component that wraps an internal canvas for custom graphics operations. Supports multiple blend modes for composition. Based on original Enjin . C_Canvasclassenjin2_1_1C__Canvascompound

---

**Namespace:** `enjin2`

**Header:** `include/enjin2/components/canvas.hpp`

## Public Methods

### ` C_Canvas(Object *owner, uint8_t width, uint8_t height)`

Constructor. 

paramownerclassenjin2_1_1Component_1a3349b9210509e148f9f7625f6a39b022memberParent object widthclassenjin2_1_1C__Drawable_1ac6e1a43e762d6c1ba2f9b04b981517ffmemberCanvas width in pixels heightclassenjin2_1_1C__Drawable_1ae67d8735cdc3935c362d48f4973caf75memberCanvas height in pixels 

---

### ` ~C_Canvas()=default`

Destructor. 


        

---

### `virtual void awake() override`

Awake is called when the component is created. 

Use this for initialization that doesn't depend on other components. This is called before Start(). 

---

### `virtual void start() override`

Start is called before the first frame update. 

Use this for initialization that depends on other components or objects being fully set up. 

---

### `virtual void update(uint16_t deltaTime) override`

Update is called once per frame. 

paramdeltaTimeTime since last frame in milliseconds 

---

### `virtual void lateUpdate(uint16_t deltaTime) override`

LateUpdate is called after all Update calls. 

paramdeltaTimeTime since last frame in milliseconds 

---

### `virtual void draw(ICanvas&lt; uint8_t &gt; &canvas) override`

Pure virtual draw method - must be implemented by derived classes. 

paramcanvasThe 8-bit canvas to draw to (matches original Enjin GFXcanvas8) 

---

### `virtual bool continueToDraw() const override const`

Check if this drawable should continue to be drawn. 

returnTrue if should continue drawing, false otherwise 

---

### `&lt; W, H &gt; &Canvas8classenjin2_1_1Canvas8compound getCanvas()`

Get access to internal canvas for drawing. 

returnReference to internal canvas 

---

### `const &lt; W, H &gt; &Canvas8classenjin2_1_1Canvas8compound getCanvas() const const`

Get const access to internal canvas. 

returnConst reference to internal canvas 

---

### `void clear(uint8_t color=0)`

Clear the canvas. 

paramcolorFill color (0-15) 

---

### `uint8_t getWidth() const const`

Get canvas width. 

returnWidth in pixels 

---

### `uint8_t getHeight() const const`

Get canvas height. 

returnHeight in pixels 

---

### `void setMatteColor(uint8_t matte)`

Set matte color (transparent color that won't be drawn). 

parammatteMatte color value (default: 16 for compatibility) 

---

### `uint8_t getMatteColor() const const`

Get current matte color. 

returnCurrent matte color 

---

## Private Methods

### `void createCanvas(uint8_t width, uint8_t height)`

Create internal canvas of specified size. 

paramwidthclassenjin2_1_1C__Drawable_1ac6e1a43e762d6c1ba2f9b04b981517ffmemberCanvas width heightclassenjin2_1_1C__Drawable_1ae67d8735cdc3935c362d48f4973caf75memberCanvas height 

---

### `void applyBlendMode(ICanvas&lt; uint8_t &gt; &target_canvas)`

Apply blend mode when drawing to target canvas. 

paramtarget_canvasTarget canvas to draw to 

---

