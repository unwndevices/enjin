---
title: Text Rendering
---

# Text Rendering

All text goes through `TextRenderer` (see the [API page](../api/graphics/TextRenderer)), in one of three
faces: monogram (the default), body and display. Its settings are sticky and live on the canvas, so each
layer keeps its own.

## From C++

```cpp
#include <enjin2/graphics/text_renderer.hpp>

enjin2::TextRenderer<enjin2::Pixel4> t(layer);
t.setFont(enjin2::faces::display);
t.setTextSize(2);
t.draw("Hello", 80, 10, enjin2::TextAlign::Center);  // y is the top of the line
```

## From Lua

```lua
gfx.setFont("body")                  -- "monogram" (default), "body", "display"
gfx.setTextWrap("ellipsis", 100)     -- "wrap" | "clip" | "ellipsis", optional box width
gfx.setTextSize(1)
gfx.text("A long line of text", 4, 4)            -- returns the lines drawn
gfx.text("Score", 156, 4, "right")
local w = gfx.getTextWidth("Score")              -- the advance
gfx.setTextOutline(true)                         -- a bg outline under the fill
```

The settings belong to the active layer (`gfx.setLayer`) and return to their defaults when the script
reloads.
