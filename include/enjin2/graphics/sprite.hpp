#ifndef ENJIN2_GRAPHICS_SPRITE_HPP
#define ENJIN2_GRAPHICS_SPRITE_HPP

#include "../core/types.hpp"
#include "../components/drawable.hpp"
#include "canvas.hpp"
#include <cstdint>

namespace enjin2 {

/**
 * @brief Sprite class for bitmap image rendering (matches original Enjin Sprite)
 * 
 * Handles rendering of bitmap images with frame animation support,
 * blend modes, and transparency (matte) functionality.
 */
class Sprite {
public:
    /**
     * @brief Default constructor
     */
    Sprite()
        : texture(nullptr)
        , width(0)
        , height(0)
        , matte(16)  // 16 = transparent for 4-bit, matches original
        , position(0, 0)
        , frame(0)
        , mode(BlendMode::Normal)
    {}

    /**
     * @brief Construct sprite with texture data
     * @param texture_data Pointer to texture bitmap data
     * @param w Width in pixels
     * @param h Height in pixels
     * @param blend_mode Blend mode for compositing
     */
    Sprite(const uint8_t* texture_data, uint8_t w, uint8_t h, BlendMode blend_mode = BlendMode::Normal)
        : texture(texture_data)
        , width(w)
        , height(h)
        , matte(16)  // Default transparent
        , position(0, 0)
        , frame(0)
        , mode(blend_mode)
    {}

    /**
     * @brief Draw sprite to canvas (matches original Enjin draw method)
     * @param canvas Canvas to draw to
     */
    void draw(ICanvas<uint8_t>& canvas) {
        if (!texture) return;
        
        const uint8_t* frame_texture = texture + (frame * width * height);
        
        // Draw bitmap with matte transparency (matches original drawGrayscaleBitmap)
        for (uint8_t y = 0; y < height; y++) {
            for (uint8_t x = 0; x < width; x++) {
                uint8_t pixel = frame_texture[y * width + x];
                
                // Skip transparent pixels (matte color)
                if (pixel != matte) {
                    int16_t draw_x = position.x + x;
                    int16_t draw_y = position.y + y;
                    
                    // Bounds check
                    if (draw_x >= 0 && draw_x < canvas.getWidth() && 
                        draw_y >= 0 && draw_y < canvas.getHeight()) {
                        
                        // Apply blend mode
                        switch (mode) {
                            case BlendMode::Normal:
                                canvas.setPixel(draw_x, draw_y, pixel);
                                break;
                            case BlendMode::Add: {
                                uint8_t existing = canvas.getPixel(draw_x, draw_y);
                                uint16_t result = static_cast<uint16_t>(existing) + static_cast<uint16_t>(pixel);
                                canvas.setPixel(draw_x, draw_y, result > 255 ? 255 : static_cast<uint8_t>(result));
                                break;
                            }
                            case BlendMode::Sub: {
                                uint8_t existing = canvas.getPixel(draw_x, draw_y);
                                int16_t result = static_cast<int16_t>(existing) - static_cast<int16_t>(pixel);
                                canvas.setPixel(draw_x, draw_y, result < 0 ? 0 : static_cast<uint8_t>(result));
                                break;
                            }
                            default:
                                canvas.setPixel(draw_x, draw_y, pixel);
                                break;
                        }
                    }
                }
            }
        }
    }

    /**
     * @brief Add sprite data to canvas (matches original Add method)
     * @param canvas Canvas to add to
     */
    void Add(ICanvas<uint8_t>& canvas) {
        if (!texture) return;
        
        const uint8_t* frame_texture = texture + (frame * width * height);
        
        for (uint8_t y = 0; y < height; y++) {
            for (uint8_t x = 0; x < width; x++) {
                uint8_t pixel = frame_texture[y * width + x];
                
                if (pixel != matte) {
                    int16_t draw_x = position.x + x;
                    int16_t draw_y = position.y + y;
                    
                    if (draw_x >= 0 && draw_x < canvas.getWidth() && 
                        draw_y >= 0 && draw_y < canvas.getHeight()) {
                        
                        uint8_t existing = canvas.getPixel(draw_x, draw_y);
                        uint16_t result = static_cast<uint16_t>(existing) + static_cast<uint16_t>(pixel);
                        canvas.setPixel(draw_x, draw_y, result > 255 ? 255 : static_cast<uint8_t>(result));
                    }
                }
            }
        }
    }

