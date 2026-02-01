---
title: Architecture
---

# Architecture

enjin2 is built on five core design principles.

## Component-Based Design

Game entities are composed from reusable components rather than deep inheritance hierarchies.

Each component provides specific functionality: position, rendering, physics, UI elements, etc.

Components communicate through their owner object, enabling flexible composition.

## Static Allocation

All memory allocation is static, eliminating heap fragmentation and ensuring predictable performance.

Maximum component counts are defined at compile time: objects support up to 16 components, scenes up to 32.

This design is ideal for embedded systems with limited memory and hard real-time constraints.

## Hardware Acceleration

The `ICanvas` abstract interface separates rendering logic from hardware-specific implementations.

Platform-specific canvas implementations handle ESP32-S3 DMA, desktop blitting, and WebGL.

Operations like `drawRect()`, `drawLine()`, and `fill()` are optimized per-platform.

## Scene State Machine

The `SceneStateMachine` manages transitions between game states (menus, gameplay, pause screens).

Scenes have a defined lifecycle: `onCreate()` → `onActivate()` → `onUpdate()` → `onRender()` → `onDeactivate()` → `onDestroy()`.

Transition types include instant, fade, and slide effects with customizable timing.

## Event System

Signals provide a type-safe callback mechanism for decoupled systems.

Components connect to scene lifecycle events, input events, and custom notifications.

Signals support multiple listeners and automatic connection management.

## See Also

- [API: SceneStateMachine](/api/core/SceneStateMachine)
- [API: Signal](/api/core/Signal)
- [API: ICanvas](/api/abstract/ICanvas)
