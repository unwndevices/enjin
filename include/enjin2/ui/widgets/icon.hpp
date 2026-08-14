#pragma once

#include "../component.hpp"
#include "../reflect.hpp"
#include "../components.hpp"
#include "../system.hpp"
#include "../../core/types.hpp"
#include "../../graphics/canvas.hpp"
#include <cstdint>

/**
 * @file icon.hpp
 * @brief Grayscale-bitmap icon on the ui ECS
 *
 * Upstreamed from Eisei's `Icon`/`C_Sprite` (#121) as a data-only
 * @ref IconComponent (a borrowed grayscale bitmap plus its matte) and an
 * @ref IconSystem that blits it to an `ICanvas<Pixel4>`.
 *
 * The icon's size is intrinsic to its bitmap, so — unlike the list/label — the
 * entity pairs the component with just a PositionComponent (top-left); there is no
 * layout box. Pixels equal to the @ref IconComponent::matte value are treated as
 * transparent (Eisei's 16-is-clear convention), so the same buffer carries both
 * the artwork (values 0..15) and its cutout.
 */

namespace enjin2 {

/**
 * @brief Data-only state for a grayscale-bitmap icon
 *
 * Borrows a byte-per-pixel grayscale buffer (values 0..15, one nibble each) whose
 * dimensions travel with it. The blit is the @ref IconSystem's job; @ref sampleAt
 * and @ref isOpaqueAt expose the transparency test as a pure seam.
 */
struct IconComponent : public Component<IconComponent> {
    const uint8_t* bitmap = nullptr; ///< Borrowed grayscale pixels, row-major (not owned)
    uint16_t width = 0;              ///< Bitmap width in pixels
    uint16_t height = 0;             ///< Bitmap height in pixels
    uint8_t matte = kTransparent;    ///< Pixel value treated as transparent
    bool visible = true;             ///< Skipped by the system when false

    /// @brief Sentinel pixel value the blit skips (Eisei's clear color).
    static constexpr uint8_t kTransparent = 16;

    /// @brief Construct over a borrowed bitmap (or empty).
    IconComponent(const uint8_t* data = nullptr, uint16_t w = 0, uint16_t h = 0)
        : bitmap(data), width(w), height(h) {}

    /// @brief Point at a new bitmap of the given size.
    void load(const uint8_t* data, uint16_t w, uint16_t h) {
        bitmap = data;
        width = w;
        height = h;
    }

    /// @brief Show or hide the icon.
    void setVisible(bool v) { visible = v; }
    /// @brief Set the pixel value treated as transparent.
    void setMatte(uint8_t m) { matte = m; }

    /**
     * @brief Raw pixel value at a bitmap coordinate (pure)
     * @param x Column in [0, width)
     * @param y Row in [0, height)
     * @return The stored value, or @ref matte for an out-of-range or absent pixel
     */
    uint8_t sampleAt(int x, int y) const {
        if (!bitmap || x < 0 || y < 0 || x >= width || y >= height) return matte;
        return bitmap[static_cast<size_t>(y) * width + x];
    }

    /**
     * @brief Whether a bitmap coordinate paints (pure)
     * @param x Column in [0, width)
     * @param y Row in [0, height)
     * @return true if the pixel is in-range and not the matte value
     */
    bool isOpaqueAt(int x, int y) const { return sampleAt(x, y) != matte; }
};

/// @brief Serializable properties of @ref IconComponent (see reflect.hpp).
/// The bitmap serializes as an AssetRegistry name reference (assets are
/// reference-only); width/height travel as plain fields beside it.
#define ENJIN2_ICON_COMPONENT_FIELDS(FIELD, PROP) \
    FIELD(bitmap)                                 \
    FIELD(width)                                  \
    FIELD(height)                                 \
    FIELD(matte)                                  \
    FIELD(visible)

ENJIN2_REFLECT_COMPONENT(IconComponent, 5, "icon", ENJIN2_ICON_COMPONENT_FIELDS)

/**
 * @brief Blits every IconComponent entity to a Pixel4 canvas
 * @tparam TWorld World composing IconComponent and PositionComponent
 * @tparam TCanvas Pixel4 canvas type (e.g. Canvas4<W,H>)
 *
 * Each visible icon is copied at its entity's position; matte-valued pixels are
 * left untouched so the background shows through. Off-canvas pixels are clipped by
 * the canvas itself.
 */
template<typename TWorld, typename TCanvas>
class IconSystem : public System<IconSystem<TWorld, TCanvas>> {
public:
    /**
     * @brief Construct against the world it draws and the canvas it draws to
     * @param world World holding the icon entities (borrowed, not owned)
     * @param canvas Target Pixel4 canvas (borrowed, not owned)
     */
    IconSystem(TWorld* world, TCanvas* canvas) : world_(world), canvas_(canvas) {}

    /**
     * @brief Blit every visible icon
     * @param dt Time since last update in seconds (unused; icons are static)
     */
    void update(float dt) override {
        (void)dt;
        if (!world_ || !canvas_) return;
        for (Entity e : world_->template query<IconComponent, PositionComponent>()) {
            auto* icon = world_->template get<IconComponent>(e);
            auto* pos = world_->template get<PositionComponent>(e);
            if (!icon || !pos) continue;
            draw(*icon, *pos);
        }
    }

    /// @brief Icons render alongside the other widgets, above scene content.
    int getPriority() const override { return 900; }

private:
    void draw(const IconComponent& icon, const PositionComponent& pos) {
        if (!icon.visible || !icon.bitmap) return;
        const Point origin = pos.renderOrigin(Size(icon.width, icon.height));
        const int originX = origin.x;
        const int originY = origin.y;
        for (int y = 0; y < icon.height; ++y) {
            for (int x = 0; x < icon.width; ++x) {
                const uint8_t v = icon.sampleAt(x, y);
                if (v == icon.matte) continue;
                canvas_->setPixel(static_cast<int16_t>(originX + x),
                                  static_cast<int16_t>(originY + y), Pixel4(v));
            }
        }
    }

    TWorld* world_;
    TCanvas* canvas_;
};

} // namespace enjin2
