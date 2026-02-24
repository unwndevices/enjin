#include "../../include/enjin2/components/canvas.hpp"
#include "../../include/enjin2/core/object.hpp"
#include <iostream>

namespace enjin2 {

C_Canvas::C_Canvas(Object* owner, uint8_t width, uint8_t height) 
    : C_Drawable(owner, width, height)
    , canvas_width(width)
    , canvas_height(height)
    , position(nullptr) {
    
    createCanvas(width, height);
}

void C_Canvas::createCanvas(uint8_t width, uint8_t height) {
    // Create canvas based on common sizes to avoid template explosion
    if (width <= 32 && height <= 32) {
        internal_canvas = std::unique_ptr<ICanvas<uint8_t>>(new Canvas8<32, 32>());
    } else if (width <= 64 && height <= 32) {
        internal_canvas = std::unique_ptr<ICanvas<uint8_t>>(new Canvas8<64, 32>());
    } else if (width <= 64 && height <= 64) {
        internal_canvas = std::unique_ptr<ICanvas<uint8_t>>(new Canvas8<64, 64>());
    } else if (width <= 128 && height <= 64) {
        internal_canvas = std::unique_ptr<ICanvas<uint8_t>>(new Canvas8<128, 64>());
    } else {
        internal_canvas = std::unique_ptr<ICanvas<uint8_t>>(new Canvas8<128, 128>());
    }
    
    // Clear canvas to black
    clear(0);
}

void C_Canvas::awake() {
    // Get position component from owner (matches original Enjin pattern)
    position = owner->getComponent<C_Position>();
    if (!position) {
        std::cerr << "[C_Canvas] Warning: C_Canvas requires C_Position component.\n";
    }
}

void C_Canvas::lateUpdate(uint16_t deltaTime) {
    // No late update operations needed
}

void C_Canvas::draw(ICanvas<Pixel4>& /*target_canvas*/) {
    // C_Canvas renders to its own internal Canvas8 buffer.
    // Compositing Canvas8 -> ICanvas<Pixel4> is deferred to ENG-01 (v2).
    // This override satisfies the C_Drawable pure virtual contract.
}

void C_Canvas::applyBlendMode(ICanvas<Pixel4>& /*target_canvas*/) {
    // Deferred — see draw() stub comment above
}

bool C_Canvas::continueToDraw() const {
    // Check if object is not queued for removal (matches original Enjin)
    return !owner->isQueuedForRemoval();
}

void C_Canvas::clear(uint8_t color) {
    if (internal_canvas) {
        internal_canvas->clear(color);
    }
}

} // namespace enjin2