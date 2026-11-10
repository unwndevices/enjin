#ifndef ENJIN2_GRAPHICS_TEXT_RENDERER_HPP
#define ENJIN2_GRAPHICS_TEXT_RENDERER_HPP

// The one text route (ADR-0029): every string on every host, from native code
// and from Lua, is drawn here, in one of the three faces (faces/faces.hpp).
//
// The rules:
//  - y is the top of the line; the baseline is y + ascent * size.
//  - A string's width is its advance (the sum of its glyphs' xAdvance).
//  - Text is UTF-8. A character the face lacks draws a tofu (a hollow box one
//    advance wide) and is logged once per character and face.
//  - Overflow is one setting, wrap | clip | ellipsis, on a box that runs from
//    the anchor to the given width, or to the layer's edge when none is given.
//    Wrapping breaks at spaces, and inside a word only when the word is longer
//    than the line.
//
// The state (face, colour, overflow, box width, cursor, scale, outline, shader,
// clip window) lives on the canvas (TextState, text_state.hpp): a renderer is a
// view of one canvas's state, cheap to make, and every setting it makes stays
// on that canvas until changed.

#include "canvas.hpp"
#include "effect.hpp"
#include "faces/faces.hpp"
#include "palette.hpp"
#include "text_state.hpp"

#include <cstdint>
#include <cstdio>
#include <string_view>
#include <type_traits>

