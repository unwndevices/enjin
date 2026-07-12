---
id: C_Canvas
title: C_Canvas
sidebar_label: C_Canvas
---

# C_Canvas

Canvas component for custom drawing operations. 


A drawable component that wraps an internal canvas for custom graphics operations. Supports multiple blend modes for composition. Based on original Enjin C_Canvas. 

---

**Namespace:** `enjin2`

**Header:** `include/enjin2/components/canvas.hpp`

## Public Methods

### ` C_Canvas(Object *owner, uint16_t width, uint16_t height)`

Constructor. 

ownerParent object widthCanvas width in pixels heightCanvas height in pixels 

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

### `virtual void update(float dt) override`

Update is called once per frame. 

dtTime since last frame in seconds 

---

### `virtual void lateUpdate(float dt) override`

LateUpdate is called after all Update calls. 

dtTime since last frame in seconds 

---

### `virtual void draw(ICanvas&lt; Pixel4 &gt; &canvas) override`

Pure virtual draw method - must be implemented by derived classes. 

canvasThe 4-bit canvas targeting ICanvas&lt;Pixel4&gt;

---

### `virtual bool continueToDraw() const override const`

Check if this drawable should continue to be drawn. 

True if should continue drawing, false otherwise 

---

### `Canvas8&lt; W, H &gt; & getCanvas()`

Get access to internal canvas for drawing. 

Reference to internal canvas 

---

### `const Canvas8&lt; W, H &gt; & getCanvas() const`

Get const access to internal canvas. 

Const reference to internal canvas 

---

### `void clear(uint8_t color=0)`

Clear the canvas. 

colorFill color (0-15) 

---

### `uint16_t getWidth() const`

Get canvas width. 

Width in pixels 

---

### `uint16_t getHeight() const`

Get canvas height. 

Height in pixels 

---

### `void setMatteColor(uint8_t matte)`

Set matte color (transparent color that won't be drawn). 

matteMatte color value (default: 16 for compatibility) 

---

### `uint8_t getMatteColor() const`

Get current matte color. 

Current matte color 

---

## Private Methods

### `void createCanvas(uint16_t width, uint16_t height)`

Create internal canvas of specified size. 

widthCanvas width heightCanvas height 

---

### `void applyBlendMode(ICanvas&lt; Pixel4 &gt; &target_canvas)`

Apply blend mode when drawing to target canvas (deferred — ENG-01). 

target_canvasTarget canvas to draw to 

---

