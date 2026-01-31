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

void C_Canvas::draw(ICanvas<uint8_t>& target_canvas) {
    if (!internal_canvas) return;
    
    Point draw_pos = GetOffsetPosition();
    
    // Apply blend mode when drawing (matches original Enjin exactly)
    switch (blend_mode) {
        case BlendMode::Normal:
            // Normal blit operation - copy pixels directly (skip matte pixels)
            for (uint8_t y = 0; y < canvas_height; y++) {
                for (uint8_t x = 0; x < canvas_width; x++) {
                    int target_x = draw_pos.x + x;
                    int target_y = draw_pos.y + y;
                    
                    if (target_x >= 0 && target_y >= 0 && 
                        target_x < 128 && target_y < 128) {  // Assume max canvas size
                        uint8_t pixel = internal_canvas->getPixel(x, y);
                        if (pixel != matte_color) { // Skip matte color (transparent pixels)
                            target_canvas.setPixel(target_x, target_y, pixel);
                        }
                    }
                }
            }
            break;
            
        case BlendMode::Opacity50:
            // 50% opacity blend
            for (uint8_t y = 0; y < canvas_height; y++) {
                for (uint8_t x = 0; x < canvas_width; x++) {
                    int target_x = draw_pos.x + x;
                    int target_y = draw_pos.y + y;
                    
                    if (target_x >= 0 && target_y >= 0 && 
                        target_x < 128 && target_y < 128) {
                        uint8_t src_pixel = internal_canvas->getPixel(x, y);
                        if (src_pixel != matte_color) { // Skip matte color
                            uint8_t dst_pixel = target_canvas.getPixel(target_x, target_y);
                            uint8_t blended = (src_pixel + dst_pixel) / 2;
                            target_canvas.setPixel(target_x, target_y, blended);
                        }
                    }
                }
            }
            break;
            
        case BlendMode::Opacity25:
            // 25% opacity blend  
            for (uint8_t y = 0; y < canvas_height; y++) {
                for (uint8_t x = 0; x < canvas_width; x++) {
                    int target_x = draw_pos.x + x;
                    int target_y = draw_pos.y + y;
                    
                    if (target_x >= 0 && target_y >= 0 && 
                        target_x < 128 && target_y < 128) {
                        uint8_t src_pixel = internal_canvas->getPixel(x, y);
                        if (src_pixel != matte_color) { // Skip matte color
                            uint8_t dst_pixel = target_canvas.getPixel(target_x, target_y);
                            uint8_t blended = (src_pixel + dst_pixel * 3) / 4;
                            target_canvas.setPixel(target_x, target_y, blended);
                        }
                    }
                }
            }
            break;
            
        case BlendMode::Add:
            // Additive blending
            for (uint8_t y = 0; y < canvas_height; y++) {
                for (uint8_t x = 0; x < canvas_width; x++) {
                    int target_x = draw_pos.x + x;
                    int target_y = draw_pos.y + y;
                    
                    if (target_x >= 0 && target_y >= 0 && 
                        target_x < 128 && target_y < 128) {
                        uint8_t src_pixel = internal_canvas->getPixel(x, y);
                        if (src_pixel != matte_color) { // Skip matte color
                            uint8_t dst_pixel = target_canvas.getPixel(target_x, target_y);
                            uint8_t blended = std::min(15, static_cast<int>(src_pixel + dst_pixel));
                            target_canvas.setPixel(target_x, target_y, blended);
                        }
                    }
                }
            }
            break;
            
        case BlendMode::Sub:
            // Subtractive blending
            for (uint8_t y = 0; y < canvas_height; y++) {
                for (uint8_t x = 0; x < canvas_width; x++) {
                    int target_x = draw_pos.x + x;
                    int target_y = draw_pos.y + y;
                    
                    if (target_x >= 0 && target_y >= 0 && 
                        target_x < 128 && target_y < 128) {
                        uint8_t src_pixel = internal_canvas->getPixel(x, y);
                        if (src_pixel != matte_color) { // Skip matte color
                            uint8_t dst_pixel = target_canvas.getPixel(target_x, target_y);
                            uint8_t blended = std::max(0, static_cast<int>(dst_pixel - src_pixel));
                            target_canvas.setPixel(target_x, target_y, blended);
                        }
                    }
                }
            }
            break;
            
        default:
            // Fallback to normal mode
            break;
    }
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