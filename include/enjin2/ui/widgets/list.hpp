#pragma once

#include "../component.hpp"
#include "../reflect.hpp"
#include "../components.hpp"
#include "../system.hpp"
#include "../theme.hpp"
#include "../../core/types.hpp"
#include "../../graphics/canvas.hpp"
#include "../../graphics/primitives.hpp"
#include "../../graphics/text_renderer.hpp"
#include <algorithm>
#include <string>
#include <vector>

/**
 * @file list.hpp
 * @brief Scrolling selection list widget on the ui ECS
 *
 * Upstreamed from Eisei's `C_List` (#121). The rewrite is deliberately not a port
 * of the old Canvas8 member API: the widget is split into a data-only
 * @ref ListComponent (state the host mutates) and a @ref ListSystem that draws it
 * to an `ICanvas<Pixel4>` through a `TextRenderer<Pixel4>`, themed via theme.hpp.
 *
 * Presentation-only by design: items arrive **pre-stringified** — the old
 * `getString(const T&)` projection moves out to the scene/host edge — and the
 * widget never touches input. The host drives selection; the system draws.
 */

namespace enjin2 {

/**
 * @brief Data-only state for a vertical, center-pinned selection list
 *
 * Holds the visible strings and the cursor, plus the marquee and smooth-scroll
 * runtime state the @ref ListSystem advances each frame. Bounds are not stored
 * here — an entity pairs this with a PositionComponent and SizeComponent, matching
 * the rest of the ui ECS.
 */
struct ListComponent : public Component<ListComponent> {
    std::vector<std::string> items; ///< Pre-stringified rows (projection done host-side)
    TextAlign textAlign;            ///< Horizontal alignment of each row

    const GFXfont* font = nullptr;  ///< Glyph font (nullptr = built-in 5x7)
    uint8_t fontSize = 1;           ///< Integer text scale
    uint8_t itemSpacing = 4;        ///< Extra pixels between rows
    int selectionOffset = 0;        ///< Rows to bias the pinned selection off-center

    // Marquee timing for the (overflowing) selected row, in milliseconds.
    uint16_t marqueeStartDelay = 600; ///< Pause before scrolling starts
    uint16_t marqueeSpeed = 50;       ///< Delay between one-pixel steps
    uint16_t marqueeEndDelay = 1000;  ///< Pause at the end before wrapping home

    /// @brief Padding inset from the row's leading edge (non-center alignments).
    static constexpr int kTextPadding = 3;
    /// @brief In-between frames blended when the selection jumps by one row.
    static constexpr int kScrollTransitionFrames = 2;
    /// @brief Unselected rows are truncated to this many characters (they don't marquee).
    static constexpr size_t kUnselectedMaxChars = 12;
    /// @brief Downward pixel nudge that visually centers the pinned selected row.
    static constexpr int kSelectionCenterBias = 4;

    /**
     * @brief Construct with an initial (pre-stringified) item set
     * @param items_ Row strings, top to bottom
     * @param align Horizontal alignment (default left)
     */
    ListComponent(std::vector<std::string> items_ = {}, TextAlign align = TextAlign::Left)
        : items(std::move(items_)), textAlign(align) {}

    // ----- Host-driven selection (presentation-only setters) -----

    /// @brief Number of rows currently held.
    int itemCount() const { return static_cast<int>(items.size()); }

    /// @brief Index of the selected row (0 when empty).
    int currentSelectionIndex() const { return selectedIndex_; }

    /// @brief The selected row's string (empty string when the list is empty).
    const std::string& currentSelection() const {
        static const std::string kEmpty;
        if (items.empty()) return kEmpty;
        return items[static_cast<size_t>(selectedIndex_)];
    }

    /// @brief Move the cursor up one row, clamping at the top.
    void moveUp() {
        if (selectedIndex_ > 0) --selectedIndex_;
    }

