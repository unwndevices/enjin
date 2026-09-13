#pragma once

#include "../core/types.hpp"
#include "../graphics/sprite.hpp"
#include "style.hpp"

#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

namespace enjin2 {

struct DialogOptions {
    Style style{kDefaultStyles[static_cast<int>(StyleSlot::Popup)]};
    Rect box{4, 100, 152, 56};
    float cps{30.0f};
    const GFXfont* font{nullptr};
    const SpriteSheet* portrait{nullptr};
    uint16_t portraitFrame{0};
};

/// One retained dialog box: layout, type-on timing, and advance state.
class Dialog {
public:
    void show(const std::string& text, const DialogOptions& options = {});
    void advance();
    void update(float dtSeconds);
    void draw(ICanvas<Pixel4>& canvas) const;
    void hide();

    bool active() const { return active_; }
    bool revealing() const;
    uint16_t page() const { return active_ ? static_cast<uint16_t>(pageIndex_ + 1) : 0; }
    uint16_t pageCount() const { return static_cast<uint16_t>(pages_.size()); }
    bool done() const { return done_; }
    size_t revealedCharacters() const { return revealedCharacters_; }

private:
    struct Page {
        std::vector<std::string> lines;
        size_t characterCount{0};
    };

    void paginate(const std::string& text);
    void appendLine(Page& page, std::string line, size_t linesPerPage);
    size_t currentPageCharacters() const;
    /// Line advance in px: the face's yAdvance, or the built-in 5×7's 8.
    size_t lineHeightPx() const;

    DialogOptions options_{};
    std::vector<Page> pages_;
    size_t pageIndex_{0};
    size_t revealedCharacters_{0};
    double revealCredit_{0.0};
    bool active_{false};
    bool done_{false};
};

} // namespace enjin2
