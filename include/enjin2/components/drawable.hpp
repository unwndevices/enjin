#pragma once

#include "../core/component.hpp"
#include "../core/types.hpp"
#include "../graphics/canvas.hpp"
#include "position.hpp"

namespace enjin2 {

class Object; // Forward declaration

/**
 * @brief Draw layer enumeration for rendering order (matches original Enjin)
 */
enum class DrawLayer {
    Default,
    Background,
    Entities,
    Foreground,
    Overlay,
    UI
};

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
    
    int sort_order;             ///< Sort order for drawing
    DrawLayer layer;            ///< Draw layer
    BlendMode blend_mode;       ///< Blend mode
    Anchor anchor;              ///< Anchor point
    bool is_visible;            ///< Visibility flag
    uint8_t width, height;      ///< Dimensions
    
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
     * @param canvas The 8-bit canvas to draw to (matches original Enjin GFXcanvas8)
     */
    virtual void draw(ICanvas<uint8_t>& canvas) = 0;
    
    /**
     * @brief Check if this drawable should continue to be drawn
     * @return True if should continue drawing, false otherwise
     */
    virtual bool continueToDraw() const;
    
    // Method names match original Enjin exactly
    void SetSortOrder(int order) { sort_order = order; }
    int GetSortOrder() const { return sort_order; }
    
    void SetBlendMode(BlendMode mode) { blend_mode = mode; }
    BlendMode GetBlendMode() const { return blend_mode; }
    
    void SetDrawLayer(DrawLayer drawLayer) { layer = drawLayer; }
    DrawLayer GetDrawLayer() const { return layer; }
    
    void SetVisibility(bool visibility) { is_visible = visibility; }
    bool GetVisibility() const { return is_visible; }
    bool isVisible() const { return is_visible; }
    
    void SetAnchorPoint(Anchor anchor);
    
    void AddOffset(Point offset) { anchor_offset -= offset; }
    void SetOffset(Point offset) { anchor_offset = offset; }
    
    Point GetOffsetPosition() const;
    
    void SetXOffset(int16_t x) { anchor_offset.x = x; }
    void SetYOffset(int16_t y) { anchor_offset.y = y; }
    
    uint8_t GetWidth() const { return width; }
    uint8_t GetHeight() const { return height; }
    
    /**
     * @brief Determine if this drawable should be drawn before another drawable
     * @param other The other drawable to compare against
     * @return True if this should be drawn before other, false otherwise
     */
    bool shouldDrawBefore(const C_Drawable& other) const {
        // First sort by layer
        if (layer != other.layer) {
            return static_cast<int>(layer) < static_cast<int>(other.layer);
        }
        
        // Then by sort order within the same layer
        return sort_order < other.sort_order;
    }
};

} // namespace enjin2