---
id: RenderSystem
title: RenderSystem
sidebar_label: RenderSystem
---

# RenderSystem

Rendering system for drawing entities to canvas. 


TCanvasCanvas type for rendering
Renders all visible entities with RenderComponent to the target canvas. Handles z-ordering and shape rendering. 

---

**Namespace:** `enjin2`

**Header:** `include/enjin2/ui/systems.hpp`

## Public Methods

### ` RenderSystem(TCanvas *targetCanvas)`

Constructor with target canvas. 

targetCanvasCanvas to render to 

---

### `virtual void update(float dt) override`

Update rendering. 

dtTime since last update in seconds 

---

### `virtual int getPriority() const override const`

Get system priority (rendering should run last). 

Priority value 

---

## Private Methods

### `void renderEntity(Entity entity)`

Render individual entity. 

entityEntity to render 

---

### `void renderShape(const PositionComponent &pos, const SizeComponent *size, const RenderComponent &render, const ShapeComponent &shape)`

Render shape component. 

posPosition component sizeSize component (may be null) renderRender component shapeShape component 

---

### `void renderRectangle(const PositionComponent &pos, const SizeComponent &size, const RenderComponent &render)`

Render simple rectangle. 

posPosition component sizeSize component renderRender component 

---

