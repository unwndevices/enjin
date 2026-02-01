---
title: Components
---

# Components

Components provide functionality to game objects through composition.

## Component Lifecycle

Components follow a four-phase lifecycle: `awake()` → `start()` → `update()` → `lateUpdate()`.

```cpp
class MyComponent : public enjin2::Component {
    void awake() override {
        // Initialize before start
    }

    void start() override {
        // Initialize after all components awake
    }

    void update(uint16_t deltaTime) override {
        // Update every frame
    }

    void lateUpdate(uint16_t deltaTime) override {
        // Update after all updates complete
    }
};
```

## Adding Components

Add components to objects with typed methods.

```cpp
auto obj = scene.addObject<Object>();
auto pos = obj->addComponent<C_Position>(10, 20);
auto render = obj->addComponent<Draw>(canvas);
```

## Accessing Components

Retrieve components by type from their owner object.

```cpp
auto pos = obj->getComponent<C_Position>();
if (pos) {
    pos->x += 1;
}
```

## Removing Components

Remove components when no longer needed.

```cpp
obj->removeComponent<C_Drawable>();
```

## Built-in Components

Common components include: `C_Position` (position), `C_Drawable` (rendering), `Sprite` (sprite rendering), `TextRenderer` (text), `Label` (UI labels), `Animation` (frame animation).

See *Note: API Reference documentation will be available in next phase.* for complete component list.

## See Also

- [API: Component](/api/core/Component)
- [API: Object](/api/core/Object)
- [API: SceneObject](/api/core/Object)
