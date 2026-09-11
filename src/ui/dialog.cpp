#include "enjin2/ui/dialog.hpp"

#include "enjin2/graphics/border.hpp"
#include "enjin2/graphics/text_renderer.hpp"

#include <algorithm>
#include <cmath>
#include <utility>

namespace enjin2 {

namespace {

uint16_t measuredWidth(const std::string& text, const GFXfont* font) {
    TextRenderer<Pixel4> renderer;
    renderer.setFont(font);
    return renderer.getTextWidth(text.c_str());
}

} // namespace

void Dialog::show(const std::string& text, const DialogOptions& options) {
    options_ = options;
    pageIndex_ = 0;
    revealedCharacters_ = 0;
    revealCredit_ = 0.0;
    done_ = false;
    paginate(text);
    active_ = true;
    if (options_.cps <= 0.0f) revealedCharacters_ = currentPageCharacters();
}

void Dialog::advance() {
    if (!active_) return;
    if (revealing()) {
        revealedCharacters_ = currentPageCharacters();
        revealCredit_ = 0.0;
        return;
    }
    if (pageIndex_ + 1 < pages_.size()) {
        ++pageIndex_;
        revealedCharacters_ = options_.cps <= 0.0f ? currentPageCharacters() : 0;
        revealCredit_ = 0.0;
        return;
    }
    active_ = false;
    done_ = true;
}

void Dialog::update(float dtSeconds) {
    if (!revealing() || dtSeconds <= 0.0f || options_.cps <= 0.0f) return;
    revealCredit_ += static_cast<double>(dtSeconds) * options_.cps;
    const size_t count = static_cast<size_t>(std::floor(revealCredit_ + 1e-6));
    if (count == 0) return;
    revealCredit_ -= static_cast<double>(count);
    revealedCharacters_ = std::min(currentPageCharacters(), revealedCharacters_ + count);
    if (!revealing()) revealCredit_ = 0.0;
}

bool Dialog::revealing() const {
    return active_ && revealedCharacters_ < currentPageCharacters();
}

void Dialog::hide() {
    active_ = false;
    done_ = false;
    revealedCharacters_ = 0;
    revealCredit_ = 0.0;
}

size_t Dialog::currentPageCharacters() const {
    return pageIndex_ < pages_.size() ? pages_[pageIndex_].characterCount : 0;
}

size_t Dialog::lineHeightPx() const {
    return options_.font ? options_.font->yAdvance : static_cast<size_t>(8);
}

void Dialog::appendLine(Page& page, std::string line, size_t linesPerPage) {
    if (page.lines.size() >= linesPerPage) {
        pages_.push_back(std::move(page));
        page = Page{};
    }
    page.characterCount += line.size();
    page.lines.push_back(std::move(line));
}

void Dialog::paginate(const std::string& text) {
    pages_.clear();
    const int inset = std::max<int>(1, options_.style.borderWidth) + options_.style.padding;
    int contentWidth = static_cast<int>(options_.box.width) - inset * 2;
    const int contentHeight = static_cast<int>(options_.box.height) - inset * 2;
    if (options_.portrait && options_.portrait->data) {
        contentWidth -= options_.portrait->cellW + options_.style.padding;
    }
    contentWidth = std::max(1, contentWidth);
    const size_t lineHeight = lineHeightPx();
    const size_t linesPerPage = std::max<size_t>(1, contentHeight > 0
        ? static_cast<size_t>(contentHeight) / std::max<size_t>(1, lineHeight) : 1);

    Page currentPage;
    std::string line;
    std::string word;

    auto flushLine = [&](bool forceEmpty) {
        if (!line.empty() || forceEmpty) appendLine(currentPage, std::move(line), linesPerPage);
        line.clear();
    };
    auto placeWord = [&]() {
        if (word.empty()) return;
        std::string candidate = line.empty() ? word : line + " " + word;
        if (measuredWidth(candidate, options_.font) <= contentWidth) {
            line = std::move(candidate);
            word.clear();
            return;
        }
        if (!line.empty()) flushLine(false);
        while (!word.empty()) {
            size_t fit = 0;
            for (size_t n = 1; n <= word.size(); ++n) {
                if (measuredWidth(word.substr(0, n), options_.font) > contentWidth) break;
                fit = n;
            }
            if (fit == 0) fit = 1;
            if (fit == word.size()) {
                line = std::move(word);
                word.clear();
            } else {
                appendLine(currentPage, word.substr(0, fit), linesPerPage);
                word.erase(0, fit);
            }
        }
    };
    auto forcePage = [&]() {
        placeWord();
        flushLine(false);
        if (!currentPage.lines.empty()) pages_.push_back(std::move(currentPage));
        currentPage = Page{};
    };

    for (char c : text) {
        if (c == '\f') {
            forcePage();
        } else if (c == '\n') {
            placeWord();
            flushLine(true);
        } else if (c == ' ' || c == '\t' || c == '\r') {
            placeWord();
        } else {
            word.push_back(c);
        }
    }
    placeWord();
    flushLine(false);
    if (!currentPage.lines.empty() || pages_.empty()) pages_.push_back(std::move(currentPage));
}

void Dialog::draw(ICanvas<Pixel4>& canvas) const {
    if (!active_ || pageIndex_ >= pages_.size()) return;

    canvas.fill(options_.box, options_.style.fill);
    BorderStyle border;
    border.color = options_.style.border.value;
    border.thickness = std::max<uint8_t>(1, options_.style.borderWidth);
    border.radius = options_.style.radius;
    border.kind = options_.style.borderKind <= static_cast<uint8_t>(BorderKind::DropShadow)
        ? static_cast<BorderKind>(options_.style.borderKind) : BorderKind::Solid;
    border.shadowDx = options_.style.shadowDx;
    border.shadowDy = options_.style.shadowDy;
    strokeBorder(canvas, options_.box, border);

    const int16_t inset = static_cast<int16_t>(border.thickness + options_.style.padding);
    int16_t textX = static_cast<int16_t>(options_.box.x + inset);
    const int16_t contentY = static_cast<int16_t>(options_.box.y + inset);
    if (options_.portrait && options_.portrait->data) {
        options_.portrait->draw(canvas, options_.portraitFrame, textX, contentY);
        textX = static_cast<int16_t>(textX + options_.portrait->cellW + options_.style.padding);
    }

    TextRenderer<Pixel4> renderer;
    renderer.setFont(options_.font);
    renderer.setTextColor(options_.style.text);
    renderer.setOutline(options_.style.textOutline != 0);
    renderer.setTextWrap(false);
    const int16_t lineHeight = static_cast<int16_t>(lineHeightPx());
    const int16_t baselineOffset = options_.font ? static_cast<int16_t>(lineHeight - 2) : 0;
    size_t remaining = revealedCharacters_;
    for (size_t i = 0; i < pages_[pageIndex_].lines.size() && remaining > 0; ++i) {
        const std::string& line = pages_[pageIndex_].lines[i];
        const size_t visible = std::min(remaining, line.size());
        const std::string clipped = line.substr(0, visible);
        renderer.drawString(canvas, textX,
            static_cast<int16_t>(contentY + baselineOffset + i * lineHeight), clipped.c_str());
        remaining -= visible;
    }
}

} // namespace enjin2