    /// @brief Move the cursor down one row, clamping at the bottom.
    void moveDown() {
        if (selectedIndex_ < itemCount() - 1) ++selectedIndex_;
    }

    /// @brief Jump the cursor to @p index, saturating into the valid range.
    void setCurrentSelection(int index) {
        selectedIndex_ = clampIndex(index);
    }

    /// @brief Swap in a new item set, reclamping the cursor to the new range.
    void updateItems(std::vector<std::string> newItems) {
        items = std::move(newItems);
        selectedIndex_ = clampIndex(selectedIndex_);
    }

    /// @brief Set the horizontal alignment of every row.
    void setTextAlignment(TextAlign align) { textAlign = align; }

    /// @brief Set marquee timings (milliseconds) for the overflowing selected row.
    void setMarqueeTiming(uint16_t startDelayMs, uint16_t speedMs, uint16_t endDelayMs) {
        marqueeStartDelay = startDelayMs;
        marqueeSpeed = speedMs;
        marqueeEndDelay = endDelayMs;
    }

    /// @brief Set the glyph font (nullptr selects the built-in font).
    void setFont(const GFXfont* f) { font = f; }
    /// @brief Set the integer text scale.
    void setFontSize(uint8_t size) { fontSize = size; }

    // ----- Runtime state the ListSystem drives (public so the system can reach it) -----

    /// @brief Current marquee scroll offset of the selected row, in pixels.
    int marqueeOffset() const { return marqueeOffset_; }

    /**
     * @brief Advance the marquee for the selected row (time-based seam)
     * @param dtMs Elapsed time in milliseconds
     * @param textWidth Rendered width of the selected row
     * @param maxTextWidth Width available before the row overflows
     *
     * When the row fits, the offset is pinned home. When it overflows, the offset
     * holds through the start delay, then steps one pixel per @ref marqueeSpeed,
     * and wraps back to zero once it has dwelt at the end for @ref marqueeEndDelay.
     * Extracted from `C_List::Update` so it can be pinned without a canvas.
     */
    void advanceMarquee(float dtMs, int textWidth, int maxTextWidth) {
        if (textWidth <= maxTextWidth) {
            marqueeOffset_ = 0;
            marqueeTimer_ = 0.0f;
            return;
        }
        if (marqueeOffset_ > textWidth - maxTextWidth) {
            // Dwell at the end, then wrap home.
            marqueeTimer_ += dtMs;
            if (marqueeTimer_ > marqueeEndDelay) {
                marqueeOffset_ = 0;
                marqueeTimer_ = 0.0f;
            }
        } else {
            marqueeTimer_ += dtMs;
            if (marqueeOffset_ == 0 && marqueeTimer_ < marqueeStartDelay) return;
            if (marqueeTimer_ > marqueeSpeed) {
                ++marqueeOffset_;
                marqueeTimer_ = 0.0f;
            }
        }
    }

    /**
     * @brief Latch a selection change so the next draw can blend the scroll
     * @return true if the selection moved since the last call
     *
     * Resets the marquee and arms the smooth-scroll blend on a change. Kept on the
     * component (not the system) so the pinned/animated state travels with the data.
     */
    bool consumeSelectionChange() {
        if (selectedIndex_ == previousIndex_) return false;
        scrollFrom_ = static_cast<float>(previousIndex_);
        scrollFramesLeft_ = kScrollTransitionFrames;
        marqueeOffset_ = 0;
        marqueeTimer_ = 0.0f;
        previousIndex_ = selectedIndex_;
        return true;
    }

