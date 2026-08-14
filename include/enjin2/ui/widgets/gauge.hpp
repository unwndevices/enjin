#pragma once

#include "../component.hpp"
#include "../reflect.hpp"
#include "../components.hpp"
#include "../system.hpp"
#include "../theme.hpp"
#include "../../core/types.hpp"
#include "../../graphics/canvas.hpp"
#include "../../graphics/primitives.hpp"
#include <algorithm>
#include <cstdint>

/**
 * @file gauge.hpp
 * @brief Circular fill-up gauge (VU meter) on the ui ECS
 *
 * Upstreamed from Eisei's `FillUpGauge`/`C_FillUpGauge` (#121) as a data-only
 * @ref GaugeComponent (value + mode) and a @ref GaugeSystem that draws the dithered
 * fill, the level line and the rim to an `ICanvas<Pixel4>`.
 *
 * The old widget rendered through an offscreen canvas masked by a circle bitmap;
 * the rewrite drops that indirection and clips the fill to the rim analytically
 * while drawing straight onto the target. The value→geometry mapping (clamp, fill
 * rectangle, level line) is pulled out as pure seams so it can be pinned without a
 * canvas. The gauge is circular, so its size is intrinsic (its diameter) and the
 * entity pairs the component with just a PositionComponent (top-left).
 */

namespace enjin2 {

/**
 * @brief Fill direction for a @ref GaugeComponent
 */
enum class GaugeMode {
    Unidirectional, ///< Fills bottom-to-top for a value in [0, 1]
    Bidirectional   ///< Fills from the center outward for a value in [-1, 1]
};

/**
 * @brief Data-only state for a circular fill-up gauge
 *
 * Holds the diameter, rim color, mode and current (clamped) value. The value→pixel
 * geometry is exposed through @ref fillRegion and @ref levelLineY as pure seams;
 * the @ref GaugeSystem consumes them to draw.
 */
struct GaugeComponent : public Component<GaugeComponent> {
    uint16_t diameter = 0;             ///< Gauge width == height (a circle)
    Pixel4 rimColor = Pixel4(13);      ///< Rim and center-line color
    GaugeMode mode = GaugeMode::Unidirectional; ///< Fill direction

    /// @brief Grayscale value the dithered fill paints with.
    static constexpr uint8_t kFillValue = 8;
    /// @brief Grayscale value the level line paints with.
    static constexpr uint8_t kLineValue = 8;

    /// @brief Construct with a diameter, color and mode.
    GaugeComponent(uint16_t d = 0, Pixel4 color = Pixel4(13),
                   GaugeMode m = GaugeMode::Unidirectional)
        : diameter(d), rimColor(color), mode(m) {}

    /// @brief Current gauge value (clamped to the mode's range).
    float value() const { return value_; }

    /**
     * @brief Set the gauge value, clamped to the mode's range
     * @param v [0, 1] unidirectional, or [-1, 1] bidirectional
     */
    void setValue(float v) {
        value_ = (mode == GaugeMode::Bidirectional)
                     ? std::clamp(v, -1.0f, 1.0f)
                     : std::clamp(v, 0.0f, 1.0f);
    }

    /**
     * @brief Change the fill direction and reclamp the current value
     * @param m New mode
     */
    void setMode(GaugeMode m) {
        mode = m;
        setValue(value_); // reclamp into the new range
    }

    /// @brief Set the rim / center-line color.
    void setColor(Pixel4 c) { rimColor = c; }

    /**
     * @brief Rectangle of the dithered fill, in gauge-local pixels (pure)
     * @return The fill band spanning the full width; empty height at value 0
     *
     * Unidirectional grows up from the bottom; bidirectional grows out from the
     * center, above the midline for positive values and below it for negative.
     */
    Rect fillRegion() const {
        const int h = diameter;
        const int w = diameter;
        if (mode == GaugeMode::Unidirectional) {
            const int filled = static_cast<int>(h * value_);
            return Rect(0, static_cast<int16_t>(h - filled),
                        static_cast<uint16_t>(w), static_cast<uint16_t>(filled));
        }
        const int filled = static_cast<int>(std::abs(h * value_ / 2.0f));
        if (value_ >= 0.0f) {
            return Rect(0, static_cast<int16_t>(h / 2 - filled),
                        static_cast<uint16_t>(w), static_cast<uint16_t>(filled));
        }
        return Rect(0, static_cast<int16_t>(h / 2),
                    static_cast<uint16_t>(w), static_cast<uint16_t>(filled));
    }

