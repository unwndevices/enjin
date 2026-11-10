#include "enjin2/ui/dialog.hpp"

#include "enjin2/graphics/border.hpp"
#include "enjin2/graphics/text_renderer.hpp"

#include <algorithm>
#include <cmath>
#include <utility>

namespace enjin2 {

namespace {

const Face& faceOf(const DialogOptions& o) { return o.font ? *o.font : faces::monogram; }

/// The first @p n bytes of @p s, cut back to a whole UTF-8 character.
std::string_view utf8Head(std::string_view s, size_t n) {
    if (n >= s.size()) return s;
    while (n > 0 && (static_cast<uint8_t>(s[n]) & 0xC0) == 0x80) --n;
    return s.substr(0, n);
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
    return static_cast<size_t>(text::lineHeight(faceOf(options_)));
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

    // The text renderer's own line breaking (ADR-0029): a form feed starts a
    // page, a newline a line; tabs and carriage returns read as spaces.
    std::string normal(text);
    for (char& c : normal)
        if (c == '\t' || c == '\r') c = ' ';
    Page currentPage;
    size_t chunk = 0;
    while (true) {
        size_t ff = normal.find('\f', chunk);
        if (ff == std::string::npos) ff = normal.size();
        const std::string_view part(normal.data() + chunk, ff - chunk);
        if (!part.empty())
            text::forEachLine(faceOf(options_), 1, TextOverflow::Wrap, part, contentWidth,
                              [&](std::string_view line) { appendLine(currentPage, std::string(line), linesPerPage); });
        if (ff == normal.size()) break;
        if (!currentPage.lines.empty()) pages_.push_back(std::move(currentPage));
        currentPage = Page{};
        chunk = ff + 1;
    }
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

    // The dialog's text settings are its own: set them on the canvas for its
    // lines and put the canvas's back after.
    const TextState saved = canvas.text();
    TextRenderer<Pixel4> renderer(canvas);
    renderer.reset();
    renderer.setFont(faceOf(options_));
    renderer.setTextColor(options_.style.text);
    renderer.setOutline(options_.style.textOutline != 0);
    renderer.setTextWrap(TextOverflow::Clip);
    const int16_t lineHeight = static_cast<int16_t>(lineHeightPx());
    size_t remaining = revealedCharacters_;
    for (size_t i = 0; i < pages_[pageIndex_].lines.size() && remaining > 0; ++i) {
        const std::string& line = pages_[pageIndex_].lines[i];
        const size_t visible = std::min(remaining, line.size());
        renderer.draw(utf8Head(line, visible), textX, static_cast<int16_t>(contentY + i * lineHeight));
        remaining -= visible;
    }
    canvas.text() = saved;
}

} // namespace enjin2
