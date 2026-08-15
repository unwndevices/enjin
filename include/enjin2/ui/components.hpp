#pragma once

#include "component.hpp"
#include "reflect.hpp"
#include "../core/types.hpp"
#include "../graphics/canvas.hpp"
#include <cstdint>
#include <string>

namespace enjin2 {

/**
 * @brief Position component for 2D spatial positioning
 */
struct PositionComponent : public Component<PositionComponent> {
    Point position;   ///< Current position (the point the anchor is pinned to)
    Point lastPos;    ///< Previous position (for interpolation/delta calculations)
    Anchor anchor = Anchor::TOP_LEFT; ///< Which box point sits on @ref position
    Point anchorOffset; ///< Manual nudge from the anchored origin (+x right, +y down)

    /**
     * @brief Constructor with initial position
     * @param pos Initial position
     */
    PositionComponent(Point pos = Point()) : position(pos), lastPos(pos) {}

    /**
     * @brief Top-left draw origin for a widget of extent @p box (pure)
     * @param box The widget's bounding-box size
     * @return Where the widget's top-left corner lands on the canvas
     *
     * Combines the anchor (which box point pins to @ref position) with the
     * manual @ref anchorOffset nudge: `position - anchorPoint(anchor, box) +
     * anchorOffset`. With the default @ref Anchor::TOP_LEFT and a zero offset
     * this is just @ref position, so every existing widget keeps its top-left
     * placement until a scene opts into an anchor. Positive offsets move the
     * widget right/down, matching Eisei's `C_Drawable::AddOffset`.
     */
    Point renderOrigin(Size box) const {
        const Point ap = anchorPoint(anchor, box);
        return Point(static_cast<int16_t>(position.x - ap.x + anchorOffset.x),
                     static_cast<int16_t>(position.y - ap.y + anchorOffset.y));
    }
    
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

    /// @brief Current position (reflection getter).
    Point getPosition() const { return position; }

    /**
     * @brief Teleport to @p pos, collapsing the movement delta
     *
     * Unlike @ref moveTo this also sets @ref lastPos, restoring the
     * constructed-at state — the scene loader lands entities with this.
     */
    void place(Point pos) {
        position = pos;
        lastPos = pos;
    }
};

/// @brief Serializable properties of @ref PositionComponent (see reflect.hpp).
/// `lastPos` is runtime delta-tracking state and stays transient; loading
/// places the entity, it does not replay its movement.
#define ENJIN2_POSITION_COMPONENT_FIELDS(FIELD, PROP) \
    PROP(position, Point, getPosition, place)         \
    FIELD(anchor)                                     \
    FIELD(anchorOffset)

ENJIN2_REFLECT_COMPONENT(PositionComponent, 1, "position", ENJIN2_POSITION_COMPONENT_FIELDS)

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

/// @brief Serializable properties of @ref SizeComponent (see reflect.hpp).
#define ENJIN2_SIZE_COMPONENT_FIELDS(FIELD, PROP) \
    FIELD(size)                                   \
    FIELD(minSize)                                \
    FIELD(maxSize)

ENJIN2_REFLECT_COMPONENT(SizeComponent, 2, "size", ENJIN2_SIZE_COMPONENT_FIELDS)

/**
 * @brief Stable string identity for scene-file entities (unwn #183, M2)
 *
 * Behavior data (bindings, event→action tables, animation tracks) addresses
 * entities by this id — `presetList.selectedIndex` — never by the runtime
 * Entity handle, which is allocator-order and must not leak into files. An
 * entity without an IdComponent is anonymous: serializable, but unreachable
 * from behavior.
 */
struct IdComponent : public Component<IdComponent> {
    std::string id; ///< Scene-unique name, as written in the file

    IdComponent(std::string id_ = {}) : id(std::move(id_)) {}
};

/// @brief Serializable properties of @ref IdComponent (see reflect.hpp).
#define ENJIN2_ID_COMPONENT_FIELDS(FIELD, PROP) \
    FIELD(id)

ENJIN2_REFLECT_COMPONENT(IdComponent, 10, "id", ENJIN2_ID_COMPONENT_FIELDS)

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

// The former immediate-mode ShapeComponent (rectangle/circle/triangle/line
// primitives coupled to RenderComponent + RenderSystem) was retired when its
// role was promoted to the reflected `shape` widget — a placeable, serializable
// Position+Size primitive in the ui-ECS widget pack (unwn #206, see
// widgets/shape.hpp). RenderSystem now draws each RenderComponent as a filled
// rectangle over its Size box.

/**
 * @brief Optional per-entity draw order for the scene render pass (unwn #243)
 *
 * Authorable cross-widget layering (ADR-0014): the scene player renders one
 * z-sorted pass across every widget system instead of a hardcoded system
 * sequence, so an entity's stacking is its own `z`, not its widget kind. The
 * component is *optional* — an entity without it defaults to its widget
 * system's legacy priority (overlay 800 < shape 850 < label/icon/sprite/list
 * 900 < gauge 950 < bar 955 < popup 1000), so every pre-#243 scene renders
 * byte-identically. Attaching a `z` overrides that default, letting (for
 * example) an `overlay` sit mid-stack and dim only the content drawn below it.
 *
 * Additive and reflected, so it serializes through the same generic path as
 * every other component and forces no scene-format version bump.
 */
struct ZComponent : public Component<ZComponent> {
    int16_t z = 0; ///< Draw order; higher draws later (on top)

    /// @brief Construct with an initial draw order.
    ZComponent(int16_t z_ = 0) : z(z_) {}
};

/// @brief Serializable properties of @ref ZComponent (see reflect.hpp).
#define ENJIN2_Z_COMPONENT_FIELDS(FIELD, PROP) \
    FIELD(z)

ENJIN2_REFLECT_COMPONENT(ZComponent, 16, "z", ENJIN2_Z_COMPONENT_FIELDS)

} // namespace enjin2