    /**
     * @brief Subtract sprite data from canvas (matches original Subtract method)
     * @param canvas Canvas to subtract from
     */
    void Subtract(ICanvas<uint8_t>& canvas) {
        if (!texture) return;
        
        const uint8_t* frame_texture = texture + (frame * width * height);
        
        for (uint8_t y = 0; y < height; y++) {
            for (uint8_t x = 0; x < width; x++) {
                uint8_t pixel = frame_texture[y * width + x];
                
                if (pixel != matte) {
                    int16_t draw_x = position.x + x;
                    int16_t draw_y = position.y + y;
                    
                    if (draw_x >= 0 && draw_x < canvas.getWidth() && 
                        draw_y >= 0 && draw_y < canvas.getHeight()) {
                        
                        uint8_t existing = canvas.getPixel(draw_x, draw_y);
                        int16_t result = static_cast<int16_t>(existing) - static_cast<int16_t>(pixel);
                        canvas.setPixel(draw_x, draw_y, result < 0 ? 0 : static_cast<uint8_t>(result));
                    }
                }
            }
        }
    }

    /**
     * @brief Set texture data with dimensions
     * @param texture_data Pointer to texture bitmap data
     * @param w Width in pixels
     * @param h Height in pixels
     */
    void setTexture(const uint8_t* texture_data, uint8_t w, uint8_t h) {
        texture = texture_data;
        width = w;
        height = h;
    }

    /**
     * @brief Set texture data and frame
     * @param texture_data Pointer to texture bitmap data
     * @param frame_id Frame index to display
     */
    void setTexture(const uint8_t* texture_data, uint8_t frame_id) {
        texture = texture_data;
        frame = frame_id;
    }

    /**
     * @brief Set current animation frame
     * @param frame_id Frame index to display
     */
    void setTexture(uint8_t frame_id) {
        frame = frame_id;
    }

    /**
     * @brief Set sprite position
     * @param x X coordinate
     * @param y Y coordinate
     */
    void setPosition(int16_t x, int16_t y) {
        position.x = x;
        position.y = y;
    }

    /**
     * @brief Set sprite position from point
     * @param pos New position
     */
    void setPosition(Point pos) {
        position = pos;
    }

    /**
     * @brief Set matte (transparent) color
     * @param matte_color Color value to treat as transparent
     */
    void setMatte(uint8_t matte_color) {
        matte = matte_color;
    }

    /// @brief Get pointer to current frame texture data
    /// @return Pointer to texture data, or nullptr if no texture set
    const uint8_t* GetTexture() const {
        if (!texture) return nullptr;
        return texture + (frame * width * height);
    }

    /// @brief Get sprite width
    /// @return Width in pixels
    uint8_t GetWidth() const { return width; }
    /// @brief Get sprite height
    /// @return Height in pixels
    uint8_t GetHeight() const { return height; }
    /// @brief Get current frame index
    /// @return Frame index
    uint8_t getFrame() const { return frame; }
    /// @brief Get sprite position
    /// @return Current position
    Point getPosition() const { return position; }
    /// @brief Get matte (transparent) color
    /// @return Matte color value
    uint8_t getMatte() const { return matte; }

    uint8_t _width;      ///< Legacy public width
    uint8_t _height;     ///< Legacy public height
    uint8_t _frame;      ///< Legacy public frame
    Point _position;     ///< Legacy public position
    uint8_t _matte;      ///< Legacy public matte
    BlendMode _mode;     ///< Legacy public blend mode

private:
    const uint8_t* texture;     ///< Pointer to texture data
    uint8_t width, height;      ///< Sprite dimensions
    uint8_t matte;              ///< Transparent color value
    Point position;             ///< Sprite position
    uint8_t frame;              ///< Current frame index
    BlendMode mode;             ///< Blend mode
};

} // namespace enjin2

#endif // ENJIN2_GRAPHICS_SPRITE_HPP