    /**
     * @brief Fractional row position to render this frame (for smooth scroll)
     * @return An index that eases from the old selection to the new one
     *
     * Advances the blend by one in-between frame per call; once the transition is
     * spent it simply returns the integer selection.
     */
    float visualIndex() {
        float vi = static_cast<float>(selectedIndex_);
        if (scrollFramesLeft_ > 0) {
            float t = 1.0f - static_cast<float>(scrollFramesLeft_) /
                                 static_cast<float>(kScrollTransitionFrames + 1);
            vi = scrollFrom_ + t * (static_cast<float>(selectedIndex_) - scrollFrom_);
            --scrollFramesLeft_;
        }
        return vi;
    }

private:
    int clampIndex(int index) const {
        if (items.empty()) return 0;
        if (index < 0) return 0;
        if (index > itemCount() - 1) return itemCount() - 1;
        return index;
    }

    int selectedIndex_ = 0;
    int previousIndex_ = 0;
    int marqueeOffset_ = 0;
    float marqueeTimer_ = 0.0f;
    float scrollFrom_ = 0.0f;
    int scrollFramesLeft_ = 0;
};

/// @brief Serializable properties of @ref ListComponent (see reflect.hpp).
/// `items` precedes the `selectedIndex` prop: setCurrentSelection clamps
/// against the current item count. Marquee/scroll runtime state stays transient.
#define ENJIN2_LIST_COMPONENT_FIELDS(FIELD, PROP) \
    FIELD(items)                                  \
    FIELD(textAlign)                              \
    FIELD(font)                                   \
    FIELD(fontSize)                               \
    FIELD(itemSpacing)                            \
    FIELD(selectionOffset)                        \
    FIELD(marqueeStartDelay)                      \
    FIELD(marqueeSpeed)                           \
    FIELD(marqueeEndDelay)                        \
    PROP(selectedIndex, int, currentSelectionIndex, setCurrentSelection)

ENJIN2_REFLECT_COMPONENT(ListComponent, 9, "list", ENJIN2_LIST_COMPONENT_FIELDS)

/**
 * @brief Draws every ListComponent entity to a Pixel4 canvas
 * @tparam TWorld World composing ListComponent, PositionComponent, SizeComponent
 * @tparam TCanvas Pixel4 canvas type (e.g. Canvas4<W,H>)
 *
 * The selected row is pinned near the vertical center and highlighted with the
 * theme accent; unselected rows slide above and below it and are drawn muted. A
 * one-row selection jump is blended across a couple of in-between frames, and the
 * selected row marquees horizontally when it overflows the widget width.
 */
template<typename TWorld, typename TCanvas>
class ListSystem : public System<ListSystem<TWorld, TCanvas>> {
public:
    /**
     * @brief Construct against the world it draws and the canvas it draws to
     * @param world World holding the list entities (borrowed, not owned)
     * @param canvas Target Pixel4 canvas (borrowed, not owned)
     * @param theme Palette/metrics to style with (defaults to the dark theme)
     */
    ListSystem(TWorld* world, TCanvas* canvas, Theme theme = kDefaultTheme)
        : world_(world), canvas_(canvas), theme_(theme) {}

    /**
     * @brief Advance and draw every list entity
     * @param dt Time since last update in seconds
     */
    void update(float dt) override {
        if (!world_ || !canvas_) return;
        const float dtMs = dt * 1000.0f;

        for (Entity e : world_->template query<ListComponent, PositionComponent, SizeComponent>()) {
            auto* list = world_->template get<ListComponent>(e);
            auto* pos = world_->template get<PositionComponent>(e);
            auto* size = world_->template get<SizeComponent>(e);
            if (!list || !pos || !size) continue;
            draw(*list, *pos, *size, dtMs);
        }
    }

