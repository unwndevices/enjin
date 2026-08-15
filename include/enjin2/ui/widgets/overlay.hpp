#pragma once

#include "../component.hpp"
#include "../reflect.hpp"
#include "../system.hpp"
#include "../../core/types.hpp"
#include "../../graphics/canvas.hpp"
#include <cstdint>

/**
 * @file overlay.hpp
 * @brief Full-screen dimming overlay on the ui ECS
 *
 * Upstreamed from Eisei's `OverlayBg` (#121). That widget layered two things: a
 * subtractive full-screen fill that darkened everything behind a modal, and a
 * gradient sprite drawn on top. The rewrite keeps only the dimming here — the
 * gradient is an ordinary bitmap and belongs to an @ref IconComponent at the host
 * edge — so @ref OverlayComponent is just "darken the frame by N steps".
 *
 * The darkening is subtractive on the 4-bit value (like the old `BlendMode::Sub`
 * fill), pulled out as the pure @ref OverlayComponent::dim seam. The system walks
 * the whole canvas, so no PositionComponent is involved.
 */

namespace enjin2 {

/**
 * @brief Data-only state for a subtractive full-screen dim
 *
 * Holds how many grayscale steps to subtract from every pixel behind a modal. The
 * per-pixel math is the pure @ref dim seam; the @ref OverlaySystem applies it
 * across the canvas.
 */
struct OverlayComponent : public Component<OverlayComponent> {
    uint8_t opacity = 0; ///< Grayscale steps subtracted from each pixel (0 = clear)
    bool visible = true; ///< Skipped by the system when false

    /// @brief Construct with an initial opacity.
    explicit OverlayComponent(uint8_t opacity_ = 0) : opacity(opacity_) {}

    /// @brief Set the dimming strength (grayscale steps to subtract).
    void setOpacity(uint8_t o) { opacity = o; }
    /// @brief Show or hide the overlay.
    void setVisible(bool v) { visible = v; }

    /**
     * @brief Darken a single 4-bit pixel value (pure seam)
     * @param v Source pixel value (0..15)
     * @return `max(0, v - opacity)` — saturating so black stays black
     */
    uint8_t dim(uint8_t v) const {
        return (v > opacity) ? static_cast<uint8_t>(v - opacity) : 0;
    }
};

/// @brief Serializable properties of @ref OverlayComponent (see reflect.hpp).
#define ENJIN2_OVERLAY_COMPONENT_FIELDS(FIELD, PROP) \
    FIELD(opacity)                                   \
    FIELD(visible)

ENJIN2_REFLECT_COMPONENT(OverlayComponent, 7, "overlay", ENJIN2_OVERLAY_COMPONENT_FIELDS)

/**
 * @brief Dims the whole canvas for every visible OverlayComponent
 * @tparam TWorld World composing OverlayComponent
 * @tparam TCanvas Pixel4 canvas type (e.g. Canvas4<W,H>)
 *
 * Subtracts each overlay's opacity from every pixel already drawn, so it must run
 * after the scene content it darkens but before the modal chrome drawn on top.
 */
template<typename TWorld, typename TCanvas>
class OverlaySystem : public System<OverlaySystem<TWorld, TCanvas>> {
public:
    /**
     * @brief Construct against the world it reads and the canvas it dims
     * @param world World holding the overlay entities (borrowed, not owned)
     * @param canvas Target Pixel4 canvas (borrowed, not owned)
     */
    OverlaySystem(TWorld* world, TCanvas* canvas) : world_(world), canvas_(canvas) {}

    /**
     * @brief Apply every visible overlay's dim to the canvas
     * @param dt Time since last update in seconds (unused; overlays are static)
     */
    void update(float dt) override {
        if (!world_ || !canvas_) return;
        for (auto entry : world_->template components<OverlayComponent>())
            drawEntity(entry.first, dt);
    }

    /// @brief Dim the whole canvas for one overlay entity (the per-entity seam
    /// the scene player's z-sorted pass drives, unwn #243). Because the pass
    /// invokes this at the overlay's `z`, it dims the canvas *as drawn below it*
    /// — a mid-`z` overlay darkens scene content and leaves higher-`z` chrome
    /// untouched, which is the run-order fix (ADR-0014).
    void drawEntity(Entity e, float dt) {
        (void)dt; // overlays are static
        if (!world_ || !canvas_) return;
        const OverlayComponent* overlay = world_->template get<OverlayComponent>(e);
        if (!overlay || !overlay->visible || overlay->opacity == 0) return;
        const int w = canvas_->getWidth();
        const int h = canvas_->getHeight();
        for (int y = 0; y < h; ++y) {
            for (int x = 0; x < w; ++x) {
                const uint8_t v =
                    canvas_->getPixel(static_cast<int16_t>(x), static_cast<int16_t>(y));
                canvas_->setPixel(static_cast<int16_t>(x), static_cast<int16_t>(y),
                                  Pixel4(overlay->dim(v)));
            }
        }
    }

    /// @brief Runs above scene content but below the modal chrome it backs.
    int getPriority() const override { return 800; }

private:
    TWorld* world_;
    TCanvas* canvas_;
};

} // namespace enjin2
