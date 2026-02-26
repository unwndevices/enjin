/**
 * @file drawable.hpp
 * @brief Base drawable component for rendering
 *
 * Provides common functionality for components that can be rendered,
 * including layer management, blending, anchoring, and visibility.
 */
#pragma once

#include "../core/component.hpp"
#include "../core/types.hpp"
#include "../graphics/canvas.hpp"
#include "position.hpp"

namespace enjin2 {

class Object; // Forward declaration

/**
 * @brief Blend mode enumeration for compositing (matches original Enjin)
 */
enum class BlendMode {
    Normal,
    Add,
    Sub,
    Difference,
    Opacity50,
    Opacity25
};

// Use Anchor enum from object.hpp

}

namespace enjin2 {

/**
 * @brief Base class for all drawable components (matches original Enjin C_Drawable)
 * 
 * Provides common functionality for components that can be rendered,
 * including layer management, blending, anchoring, and visibility.
 */
class C_Drawable : public Component {
protected:
    C_Position* position;       ///< Position component reference
    Point anchor_offset;        ///< Anchor offset
    static Point abs_center;    ///< Absolute center point (63, 63 for 128x128)
    
    uint8_t buffer_index;       ///< Layer buffer index (0 = background, N-1 = foreground)
    BlendMode blend_mode;       ///< Blend mode
    Anchor anchor;              ///< Anchor point
    bool is_visible;            ///< Visibility flag
    uint8_t width;              ///< Drawable width in pixels
    uint8_t height;             ///< Drawable height in pixels
    
public:
    /**
     * @brief Constructor (matches original Enjin)
     * @param owner Owner object
     * @param width Width of drawable area
     * @param height Height of drawable area
     */
    C_Drawable(Object* owner, uint8_t width, uint8_t height);
    
    /**
     * @brief Virtual destructor
     */
    virtual ~C_Drawable() = default;
    
    /**
     * @brief Pure virtual draw method - must be implemented by derived classes
     * @param canvas The 4-bit canvas targeting ICanvas<Pixel4>
     */
    virtual void draw(ICanvas<Pixel4>& canvas) = 0;
    
    /**
     * @brief Check if this drawable should continue to be drawn
     * @return True if should continue drawing, false otherwise
     */
    virtual bool continueToDraw() const;
    
    /// @brief Set the layer buffer index (0 = background, N-1 = foreground)
    /// @param idx Buffer index value
    void SetBufferIndex(uint8_t idx) { buffer_index = idx; }
    /// @brief Get the layer buffer index
    /// @return Current buffer index
    uint8_t GetBufferIndex() const { return buffer_index; }

    /// @brief Set the blend mode
    /// @param mode Blend mode to use
    void SetBlendMode(BlendMode mode) { blend_mode = mode; }
    /// @brief Get the blend mode
    /// @return Current blend mode
    BlendMode GetBlendMode() const { return blend_mode; }

    /// @brief Set the visibility
    /// @param visibility Visibility state
    void SetVisibility(bool visibility) { is_visible = visibility; }
    /// @brief Get the visibility
    /// @return Current visibility state
    bool GetVisibility() const { return is_visible; }
    /// @brief Check if visible
    /// @return true if visible
    bool isVisible() const { return is_visible; }

    /// @brief Set the anchor point for positioning
    /// @param anchor Anchor point
    void SetAnchorPoint(Anchor anchor);

    /// @brief Add offset to current anchor offset
    /// @param offset Offset to add
    void AddOffset(Point offset) { anchor_offset -= offset; }
    /// @brief Set the anchor offset
    /// @param offset New offset value
    void SetOffset(Point offset) { anchor_offset = offset; }

    /// @brief Get position adjusted for offset
    /// @return Offset-adjusted position
    Point GetOffsetPosition() const;

    /// @brief Set the X component of anchor offset
    /// @param x X offset value
    void SetXOffset(int16_t x) { anchor_offset.x = x; }
    /// @brief Set the Y component of anchor offset
    /// @param y Y offset value
    void SetYOffset(int16_t y) { anchor_offset.y = y; }

    /// @brief Get drawable width
    /// @return Width in pixels
    uint8_t GetWidth() const { return width; }
    /// @brief Get drawable height
    /// @return Height in pixels
    uint8_t GetHeight() const { return height; }
    
    /**
     * @brief Determine if this drawable should be drawn before another drawable
     * @param other The other drawable to compare against
     * @return True if this should be drawn before other, false otherwise
     */
    bool shouldDrawBefore(const C_Drawable& other) const {
        return buffer_index < other.buffer_index;
    }
};

} // namespace enjin2