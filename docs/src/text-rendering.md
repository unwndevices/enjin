---
title: Text Rendering
---

# Text Rendering

Text renderer draws text to canvas with font support.

## Rendering Text

Create `TextRenderer` and draw text at position.

```cpp
enjin2::TextRenderer<uint8_t> text;
text.setCursor(10, 10);
text.drawString(canvas, 10, 10, "Hello World");
```

## Text Properties

`setTextColor(color)` - Set text color (transparent background).

```cpp
text.setTextColor(15); // White text
```

`setTextColor(color, bg)` - Set text color with background.

```cpp
text.setTextColor(15, 0); // White on black
```

`setTextSize(size)` - Set text scaling (1 = normal, 2 = double).

```cpp
text.setTextSize(2); // Double size
```

`setTextSize(sx, sy)` - Set separate X/Y scaling.

```cpp
text.setTextSize(2, 1); // Double width, normal height
```

`setCursor(x, y)` - Set text position.

```cpp
text.setCursor(20, 30);
```

## Fonts

Default 5x7 font is built-in.

Use custom fonts with GFX font format.

```cpp
extern const GFXfont customFont;
text.setFont(&customFont);
```

`getTextWidth(str)` - Measure text width.

```cpp
uint16_t width = text.getTextWidth("Hello");
```

`getTextBounds(str, x, y, x1, y1, w, h)` - Get bounding rectangle.

```cpp
int16_t x1, y1;
uint16_t w, h;
text.getTextBounds("Hello", 10, 10, &x1, &y1, &w, &h);
```

## Positioning

Text position is cursor-based: set cursor once, draw multiple strings.

```cpp
text.setCursor(10, 10);
text.writeString(canvas, "Line 1\n"); // Cursor advances
text.writeString(canvas, "Line 2\n");
```

## Performance

Cache frequently used strings in `const char*` arrays.

Use monospaced fonts for predictable layout.

Avoid frequent `setFont()` and `setTextSize()` calls.

See [API Reference](/api) for complete text API.