namespace enjin2 {

namespace text {

/// Decode one UTF-8 code point from s at i, advancing i. A malformed sequence
/// yields U+FFFD and consumes one byte.
inline uint32_t decodeUtf8(std::string_view s, size_t& i) {
    const auto byte = [&](size_t k) { return static_cast<uint8_t>(s[k]); };
    const uint8_t b0 = byte(i);
    if (b0 < 0x80) { ++i; return b0; }
    int extra = 0;
    uint32_t cp = 0;
    if ((b0 & 0xE0) == 0xC0) { extra = 1; cp = b0 & 0x1F; }
    else if ((b0 & 0xF0) == 0xE0) { extra = 2; cp = b0 & 0x0F; }
    else if ((b0 & 0xF8) == 0xF0) { extra = 3; cp = b0 & 0x07; }
    else { ++i; return 0xFFFD; }
    if (i + extra >= s.size()) { ++i; return 0xFFFD; }
    for (int k = 1; k <= extra; ++k) {
        const uint8_t b = byte(i + k);
        if ((b & 0xC0) != 0x80) { ++i; return 0xFFFD; }
        cp = (cp << 6) | (b & 0x3F);
    }
    static constexpr uint32_t kMin[4] = {0, 0x80, 0x800, 0x10000};
    if (cp < kMin[extra] || cp > 0x10FFFF || (cp >= 0xD800 && cp <= 0xDFFF)) { ++i; return 0xFFFD; }
    i += static_cast<size_t>(extra) + 1;
    return cp;
}

/// The glyph for @p cp in @p f, or nullptr when the face lacks it.
inline const GFXglyph* glyph(const Face& f, uint32_t cp) {
    const GFXfont& font = *f.font;
    if (cp < font.first || cp > font.last) return nullptr;
    const GFXglyph& g = font.glyph[cp - font.first];
    return g.xAdvance ? &g : nullptr;
}

/// The tofu's advance in @p f at size 1: the advance of '?'.
inline int tofuAdvance(const Face& f) {
    const GFXglyph* q = glyph(f, '?');
    return q ? q->xAdvance : f.capHeight;
}

/// The advance of @p cp in @p f at size 1 (a tofu's when it is missing).
inline int advance(const Face& f, uint32_t cp) {
    const GFXglyph* g = glyph(f, cp);
    return g ? g->xAdvance : tofuAdvance(f);
}

/// Where a missing character is reported; replaceable (setMissingLog).
using MissingLog = void (*)(const Face& face, uint32_t cp);

inline void defaultMissingLog(const Face& face, uint32_t cp) {
    std::printf("text: U+%04X is not in %s; drawn as a tofu\n", static_cast<unsigned>(cp), face.name);
}

inline MissingLog& missingLog() {
    static MissingLog log = &defaultMissingLog;
    return log;
}

/// Report @p cp missing from @p face once per character and face. The first 64
/// pairs are remembered; past them nothing more is logged (a log line a frame
/// would be worse than a quiet one).
inline void reportMissing(const Face& face, uint32_t cp) {
    struct Seen { const Face* face; uint32_t cp; };
    static Seen seen[64];
    static int n = 0;
    for (int i = 0; i < n; ++i)
        if (seen[i].face == &face && seen[i].cp == cp) return;
    if (n == 64) return;
    seen[n++] = {&face, cp};
    if (MissingLog log = missingLog()) log(face, cp);
}

/// The width of @p s, unwrapped, at @p size: the widest line's advance
/// (a newline starts a line).
inline int width(const Face& f, std::string_view s, uint8_t size = 1) {
    int widest = 0, w = 0;
    for (size_t i = 0; i < s.size();) {
        const uint32_t cp = decodeUtf8(s, i);
        if (cp == '\n') { widest = w > widest ? w : widest; w = 0; continue; }
        if (cp == '\r') continue;
        w += advance(f, cp);
    }
    widest = w > widest ? w : widest;
    return widest * (size ? size : 1);
}

/// The line height (the face's yAdvance) at @p size.
inline int lineHeight(const Face& f, uint8_t size = 1) { return f.font->yAdvance * (size ? size : 1); }

/// How far the capitals' top sits under the line's top, at @p size: a screen
/// laid out by cap tops draws at `capTop - capOffset(face, size)`.
inline constexpr int capOffset(const Face& f, uint8_t size = 1) { return (f.ascent - f.capHeight) * (size ? size : 1); }

/// @p s without its trailing spaces.
inline std::string_view trimRight(std::string_view s) {
    while (!s.empty() && s.back() == ' ') s.remove_suffix(1);
    return s;
}

/// Where the line starting at @p start ends in the paragraph @p p (exclusive):
/// after the last word that fits in @p boxW, or inside a word longer than the line.
inline size_t wrapEnd(const Face& f, uint8_t size, std::string_view p, size_t start, int boxW) {
    int w = 0;
    size_t lastBreak = std::string_view::npos;  // a space that ends a fitting word
    size_t i = start;
    while (i < p.size()) {
        const size_t at = i;
        const uint32_t cp = decodeUtf8(p, i);
        if (cp == '\r') continue;
        if (cp == ' ') lastBreak = at;
        const int adv = advance(f, cp) * size;
        if (cp != ' ' && w + adv > boxW && at > start) {
            if (lastBreak != std::string_view::npos && lastBreak > start) return lastBreak;
            return at;
        }
        w += adv;
    }
    return p.size();
}

/**
 * @brief Split @p s into the lines TextRenderer::draw would draw, calling
 *        fn(line) for each: a newline ends a line; with Wrap, a paragraph wider
 *        than @p boxW breaks at its spaces (inside a word only when the word is
 *        longer than the line), the spaces at a break dropped. Clip and
 *        Ellipsis keep one line per paragraph (cut when drawn).
 */
template <typename Fn>
void forEachLine(const Face& f, uint8_t size, TextOverflow overflow, std::string_view s, int boxW, Fn&& fn) {
    if (!size) size = 1;
    size_t para = 0;
    while (true) {
        size_t nl = s.find('\n', para);
        if (nl == std::string_view::npos) nl = s.size();
        const std::string_view p = s.substr(para, nl - para);
        if (overflow == TextOverflow::Wrap) {
            size_t start = 0;
            do {
                const size_t end = wrapEnd(f, size, p, start, boxW);
                fn(trimRight(p.substr(start, end - start)));
                start = end;
                while (start < p.size() && p[start] == ' ') ++start;
            } while (start < p.size());
        } else {
            fn(p);
        }
        if (nl == s.size()) break;
        para = nl + 1;
    }
}

}  // namespace text

/**
 * @brief Draws text on one canvas, through that canvas's sticky TextState.
 *
 *     TextRenderer<Pixel4> t(layer);
 *     t.setFont(faces::body);
 *     t.setTextWrap(TextOverflow::Ellipsis, 80);
 *     t.draw("Track 01", 10, 4);          // y = the top of the line
 *     t.draw("12:30", 150, 4, TextAlign::Right);
 */
template <typename TPixel>
class TextRenderer {
public:
    explicit TextRenderer(ICanvas<TPixel>& canvas) : canvas_(canvas), s_(canvas.text()) {}

