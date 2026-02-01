---
title: Scene Transitions
---

# Scene Transitions

Transitions provide visual effects when switching between scenes.

## Transition Types

`TransitionType::IMMEDIATE` - Instant switch with no effect.

`TransitionType::FADE_OUT_IN` - Fade out current scene, fade in new scene.

`TransitionType::SLIDE_LEFT` - Slide scenes horizontally left.

`TransitionType::SLIDE_RIGHT` - Slide scenes horizontally right.

`TransitionType::SLIDE_UP` - Slide scenes vertically up.

`TransitionType::SLIDE_DOWN` - Slide scenes vertically down.

## Instant Transitions

Immediate transitions switch scenes with no delay.

```cpp
stateMachine.changeScene(1, enjin2::TransitionType::IMMEDIATE);
```

## Custom Transitions

Use `SceneStateMachine` for timed transitions.

```cpp
stateMachine.changeScene(2, enjin2::TransitionType::FADE_OUT_IN, 500); // 500ms duration
```

## Transition Timing

Control transition duration in milliseconds.

```cpp
uint16_t duration = 1000; // 1 second
stateMachine.changeScene(1, enjin2::TransitionType::FADE_OUT_IN, duration);
```

Default duration is 500ms.

## Common Effects

**Fade** - Use `FADE_OUT_IN` for smooth scene changes.

**Slide** - Use directional slide transitions for menu navigation.

**Instant** - Use immediate transitions for gameplay state changes.

## Performance

Transitions update both scenes simultaneously: minimize objects in transitioning scenes.

Avoid complex rendering during transitions: simple scenes fade faster.

Preload assets in `onCreate()` before transition starts.

## Monitoring

Check transition state during execution.

```cpp
if (stateMachine.isTransitioning()) {
    float progress = stateMachine.getTransitionProgress();
}
```

See [API Reference](/api) for complete transition API.
