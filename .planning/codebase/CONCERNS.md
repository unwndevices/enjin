# Codebase Concerns

## Technical Debt
- **Large Headers with Inline Logic:** `include/enjin2/graphics/canvas.hpp` is over 1200 lines and contains significant implementation logic inside the header, increasing compile times and reducing navigability.
- **Placeholder/Stubbed Effects:** `include/enjin2/graphics/effects.hpp` contains stubbed methods like `blur()` marked as placeholders.
- **Fixed Limits:** `include/enjin2/core/scene_state_machine.hpp` uses a hardcoded `MAX_SCENES = 32`, which may be limiting for complex applications.

## Security Considerations
- **Manual Memory Management:** Direct use of `memcpy` and manual buffer management in `src/scripting/lua_engine.cpp` and `src/components/image_cache.cpp` presents a risk of buffer overflows if bounds are not strictly validated.

## Performance Bottlenecks
- **Virtual setPixel Calls:** The `ICanvas::setPixel` method is virtual. Calling it for every individual pixel in high-frequency drawing operations (like `primitives.cpp`) introduces polymorphic overhead. Batch operations should be prioritized.
- **Temporary Buffer Allocations:** Operations in `src/effects/postfx.cpp` and `src/components/image_cache.cpp` create temporary buffers on the fly, potentially leading to heap fragmentation on memory-constrained devices like ESP32.

## Fragile Areas
- **Scene State Machine:** The fixed-size array for scenes in `include/enjin2/core/scene_state_machine.hpp` lacks dynamic resizing and may fail silently or crash if the limit is exceeded.