    // ---- the state (sticky, on the canvas) ---------------------------------

    TextRenderer& setFont(const Face& face) { s_.face = &face; return *this; }
    const Face& face() const { return s_.face ? *s_.face : faces::monogram; }

    TextRenderer& setTextColor(uint8_t index) { s_.color = index; return *this; }
    template <typename P = TPixel, typename = std::enable_if_t<!std::is_same_v<P, uint8_t>>>
    TextRenderer& setTextColor(TPixel p) { s_.color = toIndex(p); return *this; }

    /// Integer scale: 1 = native size; 0 counts as 1.
    TextRenderer& setTextSize(uint8_t size) { s_.size = size ? size : 1; return *this; }
    uint8_t textSize() const { return s_.size; }

    /// The overflow rule and the box's width (0: to the layer's edge).
    TextRenderer& setTextWrap(TextOverflow overflow, int16_t width = 0) {
        s_.overflow = overflow;
        s_.width = width > 0 ? width : 0;
        return *this;
    }

    TextRenderer& setCursor(int16_t x, int16_t y) { s_.cursorX = x; s_.cursorY = y; return *this; }

    /// The legibility outline, in the bg role.
    TextRenderer& setOutline(bool on) { s_.outline = on; s_.outlineColor = ROLE_BG; return *this; }
    TextRenderer& setOutline(bool on, uint8_t index) { s_.outline = on; s_.outlineColor = index; return *this; }

    /// Run every fill pixel through @p fx (sampled at absolute canvas coordinates).
    TextRenderer& setEffect(const Effect& fx) { s_.effect = fx; s_.hasEffect = true; return *this; }
    TextRenderer& clearEffect() { s_.hasEffect = false; s_.effect = Effect{}; return *this; }

    /// Draw text only inside [x, x+w) x [y, y+h).
    TextRenderer& setClip(int16_t x, int16_t y, int16_t w, int16_t h) {
        s_.clipX0 = x; s_.clipY0 = y;
        s_.clipX1 = static_cast<int16_t>(x + (w > 0 ? w : 0));
        s_.clipY1 = static_cast<int16_t>(y + (h > 0 ? h : 0));
        return *this;
    }
    TextRenderer& clearClip() {
        s_.clipX0 = s_.clipY0 = INT16_MIN;
        s_.clipX1 = s_.clipY1 = INT16_MAX;
        return *this;
    }

    /// Every setting back to its default (monogram, the text role, wrap at the edge, ...).
    TextRenderer& reset() { s_ = TextState{}; return *this; }

    // ---- measuring ----------------------------------------------------------

    /// The advance of @p s at the current face and size (the widest line).
    int width(std::string_view s) const { return text::width(face(), s, s_.size); }
    int lineHeight() const { return text::lineHeight(face(), s_.size); }
    int ascent() const { return face().ascent * s_.size; }
    /// The line top that puts the capitals' top at @p capTop (text::capOffset).
    int topForCap(int capTop) const { return capTop - text::capOffset(face(), s_.size); }

    // ---- drawing ------------------------------------------------------------

