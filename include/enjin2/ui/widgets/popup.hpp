#pragma once

#include "../component.hpp"
#include "../components.hpp"
#include "../system.hpp"
#include "../theme.hpp"
#include "../../core/types.hpp"
#include "../../graphics/canvas.hpp"
#include "../../graphics/primitives.hpp"
#include "../../graphics/text_renderer.hpp"
#include <algorithm>
#include <cstdint>
#include <string>

/**
 * @file popup.hpp
 * @brief Circular modal popup on the ui ECS
 *
 * Upstreamed from Eisei's `PopUpUI` (#121) as a data-only @ref PopUpComponent (two
 * text lines, an icon glyph, and an optional auto-hide timer) and a @ref PopUpSystem
 * that draws the circular card and its icon to an `ICanvas<Pixel4>`.
 *
 * The popup is a centered modal, so — unlike the top-left widgets — its
 * PositionComponent marks the circle **center**. The auto-hide countdown is the
 * pure @ref PopUpComponent::advance seam, mirroring the list's marquee: the system
 * ticks it before drawing. Icon artwork is drawn from primitives rather than a
 * bitmap so the popup carries no assets.
 */

namespace enjin2 {

/**
 * @brief Data-only state for a circular modal popup
 *
 * Holds the two message lines, the icon selector, styling, and the visibility /
 * auto-hide clock. The countdown is exposed through @ref advance as a pure seam so
 * it can be pinned without a canvas.
 */
struct PopUpComponent : public Component<PopUpComponent> {
    /// @brief Which built-in glyph to draw above the text (drawn from primitives).
    enum class Icon : uint8_t { None, Save, Info, Warning };

    std::string line1;              ///< Top message line (truncated to @ref kMaxChars)
    std::string line2;              ///< Bottom message line (truncated to @ref kMaxChars)
    Icon icon = Icon::None;         ///< Icon glyph to draw
    const GFXfont* font = nullptr;  ///< Text font (nullptr = built-in 5x7)
    uint8_t fontSize = 1;           ///< Integer text scale
    int radius = 44;                ///< Card radius in pixels
    Pixel4 rimColor = Pixel4(6);    ///< Card outline color
    Pixel4 textColor = Pixel4(14);  ///< Text color

    bool visible = false;           ///< Whether the popup is currently shown
    uint16_t autoHideMs = 0;        ///< Auto-hide delay in ms (0 = manual dismiss)
    uint16_t elapsedMs = 0;         ///< Time shown so far, in ms

    /// @brief Message lines are clipped to this many characters (matches PopUpUI).
    static constexpr size_t kMaxChars = 18;

    /// @brief Set both message lines, truncating each to @ref kMaxChars.
    void setLines(const std::string& l1, const std::string& l2) {
        line1 = l1.substr(0, kMaxChars);
        line2 = l2.substr(0, kMaxChars);
    }

    /// @brief Select the icon glyph.
    void setIcon(Icon i) { icon = i; }

    /**
     * @brief Show the popup, optionally arming an auto-hide countdown
     * @param autoHideMs_ Delay before the popup hides itself (0 = manual)
     */
    void show(uint16_t autoHideMs_ = 0) {
        autoHideMs = autoHideMs_;
        elapsedMs = 0;
        visible = true;
    }

    /// @brief Hide the popup and disarm the countdown.
    void hide() {
        autoHideMs = 0;
        elapsedMs = 0;
        visible = false;
    }

    /// @brief Whether the popup is currently shown.
    bool isVisible() const { return visible; }

    /**
     * @brief Advance the auto-hide countdown (time-based seam)
     * @param dtMs Elapsed time in milliseconds
     *
     * A no-op when hidden or when auto-hide is disarmed; otherwise it accrues time
     * (saturating at the uint16 ceiling) and hides the popup once the delay is
     * reached. Extracted from `PopUpUI::Update` so it can be pinned without drawing.
     */
    void advance(float dtMs) {
        if (!visible || autoHideMs == 0) return;
        if (elapsedMs >= autoHideMs) {
            hide();
            return;
        }
        const uint32_t next = static_cast<uint32_t>(elapsedMs) + static_cast<uint32_t>(dtMs);
        elapsedMs = static_cast<uint16_t>(std::min<uint32_t>(next, 0xFFFF));
    }
};

/**
 * @brief Ticks and draws every PopUpComponent entity to a Pixel4 canvas
 * @tparam TWorld World composing PopUpComponent and PositionComponent
 * @tparam TCanvas Pixel4 canvas type (e.g. Canvas4<W,H>)
 *
 * Advances each popup's auto-hide clock, then — for the visible ones — draws the
 * filled card, its rim, the icon glyph and the two centered message lines. The
 * entity's PositionComponent is the card center.
 */
template<typename TWorld, typename TCanvas>
class PopUpSystem : public System<PopUpSystem<TWorld, TCanvas>> {
public:
    /**
     * @brief Construct against the world it draws and the canvas it draws to
     * @param world World holding the popup entities (borrowed, not owned)
     * @param canvas Target Pixel4 canvas (borrowed, not owned)
     */
    PopUpSystem(TWorld* world, TCanvas* canvas) : world_(world), canvas_(canvas) {}

