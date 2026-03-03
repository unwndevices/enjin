---
title: Canvas
---

# Canvas

Canvas provides drawing operations for graphics rendering.

## Canvas Types

`Canvas8<T_WIDTH, T_HEIGHT>` - 8-bit grayscale canvas for compatibility.

`Canvas4<T_WIDTH, T_HEIGHT>` - 4-bit packed canvas for memory efficiency.

## Configuring Canvas Dimensions

The WASM and SDL build targets use a configurable canvas size controlled by two environment variables:

| Variable | Default | Description |
|---|---|---|
| `ENJIN2_CANVAS_WIDTH` | `128` | Canvas width in pixels (must be even) |
| `ENJIN2_CANVAS_HEIGHT` | `128` | Canvas height in pixels |

Set them before building:

```bash
# 320x240 WASM build
ENJIN2_CANVAS_WIDTH=320 ENJIN2_CANVAS_HEIGHT=240 ./build.sh --target wasm

# 320x240 SDL build
ENJIN2_CANVAS_WIDTH=320 ENJIN2_CANVAS_HEIGHT=240 ./build.sh --target sdl3
```

Or pass directly to CMake:

```bash
cmake -B build -DENJIN2_CANVAS_WIDTH=320 -DENJIN2_CANVAS_HEIGHT=240 ...
```

In JavaScript, query the configured size at runtime:

```js
const width = module.getCanvasWidth();   // e.g. 320
const height = module.getCanvasHeight(); // e.g. 240
```

## Creating a Canvas

```cpp
enjin2::Canvas8_128x64 canvas;
canvas.clear();
```

## Basic Drawing

`clear()` - Fill entire canvas with background color.

```cpp
canvas.clear(0); // Fill with black
```

`setPixel(x, y, color)` - Set individual pixel color.

```cpp
canvas.setPixel(10, 10, 15);
```

`getPixel(x, y)` - Read pixel color.

```cpp
uint8_t color = canvas.getPixel(10, 10);
```

## Shapes

`fillRect(x, y, w, h, color)` - Draw filled rectangle.

```cpp
canvas.fillRect(10, 10, 50, 30, 15);
```

`drawRect(x, y, w, h, color)` - Draw rectangle outline.

```cpp
canvas.drawRect(10, 10, 50, 30, 15);
```

`drawLine(x0, y0, x1, y1, color)` - Draw line between points.

```cpp
canvas.drawLine(0, 0, 127, 63, 15);
```

`drawCircle(x0, y0, radius, color)` - Draw circle outline.

`fillCircle(x0, y0, radius, color)` - Draw filled circle.

`fillTriangle(x0, y0, x1, y1, x2, y2, color)` - Draw filled triangle.

## Blending

Canvas supports additive and subtractive blending.

```cpp
canvas.add(overlay); // Add colors
canvas.subtract(overlay); // Subtract colors
```

See *Note: API Reference documentation will be available in next phase.* for complete canvas API.

## See Also

- [API: Canvas](/api/graphics/Canvas8)
- [API: CanvasExtended](/api/graphics/CanvasExtended)
- [API: CanvasGraphicsAdapter](/api/graphics/CanvasGraphicsAdapter)