    /**
     * @brief Draw @p s with the top of its first line at y.
     *
     * x is the line's left edge, centre or right edge (@p align). The box the
     * overflow rule applies to: for Left it runs from x for the state's width
     * (or to the layer's right edge), for Right it ends at x (from the left
     * edge), for Center it is centred on x (as wide as fits on the layer).
     *
     * @return the number of lines drawn. The cursor is left at the end of the
     *         last line.
     */
    int draw(std::string_view s, int x, int y, TextAlign align = TextAlign::Left) {
        const Face& f = face();
        const int boxW = boxWidth(x, align);
        int left;
        switch (align) {
            case TextAlign::Left: left = x; break;
            case TextAlign::Right: left = x - boxW; break;
            default: left = x - boxW / 2; break;
        }
        const int right = left + boxW;
        const int lineH = text::lineHeight(f, s_.size);

        int lines = 0;
        int lineTop = y;
        int endX = x;
        const bool wrap = s_.overflow == TextOverflow::Wrap;
        const bool ellipsis = s_.overflow == TextOverflow::Ellipsis;
        text::forEachLine(f, s_.size, s_.overflow, s, boxW, [&](std::string_view line) {
            endX = drawLine(f, line, x, align, lineTop, left, right,
                            !wrap && ellipsis && text::width(f, line, s_.size) > boxW);
            ++lines;
            lineTop += lineH;
        });
        s_.cursorX = static_cast<int16_t>(endX);
        s_.cursorY = static_cast<int16_t>(lineTop - lineH);
        return lines;
    }

    /// Draw @p s at the cursor (Adafruit's print), left-aligned; the cursor moves on.
    int print(std::string_view s) { return draw(s, s_.cursorX, s_.cursorY); }

private:
    ICanvas<TPixel>& canvas_;
    TextState& s_;

    static constexpr uint8_t toIndex(TPixel p) {
        if constexpr (std::is_same_v<TPixel, uint8_t>) return p;
        else return p.value;
    }
    static constexpr TPixel fromIndex(uint8_t i) {
        if constexpr (std::is_same_v<TPixel, uint8_t>) return i;
        else return TPixel(i);
    }

    int boxWidth(int x, TextAlign align) const {
        if (s_.width > 0) return s_.width;
        const int w = canvas_.getWidth();
        switch (align) {
            case TextAlign::Left: return w - x > 0 ? w - x : 0;
            case TextAlign::Right: return x > 0 ? x : 0;
            default: {
                const int half = x < w - x ? x : w - x;
                return half > 0 ? 2 * half : 0;
            }
        }
    }

    /// Draw one line; returns the pen's x after it.
    int drawLine(const Face& f, std::string_view line, int x, TextAlign align, int lineTop,
                 int left, int right, bool ellipsis) {
        const int size = s_.size;
        std::string_view body = line;
        int dots = 0;
        if (ellipsis) {
            // The longest head that leaves room for "...".
            const int dotsW = 3 * text::advance(f, '.') * size;
            const int room = (right - left) - dotsW;
            int w = 0;
            size_t i = 0, fit = 0;
            while (i < line.size()) {
                const uint32_t cp = text::decodeUtf8(line, i);
                if (cp == '\r') { fit = i; continue; }
                const int adv = text::advance(f, cp) * size;
                if (w + adv > room) break;
                w += adv;
                fit = i;
            }
            body = text::trimRight(line.substr(0, fit));
            dots = 3;
        }
        const int lineW = text::width(f, body, s_.size) + dots * text::advance(f, '.') * size;
        int pen;
        switch (align) {
            case TextAlign::Left: pen = x; break;
            case TextAlign::Right: pen = x - lineW; break;
            default: pen = x - lineW / 2; break;
        }
        // Clip and Ellipsis cut at the box; Wrap draws what didn't fit (one
        // glyph wider than the line) rather than lose it.
        int cx0 = s_.clipX0, cx1 = s_.clipX1;
        if (s_.overflow != TextOverflow::Wrap) {
            if (left > cx0) cx0 = left;
            if (right < cx1) cx1 = right;
        }
        const int baseline = lineTop + f.ascent * size;
        // The outline pass under the whole line, then the fill.
        if (s_.outline) {
            static const int8_t kOff[4][2] = {{-1, 0}, {1, 0}, {0, -1}, {0, 1}};
            for (const auto& o : kOff) run(f, body, dots, pen + o[0], baseline + o[1], cx0, cx1, true);
        }
        return run(f, body, dots, pen, baseline, cx0, cx1, false);
    }

