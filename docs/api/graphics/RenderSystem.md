---
id: RenderSystem
title: RenderSystem
sidebar_label: RenderSystem
---

# RenderSystem

Rendering system for drawing entities to canvas. 



Renders all visible entities with  to the target canvas. Handles z-ordering and shape rendering. TCanvasCanvas type for renderingRenderComponent

---

**Namespace:** `enjin2`

**Header:** `include/enjin2/ui/systems.hpp`

## Public Methods

### ` RenderSystem(TCanvas *targetCanvas)`

Constructor with target canvas. 

targetCanvasCanvas to render to 

---

### `virtual void update(float deltaTime) override`

Update rendering. 

deltaTimeTime since last update 

---

### `virtual int getPriority() const override const`

Get system priority (rendering should run last). 

Priority value 

---

## Private Methods

### `void renderEntity(Entity entity)`

Render individual entity. 

entity to render Entity

---

### `void renderShape(const PositionComponent &pos, const SizeComponent *size, const RenderComponent &render, const ShapeComponent &shape)`

Render shape component. 

posPosition component size component (may be null) SizerenderRender component shapeShape component 

---

### `void renderRectangle(const PositionComponent &pos, const SizeComponent &size, const RenderComponent &render)`

Render simple rectangle. 

posPosition component size component SizerenderRender component 

---