    /**
     * @brief Gauge-local Y of the level line at the fill boundary (pure)
     * @return The row the boundary line is drawn on
     */
    int levelLineY() const {
        const int h = diameter;
        if (mode == GaugeMode::Unidirectional) {
            return h - static_cast<int>(h * value_);
        }
        const int filled = static_cast<int>(std::abs(h * value_ / 2.0f));
        return (value_ >= 0.0f) ? (h / 2 - filled) : (h / 2 + filled);
    }

private:
    float value_ = 0.0f;
};

/// @brief Serializable properties of @ref GaugeComponent (see reflect.hpp).
/// `mode` precedes the `value` prop: setValue clamps against the current mode.
#define ENJIN2_GAUGE_COMPONENT_FIELDS(FIELD, PROP) \
    FIELD(diameter)                                \
    FIELD(rimColor)                                \
    FIELD(mode)                                    \
    PROP(value, float, value, setValue)

ENJIN2_REFLECT_COMPONENT(GaugeComponent, 6, "gauge", ENJIN2_GAUGE_COMPONENT_FIELDS)

/**
 * @brief Draws every GaugeComponent entity to a Pixel4 canvas
 * @tparam TWorld World composing GaugeComponent and PositionComponent
 * @tparam TCanvas Pixel4 canvas type (e.g. Canvas4<W,H>)
 *
 * Paints the dithered fill (clipped to the rim circle), the level line and the rim
 * outline; a bidirectional gauge also draws its center line. The 4×4 dither is the
 * checkerboard C_FillUpGauge used, so the fill reads as a texture rather than a
 * solid block on the 4-bit display.
 */
template<typename TWorld, typename TCanvas>
class GaugeSystem : public System<GaugeSystem<TWorld, TCanvas>> {
public:
    /**
     * @brief Construct against the world it draws and the canvas it draws to
     * @param world World holding the gauge entities (borrowed, not owned)
     * @param canvas Target Pixel4 canvas (borrowed, not owned)
     */
    GaugeSystem(TWorld* world, TCanvas* canvas) : world_(world), canvas_(canvas) {}

    /**
     * @brief Draw every gauge entity
     * @param dt Time since last update in seconds (unused; gauges are static)
     */
    void update(float dt) override {
        (void)dt;
        if (!world_ || !canvas_) return;
        for (Entity e : world_->template query<GaugeComponent, PositionComponent>()) {
            auto* gauge = world_->template get<GaugeComponent>(e);
            auto* pos = world_->template get<PositionComponent>(e);
            if (!gauge || !pos) continue;
            draw(*gauge, *pos);
        }
    }

    /// @brief Gauges are overlay-level chrome, drawn above lists/labels.
    int getPriority() const override { return 950; }

private:
    // The checkerboard dither C_FillUpGauge filled with (0 = leave background).
    static constexpr uint8_t kPattern[16] = {
        8, 0, 8, 0,
        0, 0, 0, 8,
        8, 0, 8, 0,
        0, 8, 0, 0,
    };

    void draw(const GaugeComponent& gauge, const PositionComponent& pos) {
        if (gauge.diameter == 0) return;
        const Point origin = pos.renderOrigin(Size(gauge.diameter, gauge.diameter));
        const int originX = origin.x;
        const int originY = origin.y;
        const int r = gauge.diameter / 2;
        const int cx = originX + r;
        const int cy = originY + r;

        // Dithered fill, clipped analytically to the rim circle.
        const Rect region = gauge.fillRegion();
        for (int y = region.y; y < region.y + region.height; ++y) {
            for (int x = region.x; x < region.x + region.width; ++x) {
                if (!insideCircle(x, y, r)) continue;
                if (kPattern[(y % 4) * 4 + (x % 4)] == 0) continue;
                canvas_->setPixel(static_cast<int16_t>(originX + x),
                                  static_cast<int16_t>(originY + y), Pixel4(GaugeComponent::kFillValue));
            }
        }

        // Level line at the fill boundary, clipped to the circle.
        drawClippedHLine(originX, originY, gauge.levelLineY(), gauge.diameter, r,
                         Pixel4(GaugeComponent::kLineValue));

        // Bidirectional gauges keep a persistent center line.
        if (gauge.mode == GaugeMode::Bidirectional) {
            drawClippedHLine(originX, originY, r, gauge.diameter, r, gauge.rimColor);
        }

        // Rim.
        Primitives<Pixel4>::drawCircle(*canvas_, static_cast<int16_t>(cx),
                                       static_cast<int16_t>(cy), static_cast<int16_t>(r),
                                       gauge.rimColor);
    }

    /// @brief True if gauge-local (x,y) sits within the rim circle of radius r.
    static bool insideCircle(int x, int y, int r) {
        const int dx = x - r;
        const int dy = y - r;
        return dx * dx + dy * dy <= r * r;
    }

    /// @brief Draw a gauge-local horizontal line, keeping only pixels inside the rim.
    void drawClippedHLine(int originX, int originY, int localY, int width, int r, Pixel4 color) {
        for (int x = 0; x < width; ++x) {
            if (!insideCircle(x, localY, r)) continue;
            canvas_->setPixel(static_cast<int16_t>(originX + x),
                              static_cast<int16_t>(originY + localY), color);
        }
    }

    TWorld* world_;
    TCanvas* canvas_;
};

} // namespace enjin2