    /**
     * @brief Advance every popup's clock and draw the visible ones
     * @param dt Time since last update in seconds
     */
    void update(float dt) override {
        if (!world_ || !canvas_) return;
        const float dtMs = dt * 1000.0f;
        for (Entity e : world_->template query<PopUpComponent, PositionComponent>()) {
            auto* popup = world_->template get<PopUpComponent>(e);
            auto* pos = world_->template get<PositionComponent>(e);
            if (!popup || !pos) continue;
            popup->advance(dtMs);
            if (popup->isVisible()) draw(*popup, *pos);
        }
    }

    /// @brief Popups are the topmost chrome.
    int getPriority() const override { return 1000; }

private:
    void draw(const PopUpComponent& popup, const PositionComponent& pos) {
        const int16_t cx = pos.position.x;
        const int16_t cy = pos.position.y;
        const int16_t r = static_cast<int16_t>(popup.radius);

        // Filled card + rim.
        Primitives<Pixel4>::fillCircle(*canvas_, cx, cy, r, Pixel4(0));
        Primitives<Pixel4>::drawCircle(*canvas_, cx, cy, r, popup.rimColor);

        drawIcon(popup, cx, cy);
        drawLines(popup, cx, cy);
    }

    void drawIcon(const PopUpComponent& popup, int16_t cx, int16_t cy) {
        constexpr int16_t iconSize = 12;
        const int16_t left = cx - iconSize / 2;
        const int16_t top = cy - 28;
        switch (popup.icon) {
            case PopUpComponent::Icon::Save:
                Primitives<Pixel4>::fillRect(*canvas_, Rect(left, top, iconSize, iconSize + 2), Pixel4(12));
                Primitives<Pixel4>::fillRect(*canvas_, Rect(left + 2, top + 2, iconSize - 4, 4), Pixel4(0));
                Primitives<Pixel4>::fillRect(*canvas_, Rect(left + 3, top + 7, iconSize - 6, 3), Pixel4(8));
                Primitives<Pixel4>::drawRect(*canvas_, Rect(left, top, iconSize, iconSize + 2), Pixel4(6));
                break;
            case PopUpComponent::Icon::Info:
                Primitives<Pixel4>::fillCircle(*canvas_, cx, top + 7, iconSize / 2 + 1, Pixel4(10));
                Primitives<Pixel4>::fillCircle(*canvas_, cx, top + 3, 1, Pixel4(0));
                Primitives<Pixel4>::fillRect(*canvas_, Rect(cx - 1, top + 6, 3, 6), Pixel4(0));
                break;
            case PopUpComponent::Icon::Warning:
                Primitives<Pixel4>::fillTriangle(*canvas_, cx - 6, top + iconSize + 4,
                                                 cx + 6, top + iconSize + 4, cx, top - 2, Pixel4(10));
                Primitives<Pixel4>::fillRect(*canvas_, Rect(cx - 1, top + 3, 2, 6), Pixel4(0));
                Primitives<Pixel4>::fillRect(*canvas_, Rect(cx - 1, top + 11, 2, 2), Pixel4(0));
                break;
            case PopUpComponent::Icon::None:
            default:
                break;
        }
    }

    void drawLines(const PopUpComponent& popup, int16_t cx, int16_t cy) {
        text_.setFont(popup.font);
        text_.setTextSize(popup.fontSize);
        text_.setTextColor(popup.textColor);
        // Two lines stacked below the card center. Vertical placement is by-eye at
        // Gate 2 (getTextBounds carries no glyph bearing), matching the list/label.
        drawCentered(popup.line1, cx, cy + 2);
        drawCentered(popup.line2, cx, cy + 18);
    }

    void drawCentered(const std::string& s, int16_t cx, int16_t y) {
        if (s.empty()) return;
        const int w = static_cast<int>(text_.getTextWidth(s.c_str()));
        text_.drawString(*canvas_, static_cast<int16_t>(cx - w / 2), y, s.c_str());
    }

    TWorld* world_;
    TCanvas* canvas_;
    TextRenderer<Pixel4> text_;
};

} // namespace enjin2
