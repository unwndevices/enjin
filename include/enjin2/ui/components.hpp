#pragma once

#include "component.hpp"
#include "../core/types.hpp"
#include "../graphics/canvas.hpp"
#include <cstdint>

namespace enjin2 {

/**
 * @brief Position component for 2D spatial positioning
 */
struct PositionComponent : public Component<PositionComponent> {
    Point position;   ///< Current position
    Point lastPos;    ///< Previous position (for interpolation/delta calculations)
    
    /**
     * @brief Constructor with initial position
     * @param pos Initial position
     */
    PositionComponent(Point pos = Point()) : position(pos), lastPos(pos) {}
    
    /**
     * @brief Move to new position, storing old position
     * @param newPos New position
     */
    void moveTo(Point newPos) {
        lastPos = position;
        position = newPos;
    }
    
    /**
     * @brief Translate by offset
     * @param offset Offset to add to current position
     */
    void translate(Point offset) {
        lastPos = position;
        position.x += offset.x;
        position.y += offset.y;
    }
};

/**
 * @brief Size component for 2D spatial dimensions
 */
struct SizeComponent : public Component<SizeComponent> {
    Size size;        ///< Current dimensions
    Size minSize;     ///< Minimum allowed size
    Size maxSize;     ///< Maximum allowed size
    
    /**
     * @brief Constructor with size constraints
     * @param sz Initial size
     * @param minSz Minimum size (default: 1x1)
     * @param maxSz Maximum size (default: unlimited)
     */
    SizeComponent(Size sz = Size(), Size minSz = Size(1, 1), Size maxSz = Size(65535, 65535))
        : size(sz), minSize(minSz), maxSize(maxSz) {}
    
    /**
     * @brief Resize with constraint checking
     * @param newSize Requested new size
     */
    void resize(Size newSize) {
        size.width = std::max(minSize.width, std::min(maxSize.width, newSize.width));
        size.height = std::max(minSize.height, std::min(maxSize.height, newSize.height));
    }
    
    /**
     * @brief Get area in pixels
     * @return Total area
     */
    uint32_t getArea() const {
        return static_cast<uint32_t>(size.width) * size.height;
    }
};

/**
 * @brief Visual rendering component for drawable entities
 */
struct RenderComponent : public Component<RenderComponent> {
    Pixel4 color;           ///< Primary color
    Pixel4 backgroundColor; ///< Background color
    uint8_t opacity;        ///< Opacity (0-255)
    bool visible;           ///< Visibility flag
    int16_t zOrder;         ///< Z-order for layering (higher = front)
    
    /**
     * @brief Constructor with rendering parameters
     * @param col Primary color
     * @param bgCol Background color
     * @param op Opacity level
     * @param vis Initial visibility
     * @param z Z-order
     */
    RenderComponent(Pixel4 col = Colors::WHITE, Pixel4 bgCol = Colors::BLACK,
                   uint8_t op = 255, bool vis = true, int16_t z = 0)
        : color(col), backgroundColor(bgCol), opacity(op), visible(vis), zOrder(z) {}
    
    /**
     * @brief Set color with opacity
     * @param newColor New color
     * @param alpha Opacity (0-255)
     */
    void setColor(Pixel4 newColor, uint8_t alpha = 255) {
        color = newColor;
        opacity = alpha;
    }
    
    /**
     * @brief Check if component should be rendered
     * @return true if visible and opacity > 0
     */
    bool shouldRender() const {
        return visible && opacity > 0;
    }
};

/**
 * @brief Animation component for time-based property changes
 */
struct AnimationComponent : public Component<AnimationComponent> {
    float duration;      ///< Total animation duration in seconds
    float currentTime;   ///< Current animation time
    bool playing;        ///< Animation playback state
    bool looping;        ///< Whether animation should loop
    bool pingPong;       ///< Whether animation should ping-pong
    float speed;         ///< Animation speed multiplier
    
    /**
     * @brief Constructor with animation parameters
     * @param dur Duration in seconds
     * @param loop Whether to loop
     * @param pp Whether to ping-pong
     * @param spd Speed multiplier
     */
    AnimationComponent(float dur = 1.0f, bool loop = false, bool pp = false, float spd = 1.0f)
        : duration(dur), currentTime(0.0f), playing(false), looping(loop), 
          pingPong(pp), speed(spd) {}
    
    /**
     * @brief Start animation playback
     */
    void play() {
        playing = true;
    }
    
    /**
     * @brief Pause animation playback
     */
    void pause() {
        playing = false;
    }
    
    /**
     * @brief Stop and reset animation
     */
    void stop() {
        playing = false;
        currentTime = 0.0f;
    }
    
