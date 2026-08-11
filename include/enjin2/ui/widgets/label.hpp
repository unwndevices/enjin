#pragma once

#include "../component.hpp"
#include "../reflect.hpp"
#include "../components.hpp"
#include "../system.hpp"
#include "../../core/types.hpp"
#include "../../graphics/canvas.hpp"
#include "../../graphics/primitives.hpp"
#include "../../graphics/text_renderer.hpp"
#include <string>
#include <vector>

/**
 * @file label.hpp
 * @brief Multi-line centered text label on the ui ECS
 *
 * Upstreamed from Eisei's `C_Label` (#121) as a data-only @ref LabelComponent
 * (the string plus its styling) and a @ref LabelSystem that word-wraps, centers
 * and draws it to an `ICanvas<Pixel4>` through a `TextRenderer<Pixel4>`.
 *
 * Like the rest of the widget layer the label is presentation-only: the text
 * arrives pre-formatted (the old `SetValue` float/unit projection moves to the
 * host), and the entity carries its bounds through a PositionComponent (top-left)
 * and SizeComponent, matching list.hpp. The optional rounded background box now
 * draws through `Primitives::fillRoundRect` (upstreamed in the same pass).
 */

namespace enjin2 {

/**
 * @brief Data-only state for a centered, word-wrapped text label
 *
 * Holds the string and its styling; wrapping and layout are the @ref LabelSystem's
 * job. The word-wrap itself is exposed as the pure, measurer-injected @ref wrapText
 * seam so it can be pinned without a font or a canvas.
 */
struct LabelComponent : public Component<LabelComponent> {
    std::string text;               ///< Pre-formatted label text (host does any projection)
    const GFXfont* font = nullptr;  ///< Glyph font (nullptr = built-in 5x7)
    uint8_t fontSize = 1;           ///< Integer text scale
    Pixel4 color = Pixel4(14);      ///< Text (and box outline) color
    Pixel4 background = Pixel4(0);  ///< Box fill; 0 leaves the label transparent
    uint8_t pointer = 0;            ///< Tail height below the box (0 = no tail)

    /// @brief Corner radius of the background box (matches C_Label's radius-8 panel).
    static constexpr int kBoxRadius = 8;
    /// @brief Vertical gap between wrapped lines, in pixels.
    static constexpr int kLineSpacing = 4;

    /// @brief Construct with an initial (pre-formatted) string.
    explicit LabelComponent(std::string text_ = {}) : text(std::move(text_)) {}

    /// @brief Replace the displayed string.
    void setText(std::string s) { text = std::move(s); }
    /// @brief Set the glyph font (nullptr selects the built-in font).
    void setFont(const GFXfont* f) { font = f; }
    /// @brief Set the integer text scale.
    void setFontSize(uint8_t size) { fontSize = size; }
    /// @brief Set the text/outline color.
    void setColor(Pixel4 c) { color = c; }
    /// @brief Set the box fill color (0 for a transparent label).
    void setBackground(Pixel4 c) { background = c; }

    /**
     * @brief Greedily word-wrap text to a pixel width (pure seam)
     * @tparam Measure Callable `int(const std::string&)` returning a string's pixel width
     * @param text Source string (space-separated words)
     * @param maxWidth Width budget in pixels; a word alone may still exceed it
     * @param measure Text-width measurer (the system passes the TextRenderer's)
     * @return The wrapped lines, top to bottom
     *
     * Mirrors `C_Label::Draw`'s pre-pass: build each line by appending words while
     * the measured line still fits, spilling to a new line when it would overflow.
     * Injecting the measurer keeps the wrap testable without font metrics.
     */
    template<typename Measure>
    static std::vector<std::string> wrapText(const std::string& text, int maxWidth, Measure measure) {
        std::vector<std::string> lines;
        std::string current;
        size_t i = 0;
        const size_t n = text.size();
        while (i < n) {
            // Pull the next space-delimited word (skip runs of spaces).
            while (i < n && text[i] == ' ') ++i;
            size_t start = i;
            while (i < n && text[i] != ' ') ++i;
            if (start == i) break;
            std::string word = text.substr(start, i - start);

            std::string candidate = current.empty() ? word : current + " " + word;
            if (!current.empty() && measure(candidate) > maxWidth) {
                lines.push_back(current);
                current = word;
            } else {
                current = std::move(candidate);
            }
        }
        if (!current.empty()) lines.push_back(current);
        return lines;
    }
};

/// @brief Serializable properties of @ref LabelComponent (see reflect.hpp).
#define ENJIN2_LABEL_COMPONENT_FIELDS(FIELD, PROP) \
    FIELD(text)                                    \
    FIELD(font)                                    \
    FIELD(fontSize)                                \
    FIELD(color)                                   \
    FIELD(background)                              \
    FIELD(pointer)

ENJIN2_REFLECT_COMPONENT(LabelComponent, 4, "label", ENJIN2_LABEL_COMPONENT_FIELDS)

/**
 * @brief Draws every LabelComponent entity to a Pixel4 canvas
 * @tparam TWorld World composing LabelComponent, PositionComponent, SizeComponent
 * @tparam TCanvas Pixel4 canvas type (e.g. Canvas4<W,H>)
 *
 * Wraps the text to the widget width, centers the block vertically and each line
 * horizontally, and — when a background color is set — draws a rounded panel (plus
 * an optional downward tail) beneath it.
 */
template<typename TWorld, typename TCanvas>
class LabelSystem : public System<LabelSystem<TWorld, TCanvas>> {
public:
    /**
     * @brief Construct against the world it draws and the canvas it draws to
     * @param world World holding the label entities (borrowed, not owned)
     * @param canvas Target Pixel4 canvas (borrowed, not owned)
     */
    LabelSystem(TWorld* world, TCanvas* canvas) : world_(world), canvas_(canvas) {}

