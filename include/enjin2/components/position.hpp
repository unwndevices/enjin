#pragma once

#include "../core/component.hpp"
#include "../core/types.hpp"
#include "../core/object.hpp"
#include <cmath>

namespace enjin2 {

/**
 * @brief Position component for object positioning
 * 
 * Manages the position and anchor point of an object in 2D space.
 * This is a fundamental component used by most drawable objects.
 */
class C_Position : public Component {
private:
    Point position;         ///< Object position in pixels
    Anchor anchor;          ///< Anchor point for positioning
    Point anchorOffset;     ///< Additional offset from anchor
    
public:
    /**
     * @brief Constructor with default position
     * @param owner Owner object
     */
    explicit C_Position(Object* owner) 
        : Component(owner), position(0, 0), anchor(Anchor::TOP_LEFT), anchorOffset(0, 0) {}
    
    /**
     * @brief Constructor with initial position
     * @param owner Owner object
     * @param x Initial X position
     * @param y Initial Y position
     */
    C_Position(Object* owner, int16_t x, int16_t y) 
        : Component(owner), position(x, y), anchor(Anchor::TOP_LEFT), anchorOffset(0, 0) {}
    
    /**
     * @brief Constructor with Point
     * @param owner Owner object
     * @param pos Initial position
     */
    C_Position(Object* owner, const Point& pos) 
        : Component(owner), position(pos), anchor(Anchor::TOP_LEFT), anchorOffset(0, 0) {}
    
    /**
     * @brief Set position
     * @param x X coordinate
     * @param y Y coordinate
     */
    void setPosition(int16_t x, int16_t y) {
        position.x = x;
        position.y = y;
    }
    
    /**
     * @brief Set position
     * @param pos New position
     */
    void setPosition(const Point& pos) {
        position = pos;
    }
    
    /**
     * @brief Get position
     * @return Current position
     */
    const Point& getPosition() const { return position; }
    
    /**
     * @brief Move position by offset
     * @param dx X offset
     * @param dy Y offset
     */
    void move(int16_t dx, int16_t dy) {
        position.x += dx;
        position.y += dy;
    }
    
    /**
     * @brief Move position by offset
     * @param offset Offset vector
     */
    void move(const Point& offset) {
        position.x += offset.x;
        position.y += offset.y;
    }
    
    /**
     * @brief Set anchor point
     * @param anchor New anchor point
     */
    void setAnchor(Anchor anchor) {
        this->anchor = anchor;
    }
    
    /**
     * @brief Get anchor point
     * @return Current anchor point
     */
    Anchor getAnchor() const { return anchor; }
    
    /**
     * @brief Set anchor offset
     * @param offset Offset from anchor point
     */
    void setAnchorOffset(const Point& offset) {
        anchorOffset = offset;
    }
    
    /**
     * @brief Get anchor offset
     * @return Current anchor offset
     */
    const Point& getAnchorOffset() const { return anchorOffset; }
    
    /**
     * @brief Calculate final rendering position based on anchor and size
     * @param size Size of the object for anchor calculation
     * @return Final rendering position
     */
    Point calculateRenderPosition(const Size& size) const {
        Point renderPos = position + anchorOffset;
        
        // Apply anchor offset based on object size
        switch (anchor) {
            case Anchor::TOP_LEFT:
                // No adjustment needed
                break;
            case Anchor::TOP_CENTER:
                renderPos.x -= size.width / 2;
                break;
            case Anchor::TOP_RIGHT:
                renderPos.x -= size.width;
                break;
            case Anchor::CENTER_LEFT:
                renderPos.y -= size.height / 2;
                break;
            case Anchor::CENTER:
                renderPos.x -= size.width / 2;
                renderPos.y -= size.height / 2;
                break;
            case Anchor::CENTER_RIGHT:
                renderPos.x -= size.width;
                renderPos.y -= size.height / 2;
                break;
            case Anchor::BOTTOM_LEFT:
                renderPos.y -= size.height;
                break;
            case Anchor::BOTTOM_CENTER:
                renderPos.x -= size.width / 2;
                renderPos.y -= size.height;
                break;
            case Anchor::BOTTOM_RIGHT:
                renderPos.x -= size.width;
                renderPos.y -= size.height;
                break;
        }
        
        return renderPos;
    }
    
    /**
     * @brief Linear interpolation to target position
     * @param target Target position
     * @param t Interpolation factor (0.0 to 1.0)
     */
    void lerp(const Point& target, float t) {
        if (t <= 0.0f) return;
        if (t >= 1.0f) {
            position = target;
            return;
        }
        
        position.x = static_cast<int16_t>(position.x + (target.x - position.x) * t);
        position.y = static_cast<int16_t>(position.y + (target.y - position.y) * t);
    }
    
    /**
     * @brief Calculate distance to another position
     * @param other Other position
     * @return Distance in pixels
     */
    float distanceTo(const Point& other) const {
        int16_t dx = other.x - position.x;
        int16_t dy = other.y - position.y;
        return std::sqrt(static_cast<float>(dx * dx + dy * dy));
    }
    
    /**
     * @brief Calculate squared distance to another position (faster than distanceTo)
     * @param other Other position
     * @return Squared distance
     */
    int32_t distanceSquaredTo(const Point& other) const {
        int16_t dx = other.x - position.x;
        int16_t dy = other.y - position.y;
        return static_cast<int32_t>(dx * dx + dy * dy);
    }
};

} // namespace enjin2