    /// @brief Lists render on top of scene content but below overlays.
    int getPriority() const override { return 900; }

private:
    void draw(ListComponent& list, const PositionComponent& pos,
              const SizeComponent& size, float dtMs) {
        if (list.items.empty()) return;

        list.consumeSelectionChange();
        const float visualIndex = list.visualIndex();
        const int selected = list.currentSelectionIndex();

        text_.setFont(list.font);
        text_.setTextSize(list.fontSize);

        const Point origin = pos.renderOrigin(size.size);
        const int originX = origin.x;
        const int originY = origin.y;
        const int width = size.size.width;
        const int height = size.size.height;
        const int maxTextWidth = width - ListComponent::kTextPadding;

        // Advance the marquee against the selected row's rendered width.
        const int selectedWidth =
            static_cast<int>(text_.getTextWidth(list.currentSelection().c_str()));
        list.advanceMarquee(dtMs, selectedWidth, maxTextWidth);

        // Row height from the "Ag" ink box (ascender/descender sample) — BASE
        // C_List's own probe, meaningful again now that getTextBounds returns
        // ink extents rather than yAdvance (unwn #161 restore).
        int16_t tx, ty;
        uint16_t tw, th;
        text_.getTextBounds("Ag", 0, 0, &tx, &ty, &tw, &th);
        const int itemHeight = static_cast<int>(th) + list.itemSpacing;
        if (itemHeight <= 0) return;

        const int boundsTop = originY;
        const int boundsBottom = originY + height;

        // The selected row is pinned near center; the rest hang off a shared base.
        const int selectedY = boundsTop + (height / 2) + ListComponent::kSelectionCenterBias +
                              list.selectionOffset * itemHeight;
        const int scrollPixelOffset =
            static_cast<int>((visualIndex - static_cast<float>(selected)) *
                             static_cast<float>(itemHeight));
        const int yBase = selectedY - selected * itemHeight - scrollPixelOffset;

        const int count = list.itemCount();
        const int start = std::max(0, (boundsTop - yBase) / itemHeight - 1);
        const int end = std::min(count, (boundsBottom - yBase) / itemHeight + 2);

        for (int i = start; i < end; ++i) {
            const bool isSelected = (i == selected);
            const int y = isSelected ? selectedY : (yBase + i * itemHeight);

            // Cull rows that fall outside the widget's vertical bounds.
            if (y > boundsBottom || (y - static_cast<int>(th)) < boundsTop) continue;

            // Unselected rows are truncated; the selected row scrolls in full.
            std::string textStr = list.items[static_cast<size_t>(i)];
            if (!isSelected && textStr.size() > ListComponent::kUnselectedMaxChars) {
                textStr = textStr.substr(0, ListComponent::kUnselectedMaxChars);
            }

            const int w = static_cast<int>(text_.getTextWidth(textStr.c_str()));
            int alignOffset = 0;
            switch (list.textAlign) {
                case TextAlign::Left: alignOffset = 0; break;
                case TextAlign::Center: alignOffset = (width - w) / 2; break;
                case TextAlign::Right: alignOffset = (width - w); break;
            }

            if (isSelected) {
                // C_List's bar placement (BASE @941a9ab6): the bar tops out at the
                // glyph ascent — `ty` is the "Ag" probe's Y-bearing, negative for
                // GFX fonts — now that getTextBounds reports the ink box again
                // (unwn #161 restore; the Gate-2 eye-tuned offset is gone).
                const int top = y + static_cast<int>(ty) - list.itemSpacing / 2 - 1;
                Rect bar(originX, top, static_cast<uint16_t>(width),
                         static_cast<uint16_t>(itemHeight));
                Primitives<Pixel4>::fillRoundRect(*canvas_, bar, 2, theme_.accent);
                text_.setTextColor(theme_.accentText);
            } else {
                text_.setTextColor(theme_.muted);
            }

            const int pad = (list.textAlign == TextAlign::Center) ? 0 : ListComponent::kTextPadding;
            const int x = originX + pad + alignOffset - (isSelected ? list.marqueeOffset() : 0);
            text_.drawString(*canvas_, static_cast<int16_t>(x), static_cast<int16_t>(y),
                             textStr.c_str());
        }
    }

    TWorld* world_;
    TCanvas* canvas_;
    Theme theme_;
    TextRenderer<Pixel4> text_;
};

} // namespace enjin2
