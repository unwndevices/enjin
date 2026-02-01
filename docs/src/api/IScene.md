---
id: IScene
title: IScene
sidebar_label: IScene
---

# IScene

Abstract scene interface for scene lifecycle. 



Both enjin1 and enjin2 can implement this interface for compile-time polymorphism. Provides standard scene lifecycle methods (onCreate, onUpdate, onRender, etc.). templateparamPixelTypePixel type for rendering (e.g., , uint8_t)Pixel4structenjin2_1_1Pixel4compound

---

**Namespace:** enjin2

**Header:** include/enjin2/abstract/iscene.hpp

## Public Methods

### `cpp
*virtual  ~IScene()=default*
``

Virtual destructor for proper cleanup through base pointer. 


        

---

### `cpp
*void onCreate()=0*
``

Called when scene is created. 

Called once when the scene is first created. Override to set up initial objects and state. 

---

### `cpp
*void onActivate()=0*
``

Called when scene becomes active. 

Called when the scene becomes the active scene. Use this to resume animations, start background processes, etc. 

---

### `cpp
*void onDeactivate()=0*
``

Called when scene becomes inactive. 

Called when the scene is no longer active. Use this to pause animations, stop background processes, etc. 

---

### `cpp
*void onDestroy()=0*
``

Called when scene is destroyed. 

Use this to clean up scene-specific resources. 

---

### `cpp
*void onUpdate(uint16_t deltaTime)=0*
``

Called every frame during update. 


Use this for scene-specific update logic that should happen before object updates. paramdeltaTimeTime since last frame in milliseconds

---

### `cpp
*void onRender(ICanvas&lt; PixelType &gt; &canvas)=0*
``

Called during rendering. 


Use this for scene-specific rendering like backgrounds or UI overlays. paramcanvasTarget canvas for rendering

---

### `cpp
*uint32_t getId() const =0 const*
``

Get scene ID. 

return identifier Sceneclassenjin2_1_1Scenecompound

---

### `cpp
*bool isActive() const =0 const*
``

Check if scene is active. 

returnTrue if scene is active 

---

### `cpp
*bool isInitialized() const =0 const*
``

Check if scene is initialized. 

returnTrue if scene is initialized 

---

