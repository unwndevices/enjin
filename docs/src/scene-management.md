---
title: Scene Management
---

# Scene Management

Scenes organize game states into manageable units.

## Scene Lifecycle

Scenes implement six lifecycle methods: `onCreate()` → `onActivate()` → `onUpdate()` → `onRender()` → `onDeactivate()` → `onDestroy()`.

```cpp
class MyScene : public enjin2::Scene {
    void onCreate() override {
        // Create objects and initialize
    }

    void onActivate() override {
        // Resume when scene becomes active
    }

    void onUpdate(uint16_t deltaTime) override {
        // Update game logic every frame
    }

    void onRender(enjin2::ICanvas<enjin2::Pixel4>& canvas) override {
        // Render scene graphics
    }

    void onDeactivate() override {
        // Pause when scene becomes inactive
    }

    void onDestroy() override {
        // Clean up resources
    }
};
```

## Creating Scenes

Add scenes to the state machine with unique IDs.

```cpp
enjin2::SceneStateMachine stateMachine;
auto menuScene = stateMachine.addScene<MyScene>(1, args...);
auto gameScene = stateMachine.addScene<GameScene>(2, args...);
```

## Scene State Machine

The state machine manages scene transitions.

```cpp
stateMachine.changeScene(1); // Switch to scene ID 1
```

Initialize the state machine before first use.

```cpp
stateMachine.changeScene(1);
```

## Adding Objects

Add objects to scenes with `addObject<T>()`.

```cpp
auto player = scene.addObject<Player>();
auto enemy = scene.addObject<Enemy>();
```

## Scene Properties

`getId()` - Get scene identifier.

`isActive()` - Check if scene is currently active.

`isInitialized()` - Check if scene has been initialized.

`getObjects()` - Access object collection.

## Transitions

Scene transitions are managed by the state machine (see [Scene Transitions](./scene-transitions.md)).

Common pattern: create a "Scene" base class with shared setup, then derive specific scenes.

See *Note: API Reference documentation will be available in next phase.* for complete scene API.

## See Also

- [API: Scene](/api/core/Scene)
- [API: SceneStateMachine](/api/core/SceneStateMachine)
- [API: SceneTransition](/api/core/SceneStateMachine)
