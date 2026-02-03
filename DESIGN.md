# enjin2 Design

## Brief Description

enjin2 is a lightweight C++ game engine designed for embedded devices and resource-constrained environments.

Target platforms include ESP32-S3 microcontrollers and desktop operating systems (Linux, Windows).

The library enables game development on hardware with limited memory while providing modern game engine features.

## Objectives

Provide static allocation throughout the entire codebase, eliminating dynamic memory usage completely.

Support Lua and WebAssembly integration for flexible game logic scripting.

Enable multi-platform development across embedded and desktop environments.

Deliver predictable performance suitable for real-time embedded systems.

Maintain clean, modular architecture for long-term maintainability and extensibility.

## Technical Specifications

**Language:** C++17 standard

**Architecture:** Component-based entity system

**Memory Management:** Static allocation with compile-time resource limits

**Rendering:** Hardware-abstracted via ICanvas interface

**Scene Management:** State machine with transition effects

**Modules:** Core, Graphics, UI, Scripting, Utils, Animation, Effects

**Platform Support:** ESP32-S3, Linux, Windows

**Dependencies:** Minimal external libraries (Lua, Adafruit-GFX, stb_image)

**Code Size:** Approximately 28,271 lines of C++ code

**Component Limits:** Up to 16 components per object, 32 objects per scene

**Pixel Types:** 8-bit grayscale (Canvas8) and 16-bit RGB565 (Canvas16)

**Font Rendering:** Adafruit GFX library integration

**Image Loading:** stb_image for bitmap and PNG support

## Unique Elements

**Zero Dynamic Memory:** All memory allocation is static, eliminating heap fragmentation and enabling predictable performance.

**Dual Scripting Support:** Lua and WebAssembly integration for game logic across different platforms.

**Multi-Platform Canvas Abstraction:** Single ICanvas interface supports ESP32-S3 DMA, desktop blitting, and WebGL rendering.

**Compile-Time Resource Limits:** Maximum component counts defined at compile time for predictable resource usage.

**Signal-Based Event System:** Type-safe callback mechanism for decoupled component communication.

**Hierarchical Scene System:** Nested scene hierarchy with smooth transition effects (fade, slide, instant).

**Integrated UI Component Library:** Complete UI widget system including buttons, sliders, panels, and text input.

**Platform-Neutral Graphics:** Hardware-accelerated rendering operations optimized per target platform.

**Lifecycle Management:** Defined lifecycle methods (awake, start, update) for controlled component initialization.
