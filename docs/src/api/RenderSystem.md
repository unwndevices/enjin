---
id: RenderSystem
title: RenderSystem
sidebar_label: RenderSystem
---

# RenderSystem

Rendering system for drawing entities to canvas. 



Renders all visible entities with  to the target canvas. Handles z-ordering and shape rendering. templateparamTCanvasCanvas type for renderingRenderComponentstructenjin2_1_1RenderComponentcompound

---

**Namespace:** enjin2

**Header:** include/enjin2/ui/systems.hpp

## Public Methods

### `cpp
* RenderSystem(TCanvas *targetCanvas)*
``

Constructor with target canvas. 

paramtargetCanvasCanvas to render to 

---

### `cpp
*virtual void update(float deltaTime) override*
``

Update rendering. 

paramdeltaTimeTime since last update 

---

### `cpp
*virtual int getPriority() const override const*
``

Get system priority (rendering should run last). 

returnPriority value 

---

## Private Methods

### `cpp
*void renderEntity(Entity entity)*
``

Render individual entity. 

paramentity to render Entitystructenjin2_1_1Entitycompound

---

### `cpp
*void renderShape(const PositionComponent &pos, const SizeComponent *size, const RenderComponent &render, const ShapeComponent &shape)*
``

Render shape component. 

paramposPosition component size component (may be null) Sizestructenjin2_1_1SizecompoundrenderRender component shapeShape component 

---

### `cpp
*void renderRectangle(const PositionComponent &pos, const SizeComponent &size, const RenderComponent &render)*
``

Render simple rectangle. 

paramposPosition component size component Sizestructenjin2_1_1SizecompoundrenderRender component 

---