    /// Walk @p body (then @p dots periods) from @p pen, stamping each glyph.
    int run(const Face& f, std::string_view body, int dots, int pen, int baseline, int cx0, int cx1,
            bool outlinePass) {
        const int size = s_.size;
        for (size_t i = 0; i < body.size();) {
            const uint32_t cp = text::decodeUtf8(body, i);
            if (cp == '\r') continue;
            pen += stamp(f, cp, pen, baseline, cx0, cx1, outlinePass) * size;
        }
        for (int d = 0; d < dots; ++d) pen += stamp(f, '.', pen, baseline, cx0, cx1, outlinePass) * size;
        return pen;
    }

    void plot(int px, int py, int cx0, int cx1, bool outlinePass) {
        if (px < cx0 || px >= cx1 || py < s_.clipY0 || py >= s_.clipY1) return;
        if (px < 0 || py < 0 || px >= canvas_.getWidth() || py >= canvas_.getHeight()) return;
        uint8_t c;
        if (outlinePass) c = s_.outlineColor;
        else if (s_.hasEffect) c = s_.effect.shadePixel(s_.color, static_cast<int16_t>(px), static_cast<int16_t>(py));
        else c = s_.color;
        canvas_.setPixel(static_cast<int16_t>(px), static_cast<int16_t>(py), fromIndex(c));
    }

    void block(int bx, int by, int cx0, int cx1, bool outlinePass) {
        const int size = s_.size;
        for (int sy = 0; sy < size; ++sy)
            for (int sx = 0; sx < size; ++sx) plot(bx + sx, by + sy, cx0, cx1, outlinePass);
    }

    /// Stamp one character with its pen at (pen, baseline); returns its advance at size 1.
    int stamp(const Face& f, uint32_t cp, int pen, int baseline, int cx0, int cx1, bool outlinePass) {
        const int size = s_.size;
        const GFXglyph* g = text::glyph(f, cp);
        if (!g) {
            if (!outlinePass) text::reportMissing(f, cp);
            // The tofu: a hollow box one advance wide (less a column), cap high.
            const int adv = text::tofuAdvance(f);
            const int w = adv - 1 > 2 ? adv - 1 : 2;
            const int h = f.capHeight > 2 ? f.capHeight : 2;
            const int top = -h;
            for (int yy = 0; yy < h; ++yy)
                for (int xx = 0; xx < w; ++xx)
                    if (yy == 0 || yy == h - 1 || xx == 0 || xx == w - 1)
                        block(pen + xx * size, baseline + (top + yy) * size, cx0, cx1, outlinePass);
            return adv;
        }
        const uint8_t* bits = f.font->bitmap + g->bitmapOffset;
        int bit = 0;
        for (int row = 0; row < g->height; ++row) {
            for (int col = 0; col < g->width; ++col, ++bit) {
                if (!(bits[bit >> 3] & (0x80u >> (bit & 7)))) continue;
                block(pen + (g->xOffset + col) * size, baseline + (g->yOffset + row) * size, cx0, cx1, outlinePass);
            }
        }
        return g->xAdvance;
    }
};

/**
 * @brief Keeps a canvas's text state as it was: saved now, put back when the
 *        scope ends. A screen that draws on a layer it shares (the UI layer, the
 *        System layer, an applet's layer a host draws on too) sets what it needs
 *        inside one, so nothing it set reaches the layer's next user.
 *
 *     TextScope scope(layer);
 *     TextRenderer<Pixel4> t(layer);
 *     t.setFont(faces::body);  ...
 */
template <typename TPixel>
class TextScope {
public:
    explicit TextScope(ICanvas<TPixel>& canvas) : canvas_(canvas), saved_(canvas.text()) { canvas.text() = TextState{}; }
    ~TextScope() { canvas_.text() = saved_; }
    TextScope(const TextScope&) = delete;
    TextScope& operator=(const TextScope&) = delete;

private:
    ICanvas<TPixel>& canvas_;
    TextState saved_;
};

}  // namespace enjin2

#endif  // ENJIN2_GRAPHICS_TEXT_RENDERER_HPP
