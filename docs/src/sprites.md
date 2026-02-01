---
title: Sprites
---

# Sprites

Sprites render bitmap images with transparency and blending.

## Loading Sprites

Create sprites from texture data.

```cpp
const uint8_t texture[] = { ... }; // 8-bit bitmap data
enjin2::Sprite sprite(texture, 32, 32);
```

## Rendering

Draw sprites to canvas using `draw()` method.

```cpp
enjin2::Sprite sprite(texture, 32, 32);
sprite.setPosition(10, 10);
sprite.draw(canvas);
```

## Sprite Properties

`setPosition(x, y)` - Set sprite position.

```cpp
sprite.setPosition(50, 50);
```

`setMatte(color)` - Set transparent color (pixels matching this value won't render).

```cpp
sprite.setMatte(0); // Skip black pixels
```

## Blend Modes

Sprites support three blend modes: `BlendMode::Normal`, `BlendMode::Add`, `BlendMode::Sub`.

```cpp
enjin2::Sprite sprite(texture, 32, 32, enjin2::BlendMode::Add);
```

## Sprite Sheets

Use frame-based animation with sprite sheets.

```cpp
sprite.setTexture(texture, 2); // Use frame 2
sprite.setTexture(texture_data, frame_id); // Set frame
```

## Performance Tips

Batch sprite rendering by group: sort by texture, then by blend mode.

Use matte to skip transparent pixels efficiently.

Avoid frequent `setTexture()` calls: cache texture pointers.

See [API Reference](/api) for complete sprite API.
