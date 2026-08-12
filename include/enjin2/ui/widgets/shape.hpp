#pragma once

#include "../component.hpp"
#include "../reflect.hpp"
#include "../components.hpp"
#include "../system.hpp"
#include "../../core/types.hpp"
#include "../../graphics/canvas.hpp"
#include "../../graphics/primitives.hpp"
#include <algorithm>
#include <cstdint>

/**
 * @file shape.hpp
 * @brief Geometric-primitive widget on the ui ECS (unwn #206)
 *
 * Promotes the engine-internal shape primitives to one reflected @ref
 * ShapeComponent with a @ref ShapeType selector, so a designer can place a
 * rect, a circle or a line from the same palette entry as every other widget.
 * Geometry comes entirely from the shared Position+Size box (spec §Widget set):
 *
 *  - **Rect** fills / frames the box exactly.
 *  - **Circle** is inscribed in the box (centered, diameter = min(w, h)).
 *  - **Line** runs the box corner to corner, top-left to bottom-right.
 *
 * `triangle` is deliberately dropped: it has no natural mapping onto an
 * axis-aligned Position+Size box (unwn #206). A "frame" is authored as
 * `type: rect, filled: false`.
 *
 * @ref filled selects fill vs outline; @ref thickness widens a rect/circle
 * outline into concentric rings (it does not apply to a filled shape, nor to a
 * line, which is always a single Bresenham stroke); @ref color travels on the
 * widget itself rather than the theme, since a decorative primitive is not
 * chrome.
 */

namespace enjin2 {

/**
 * @brief Primitive selector for a @ref ShapeComponent
 *
 * Serializes as its integer value (the inspector edits it as an enum field).
 * The order is the permanent wire encoding — append only, never reorder.
 */
enum class ShapeType : uint8_t {
    Rect = 0,   ///< Axis-aligned rectangle filling the Position+Size box
    Circle = 1, ///< Circle inscribed in the box (diameter = min(w, h))
    Line = 2    ///< Line from the box's top-left to its bottom-right corner
};

/**
 * @brief Data-only state for a geometric-primitive widget
 *
 * Carries only the primitive's appearance — @ref type, @ref filled, @ref
 * thickness and @ref color. Its geometry is the entity's shared Position+Size
 * box, so the component pairs with a PositionComponent and a SizeComponent; the
 * @ref ShapeSystem reads both to place the primitive.
 */
struct ShapeComponent : public Component<ShapeComponent> {
    ShapeType type = ShapeType::Rect; ///< Which primitive to draw
    bool filled = false;              ///< Fill vs outline (a "frame" is rect + !filled)
    uint8_t thickness = 1;            ///< Outline ring count (rect/circle only; ignored when filled)
    Pixel4 color = Pixel4(15);        ///< Draw color, carried on the widget

    /// @brief Construct with an appearance (geometry rides Position+Size).
    ShapeComponent(ShapeType t = ShapeType::Rect, bool fill = false,
                   uint8_t thick = 1, Pixel4 c = Pixel4(15))
        : type(t), filled(fill), thickness(thick), color(c) {}
};

/// @brief Serializable properties of @ref ShapeComponent (see reflect.hpp).
#define ENJIN2_SHAPE_COMPONENT_FIELDS(FIELD, PROP) \
    FIELD(type)                                    \
    FIELD(filled)                                  \
    FIELD(thickness)                               \
    FIELD(color)

ENJIN2_REFLECT_COMPONENT(ShapeComponent, 14, "shape", ENJIN2_SHAPE_COMPONENT_FIELDS)

/**
 * @brief Draws every ShapeComponent entity to a Pixel4 canvas
 * @tparam TWorld World composing ShapeComponent, PositionComponent and SizeComponent
 * @tparam TCanvas Pixel4 canvas type (e.g. Canvas4<W,H>)
 *
 * Each shape is placed by its entity's Position (top-left) and Size (extent):
 * a rect fills / frames the box, a circle is inscribed in it, a line runs its
 * top-left→bottom-right diagonal. An entity without a SizeComponent has no box
 * and is skipped by the query.
 */
template<typename TWorld, typename TCanvas>
class ShapeSystem : public System<ShapeSystem<TWorld, TCanvas>> {
public:
    /**
     * @brief Construct against the world it draws and the canvas it draws to
     * @param world World holding the shape entities (borrowed, not owned)
     * @param canvas Target Pixel4 canvas (borrowed, not owned)
     */
    ShapeSystem(TWorld* world, TCanvas* canvas) : world_(world), canvas_(canvas) {}

    /**
     * @brief Draw every shape entity
     * @param dt Time since last update in seconds (unused; shapes are static)
     */
    void update(float dt) override {
        (void)dt;
        if (!world_ || !canvas_) return;
        for (Entity e : world_->template query<ShapeComponent, PositionComponent, SizeComponent>()) {
            auto* shape = world_->template get<ShapeComponent>(e);
            auto* pos = world_->template get<PositionComponent>(e);
            auto* size = world_->template get<SizeComponent>(e);
            if (!shape || !pos || !size) continue;
            draw(*shape, pos->position, size->size);
        }
    }

    /// @brief Shapes are decorative backdrop, drawn beneath labels/icons.
    int getPriority() const override { return 850; }

private:
    void draw(const ShapeComponent& s, const Point& pos, const Size& box) {
        if (box.width == 0 || box.height == 0) return;
        switch (s.type) {
        case ShapeType::Rect:
            drawRectShape(s, pos, box);
            break;
        case ShapeType::Circle:
            drawCircleShape(s, pos, box);
            break;
        case ShapeType::Line:
            Primitives<Pixel4>::drawLine(
                *canvas_, pos.x, pos.y,
                static_cast<int16_t>(pos.x + box.width - 1),
                static_cast<int16_t>(pos.y + box.height - 1), s.color);
            break;
        }
    }

    void drawRectShape(const ShapeComponent& s, const Point& pos, const Size& box) {
        if (s.filled) {
            Primitives<Pixel4>::fillRect(
                *canvas_, Rect(pos.x, pos.y, box.width, box.height), s.color);
            return;
        }
        // Concentric outline rings, one per unit of thickness, inset from the box
        // edge until the ring would collapse.
        for (int t = 0; t < s.thickness; ++t) {
            const int w = static_cast<int>(box.width) - 2 * t;
            const int h = static_cast<int>(box.height) - 2 * t;
            if (w <= 0 || h <= 0) break;
            Primitives<Pixel4>::drawRect(
                *canvas_,
                Rect(static_cast<int16_t>(pos.x + t), static_cast<int16_t>(pos.y + t),
                     static_cast<uint16_t>(w), static_cast<uint16_t>(h)),
                s.color);
        }
    }

    void drawCircleShape(const ShapeComponent& s, const Point& pos, const Size& box) {
        const int diameter = std::min<int>(box.width, box.height);
        const int r = diameter / 2;
        if (r <= 0) return;
        // Center within the box; the inscribed circle uses the smaller extent.
        const int16_t cx = static_cast<int16_t>(pos.x + box.width / 2);
        const int16_t cy = static_cast<int16_t>(pos.y + box.height / 2);
        if (s.filled) {
            Primitives<Pixel4>::fillCircle(*canvas_, cx, cy, static_cast<int16_t>(r), s.color);
            return;
        }
        for (int t = 0; t < s.thickness; ++t) {
            const int rr = r - t;
            if (rr < 0) break;
            Primitives<Pixel4>::drawCircle(*canvas_, cx, cy, static_cast<int16_t>(rr), s.color);
        }
    }

    TWorld* world_;
    TCanvas* canvas_;
};

} // namespace enjin2
