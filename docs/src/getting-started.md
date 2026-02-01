---
title: Getting Started
sidebar_label: Getting Started
---

# Getting Started

Get enjin2 up and running in three simple steps.

## 1. Clone Repository

```bash
git clone https://github.com/unwndevices/enjin.git
cd enjin
```

## 2. Configure Build

```bash
mkdir build && cd build
cmake ..
```

## 3. Build

```bash
cmake --build .
```

## Quick Example

```cpp
#include <enjin2.hpp>

using namespace enjin2;

int main() {
    // Create canvas and draw a rectangle
    Canvas8_128x64 canvas;
    canvas.fillRect(10, 10, 108, 44, 15);

    return 0;
}
```

## Next Steps

[Components](./components.md) - Learn the component system.

[Canvas](./canvas.md) - Graphics operations guide.

[Scene Management](./scene-management.md) - Scene system overview.

[API Reference](/api) - Complete API documentation.