    /**
     * @brief Get normalized animation progress (0-1)
     * @return Animation progress
     */
    float getProgress() const {
        if (duration <= 0.0f) return 1.0f;
        return std::min(1.0f, currentTime / duration);
    }
    
    /**
     * @brief Check if animation is complete
     * @return true if animation finished
     */
    bool isComplete() const {
        return currentTime >= duration;
    }
};

/**
 * @brief Input state component for interactive entities
 */
struct InputComponent : public Component<InputComponent> {
    bool hovered;        ///< Mouse/touch hover state
    bool pressed;        ///< Currently being pressed
    bool clicked;        ///< Was clicked this frame
    bool focused;        ///< Has input focus
    bool enabled;        ///< Accepts input events
    Point lastHitPos;    ///< Last interaction position
    
    /**
     * @brief Constructor with initial input state
     * @param en Initial enabled state
     */
    InputComponent(bool en = true)
        : hovered(false), pressed(false), clicked(false), 
          focused(false), enabled(en), lastHitPos() {}
    
    /**
     * @brief Reset transient input state (called each frame)
     */
    void resetTransientState() {
        clicked = false;
    }
    
    /**
     * @brief Handle press event
     * @param pos Position of press
     */
    void onPress(Point pos) {
        if (!enabled) return;
        pressed = true;
        lastHitPos = pos;
    }
    
    /**
     * @brief Handle release event
     * @param pos Position of release
     */
    void onRelease(Point pos) {
        if (!enabled) return;
        if (pressed) {
            clicked = true;
        }
        pressed = false;
        lastHitPos = pos;
    }
    
    /**
     * @brief Handle hover enter event
     */
    void onHoverEnter() {
        if (enabled) hovered = true;
    }
    
    /**
     * @brief Handle hover exit event
     */
    void onHoverExit() {
        hovered = false;
    }
};

/**
 * @brief Shape rendering component for geometric primitives
 */
struct ShapeComponent : public Component<ShapeComponent> {
    /// @brief Shape type enumeration
    enum ShapeType {
        RECTANGLE,   ///< Rectangle shape
        CIRCLE,      ///< Circle shape
        TRIANGLE,    ///< Triangle shape
        LINE         ///< Line shape
    } type;          ///< Current shape type

    bool filled;         ///< Whether shape is filled or outline
    uint8_t thickness;   ///< Line thickness for outlines

    uint16_t radius;     ///< Circle radius
    Point p1;            ///< First vertex (triangles)
    Point p2;            ///< Second vertex (triangles)
    Point p3;            ///< Third vertex (triangles)
    Point start;         ///< Start point (lines)
    Point end;           ///< End point (lines)
    
    /**
     * @brief Create rectangle shape
     * @param fill Whether to fill the rectangle
     * @param thick Line thickness
     * @return Configured ShapeComponent
     */
    static ShapeComponent rectangle(bool fill = true, uint8_t thick = 1) {
        ShapeComponent comp;
        comp.type = RECTANGLE;
        comp.filled = fill;
        comp.thickness = thick;
        return comp;
    }
    
    /**
     * @brief Create circle shape
     * @param radius Circle radius
     * @param fill Whether to fill the circle
     * @param thick Line thickness
     * @return Configured ShapeComponent
     */
    static ShapeComponent circle(uint16_t radius, bool fill = true, uint8_t thick = 1) {
        ShapeComponent comp;
        comp.type = CIRCLE;
        comp.filled = fill;
        comp.thickness = thick;
        comp.radius = radius;
        return comp;
    }
    
    /**
     * @brief Create triangle shape
     * @param pt1 First vertex
     * @param pt2 Second vertex
     * @param pt3 Third vertex
     * @param fill Whether to fill the triangle
     * @param thick Line thickness
     * @return Configured ShapeComponent
     */
    static ShapeComponent triangle(Point pt1, Point pt2, Point pt3, bool fill = true, uint8_t thick = 1) {
        ShapeComponent comp;
        comp.type = TRIANGLE;
        comp.filled = fill;
        comp.thickness = thick;
        comp.p1 = pt1;
        comp.p2 = pt2;
        comp.p3 = pt3;
        return comp;
    }
    
    /**
     * @brief Create line shape
     * @param startPt Start point
     * @param endPt End point
     * @param thick Line thickness
     * @return Configured ShapeComponent
     */
    static ShapeComponent line(Point startPt, Point endPt, uint8_t thick = 1) {
        ShapeComponent comp;
        comp.type = LINE;
        comp.filled = false;
        comp.thickness = thick;
        comp.start = startPt;
        comp.end = endPt;
        return comp;
    }

public:
    ShapeComponent() : type(RECTANGLE), filled(true), thickness(1), radius(0) {}
};

} // namespace enjin2