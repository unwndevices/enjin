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
 * @file bar.hpp
 * @brief Linear meter widget on the ui ECS (unwn #206)
 *
 * A lightweight linear level meter — the straight-line sibling of the circular
 * @ref GaugeComponent, not a gauge mode (spec §Widget set). Its @ref value is
 * bindable and rides the ParamRegistry resolver like any other reflected
 * property (unwn #202): the scene VM writes the reflected `value` field each
 * tick, no bar-specific mechanism. The meter fills proportionally along its
 * @ref length in the chosen @ref orientation:
 *
 *  - **Horizontal** fills left→right.
 *  - **Vertical** fills bottom→top (matching the unidirectional gauge).
 *
 * The value→pixel mapping is exposed as the pure @ref fillRegion seam so it can
 * be pinned without a canvas. The bar is placed by its entity's Position;
 * @ref length / @ref thickness are its own extent (the editor still drags a
 * Position+Size box to place it).
 */

namespace enjin2 {

/**
 * @brief Fill axis for a @ref BarComponent
 *
 * Serializes as its integer value (the inspector edits it as an enum field).
 * Append only, never reorder — the order is the permanent wire encoding.
 */
enum class BarOrientation : uint8_t {
    Horizontal = 0, ///< Fills left→right along `length`
    Vertical = 1    ///< Fills bottom→top along `length`
};

/**
 * @brief Data-only state for a linear level meter
 *
 * Holds the meter's extent (@ref length along the fill axis, @ref thickness
 * across it), @ref orientation, @ref color and the current (clamped) @ref
 * value. The value→pixel geometry is the @ref fillRegion pure seam; the @ref
 * BarSystem consumes it to paint the filled span at the entity position.
 */
struct BarComponent : public Component<BarComponent> {
    uint16_t length = 0;                                  ///< Extent along the fill axis
    uint16_t thickness = 1;                               ///< Extent across the fill axis
    BarOrientation orientation = BarOrientation::Horizontal; ///< Fill axis
    Pixel4 color = Pixel4(15);                            ///< Fill color

    /// @brief Construct with an extent, orientation and color.
    BarComponent(uint16_t len = 0, uint16_t thick = 1,
                 BarOrientation o = BarOrientation::Horizontal, Pixel4 c = Pixel4(15))
        : length(len), thickness(thick), orientation(o), color(c) {}

    /// @brief Current meter value in [0, 1].
    float value() const { return value_; }

    /**
     * @brief Set the meter value, clamped to [0, 1]
     * @param v Fill fraction; values outside [0, 1] saturate
     *
     * The bindable write point: a `value` binding resolves here through the
     * reflected property setter, so an out-of-range live value can never paint
     * past the meter's ends.
     */
    void setValue(float v) { value_ = std::clamp(v, 0.0f, 1.0f); }

    /// @brief Set the fill color.
    void setColor(Pixel4 c) { color = c; }

    /**
     * @brief Rectangle of the filled span, in bar-local pixels (pure)
     * @return The filled band; empty (zero extent) at value 0
     *
     * Horizontal grows left→right from the origin; vertical grows bottom→top,
     * so the filled band hugs the meter's lower end.
     */
    Rect fillRegion() const {
        // Truncate, matching GaugeComponent::fillRegion — the two meters are
        // siblings and must map the same fraction to the same fill length.
        const int filled = static_cast<int>(length * value_);
        if (orientation == BarOrientation::Horizontal) {
            return Rect(0, 0, static_cast<uint16_t>(filled), thickness);
        }
        return Rect(0, static_cast<int16_t>(static_cast<int>(length) - filled),
                    thickness, static_cast<uint16_t>(filled));
    }

private:
    float value_ = 0.0f;
};

/// @brief Serializable properties of @ref BarComponent (see reflect.hpp).
/// `value` is a bindable property (clamped in setValue); length/thickness/
/// orientation/color are plain fields.
#define ENJIN2_BAR_COMPONENT_FIELDS(FIELD, PROP) \
    FIELD(length)                                \
    FIELD(thickness)                             \
    FIELD(orientation)                           \
    FIELD(color)                                 \
    PROP(value, float, value, setValue)

ENJIN2_REFLECT_COMPONENT(BarComponent, 15, "bar", ENJIN2_BAR_COMPONENT_FIELDS)

/**
 * @brief Draws every BarComponent entity to a Pixel4 canvas
 * @tparam TWorld World composing BarComponent and PositionComponent
 * @tparam TCanvas Pixel4 canvas type (e.g. Canvas4<W,H>)
 *
 * Paints each meter's filled span (@ref BarComponent::fillRegion) at its entity
 * position; an empty or zero-length bar draws nothing. Off-canvas pixels are
 * clipped by the canvas.
 */
template<typename TWorld, typename TCanvas>
class BarSystem : public System<BarSystem<TWorld, TCanvas>> {
public:
    /**
     * @brief Construct against the world it draws and the canvas it draws to
     * @param world World holding the bar entities (borrowed, not owned)
     * @param canvas Target Pixel4 canvas (borrowed, not owned)
     */
    BarSystem(TWorld* world, TCanvas* canvas) : world_(world), canvas_(canvas) {}

    /**
     * @brief Draw every bar entity
     * @param dt Time since last update in seconds (unused; bars are static)
     */
    void update(float dt) override {
        (void)dt;
        if (!world_ || !canvas_) return;
        for (Entity e : world_->template query<BarComponent, PositionComponent>()) {
            auto* bar = world_->template get<BarComponent>(e);
            auto* pos = world_->template get<PositionComponent>(e);
            if (!bar || !pos) continue;
            // The bar's bounding box is length along the fill axis, thickness
            // across it — so a vertical bar swaps the two for the anchor.
            const Size box = bar->orientation == BarOrientation::Horizontal
                                 ? Size(bar->length, bar->thickness)
                                 : Size(bar->thickness, bar->length);
            draw(*bar, pos->renderOrigin(box));
        }
    }

    /// @brief Bars are level indicators, drawn just above the gauges (950) so a
    /// priority-sorted rig matches the hand-ordered ScenePlayer draw sequence.
    int getPriority() const override { return 955; }

private:
    void draw(const BarComponent& bar, const Point& pos) {
        if (bar.length == 0 || bar.thickness == 0) return;
        const Rect region = bar.fillRegion();
        if (region.width == 0 || region.height == 0) return;
        Primitives<Pixel4>::fillRect(
            *canvas_,
            Rect(static_cast<int16_t>(pos.x + region.x), static_cast<int16_t>(pos.y + region.y),
                 region.width, region.height),
            bar.color);
    }

    TWorld* world_;
    TCanvas* canvas_;
};

} // namespace enjin2
