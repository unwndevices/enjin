#ifndef ENJIN2_GRAPHICS_CORNER_WATCH_HPP
#define ENJIN2_GRAPHICS_CORNER_WATCH_HPP

// Corner cuts: text whose ink lands where a screen's rounded corners hide it.
//
// A screen with rounded corners shows a canvas less its four corners. Text
// drawn there is cut, and nothing in the drawing says so. The host arms a
// CornerWatch with the screen's corners; every text route then reports, once
// per string, the strings whose ink a corner hides (or comes within `margin`
// px of hiding). It is a signal for whoever lays out the screen, never a
// clip and never a refusal: the pixels are drawn as they always were.
//
// Text only: a string is the one thing the engine knows is meant to be read.
// Shapes and sprites draw into corners on purpose (backdrops, full-bleed art).
//
// Host only (ENJIN2_CORNER_WATCH): on the device the ink scope is an empty
// object and nothing is checked. Define it to 0 or 1 to override.

#include <cstdint>
#include <string_view>

#ifndef ENJIN2_CORNER_WATCH
#  if defined(ESP_PLATFORM)
#    define ENJIN2_CORNER_WATCH 0
#  else
#    define ENJIN2_CORNER_WATCH 1
#  endif
#endif

namespace enjin2 {

enum class Corner : uint8_t { None, TopLeft, TopRight, BottomRight, BottomLeft };

inline const char* cornerName(Corner c) {
    switch (c) {
        case Corner::TopLeft: return "top-left";
        case Corner::TopRight: return "top-right";
        case Corner::BottomRight: return "bottom-right";
        case Corner::BottomLeft: return "bottom-left";
        default: return "none";
    }
}

/// A screen's rounded corners, in the px of the canvas it shows.
struct RoundedCorners {
    int16_t width = 0;   ///< the canvas the screen shows
    int16_t height = 0;
    int16_t radius = 0;  ///< each corner's radius
    int16_t margin = 0;  ///< px inside the curve that count as cut too

    /// The corner that cuts pixel (x, y), or None when the screen shows it
    /// clear of the margin. A pixel off the canvas is None: it is cut by an
    /// edge, not a corner. A pixel counts as shown when its centre lies
    /// within `radius - margin` of its corner's arc centre (integer maths on
    /// doubled coordinates: centres sit on half pixels).
    constexpr Corner cornerAt(int x, int y) const {
        if (radius <= 0 || x < 0 || y < 0 || x >= width || y >= height) return Corner::None;
        const bool left = x < radius, right = x >= width - radius;
        const bool top = y < radius, bottom = y >= height - radius;
        if (!(left || right) || !(top || bottom)) return Corner::None;
        // Doubled: the arc centre sits at (2r, 2r) from its corner, a pixel's
        // centre at (2x + 1, 2y + 1).
        const int dx = left ? 2 * radius - (2 * x + 1) : (2 * x + 1) - 2 * (width - radius);
        const int dy = top ? 2 * radius - (2 * y + 1) : (2 * y + 1) - 2 * (height - radius);
        const int keep = 2 * (radius - margin);
        if (dx <= 0 || dy <= 0 || dx * dx + dy * dy <= keep * keep) return Corner::None;
        return top ? (left ? Corner::TopLeft : Corner::TopRight)
                   : (left ? Corner::BottomLeft : Corner::BottomRight);
    }
};

/// One string a corner cuts: reported once, when the string is drawn.
struct CornerCut {
    std::string_view text;  ///< valid during the call only
    int16_t x = 0, y = 0;   ///< the string's ink box on the canvas
    int16_t w = 0, h = 0;
    Corner corner = Corner::None;  ///< the first corner its ink met
    uint16_t pixels = 0;           ///< ink pixels cut, every corner
    bool script = false;           ///< drawn by a script (gfx.text*), not the host's own code
};

/// What the host arms to hear the cuts. Only text drawn on a canvas the size
/// of `corners` is watched: an offscreen canvas of another size is not the
/// screen.
class CornerWatch {
public:
    explicit CornerWatch(RoundedCorners corners) : corners_(corners) {}
    virtual ~CornerWatch() = default;
    virtual void cut(const CornerCut& cut) = 0;
    const RoundedCorners& corners() const { return corners_; }

    /// The watch every text route reports to; nullptr leaves text unwatched.
    static CornerWatch* armed() { return armed_; }
    static void arm(CornerWatch* watch) { armed_ = watch; }

    /// Held by the script bindings while they draw a script's text, so a cut
    /// says whose layout it is (CornerCut::script).
    struct ScriptText {
        ScriptText() { ++scriptDepth_; }
        ~ScriptText() { --scriptDepth_; }
        ScriptText(const ScriptText&) = delete;
        ScriptText& operator=(const ScriptText&) = delete;
    };
    static bool inScript() { return scriptDepth_ > 0; }

private:
    RoundedCorners corners_;
    static inline CornerWatch* armed_ = nullptr;
    static inline int scriptDepth_ = 0;
};

#if ENJIN2_CORNER_WATCH

/// One string's ink, from its first pixel to the end of the scope. A text
/// route opens one per string and passes it every ink pixel it draws; when
/// the scope ends, a string a corner cut is reported to the armed watch.
class TextInk {
public:
    TextInk(int canvasW, int canvasH, std::string_view text) : text_(text) {
        CornerWatch* w = CornerWatch::armed();
        if (w && w->corners().width == canvasW && w->corners().height == canvasH) {
            watch_ = w;
            script_ = CornerWatch::inScript();
        }
    }
    TextInk(const TextInk&) = delete;
    TextInk& operator=(const TextInk&) = delete;

    void pixel(int x, int y) {
        if (!watch_) return;
        if (x < x0_) x0_ = x;
        if (y < y0_) y0_ = y;
        if (x > x1_) x1_ = x;
        if (y > y1_) y1_ = y;
        const Corner c = watch_->corners().cornerAt(x, y);
        if (c == Corner::None) return;
        if (corner_ == Corner::None) corner_ = c;
        if (pixels_ < UINT16_MAX) ++pixels_;
    }

    ~TextInk() {
        if (!watch_ || pixels_ == 0) return;
        CornerCut cut;
        cut.text = text_;
        cut.x = static_cast<int16_t>(x0_);
        cut.y = static_cast<int16_t>(y0_);
        cut.w = static_cast<int16_t>(x1_ - x0_ + 1);
        cut.h = static_cast<int16_t>(y1_ - y0_ + 1);
        cut.corner = corner_;
        cut.pixels = pixels_;
        cut.script = script_;
        watch_->cut(cut);
    }

private:
    CornerWatch* watch_ = nullptr;
    std::string_view text_;
    int x0_ = INT16_MAX, y0_ = INT16_MAX, x1_ = INT16_MIN, y1_ = INT16_MIN;
    Corner corner_ = Corner::None;
    uint16_t pixels_ = 0;
    bool script_ = false;
};

#else

class TextInk {
public:
    TextInk(int, int, std::string_view) {}
    TextInk(const TextInk&) = delete;
    TextInk& operator=(const TextInk&) = delete;
    void pixel(int, int) {}
};

#endif

}  // namespace enjin2

#endif  // ENJIN2_GRAPHICS_CORNER_WATCH_HPP