    /**
     * @brief Draw every label entity
     * @param dt Time since last update in seconds (unused; labels are static)
     */
    void update(float dt) override {
        (void)dt;
        if (!world_ || !canvas_) return;
        for (Entity e : world_->template query<LabelComponent, PositionComponent, SizeComponent>()) {
            auto* label = world_->template get<LabelComponent>(e);
            auto* pos = world_->template get<PositionComponent>(e);
            auto* size = world_->template get<SizeComponent>(e);
            if (!label || !pos || !size) continue;
            draw(*label, *pos, *size);
        }
    }

    /// @brief Labels render on top of scene content, alongside lists.
    int getPriority() const override { return 900; }

private:
    void draw(const LabelComponent& label, const PositionComponent& pos,
              const SizeComponent& size) {
        if (label.text.empty() && label.background.value == 0) return;

        text_.setFont(label.font);
        text_.setTextSize(label.fontSize);

        const int originX = pos.position.x;
        const int originY = pos.position.y;
        const int width = size.size.width;
        const int boxHeight = size.size.height - label.pointer;

        auto measure = [this](const std::string& s) {
            return static_cast<int>(text_.getTextWidth(s.c_str()));
        };
        std::vector<std::string> lines = LabelComponent::wrapText(label.text, width, measure);

        // C_Label's block metrics (BASE @941a9ab6): each line contributes its
        // own ink height — getTextBounds reports ink extents again (unwn #161
        // restore), so no line-height probe or eye-tuned offset is needed.
        struct LineInk {
            int16_t x1, y1;
            uint16_t w, h;
        };
        std::vector<LineInk> ink(lines.size());
        int totalHeight = 0;
        for (size_t i = 0; i < lines.size(); ++i) {
            // Measured at the canvas's wrap boundary so the bounds walk agrees
            // with drawString's wrap (#161) — a no-op for lines wrapText already
            // fit to the box, but a single over-long word measures as drawn.
            text_.getTextBounds(lines[i].c_str(), 0, 0, &ink[i].x1, &ink[i].y1,
                                &ink[i].w, &ink[i].h, canvas_->getWidth());
            totalHeight += static_cast<int>(ink[i].h);
        }
        if (!lines.empty())
            totalHeight += (static_cast<int>(lines.size()) - 1) * LabelComponent::kLineSpacing;

        // Background panel + optional tail, drawn under the text.
        if (label.background.value != 0) {
            Rect box(originX, originY, static_cast<uint16_t>(width),
                     static_cast<uint16_t>(boxHeight));
            Primitives<Pixel4>::fillRoundRect(*canvas_, box, LabelComponent::kBoxRadius, label.background);
            Primitives<Pixel4>::drawRoundRect(*canvas_, box, LabelComponent::kBoxRadius, label.color);
            if (label.pointer > 0) {
                const int baseY = originY + boxHeight - 1;
                const int tipY = originY + boxHeight + label.pointer - 1;
                const int cx = originX + width / 2;
                Primitives<Pixel4>::fillTriangle(*canvas_, cx - 3, baseY, cx + 3, baseY,
                                                 cx, tipY, label.background);
                Primitives<Pixel4>::drawLine(*canvas_, cx - 3, baseY, cx, tipY, label.color);
                Primitives<Pixel4>::drawLine(*canvas_, cx + 3, baseY, cx, tipY, label.color);
            }
        }

        text_.setTextColor(label.color);

        // Center the text block vertically within the box, then each line's ink
        // within the width, subtracting the ink bearings (`- x1`, `- y1`) to
        // convert box position to cursor/baseline — C_Label's formula, exact
        // again now that the bearings are real (unwn #161 restore).
        int cursorY = originY + (boxHeight - totalHeight) / 2;
        if (cursorY < originY) cursorY = originY;
        for (size_t i = 0; i < lines.size(); ++i) {
            int x = originX + (width - static_cast<int>(ink[i].w)) / 2;
            if (x < originX) x = originX;
            text_.drawString(*canvas_,
                             static_cast<int16_t>(x - ink[i].x1),
                             static_cast<int16_t>(cursorY - ink[i].y1),
                             lines[i].c_str());
            cursorY += static_cast<int>(ink[i].h) + LabelComponent::kLineSpacing;
        }
    }

    TWorld* world_;
    TCanvas* canvas_;
    TextRenderer<Pixel4> text_;
};

} // namespace enjin2
