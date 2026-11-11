#include "bindings_internal.hpp"
#include "../../include/enjin2/graphics/palette.hpp"
#include <algorithm>
#include <cmath>

namespace enjin2 {

// Replaces null-check preamble repeated across drawing functions
#define REQUIRE_CANVAS(b, L) \
    LuaBindings* b = LuaBindings::getBindings(L); \
    if (!(b) || !(b)->currentCanvas) return 0; \
    (b)->checkDrawLayer(L)

//==============================================================================
// Line-width strokes (Tomodachi #255): gfx.setLineWidth feeds gfx.line and the
// "line" modes of rectangle/circle/triangle. Width 1 keeps the plain 1 px
// primitives; wider strokes are built from clipped fills.
//==============================================================================

namespace {

void fillSpan(LuaCanvas& c, int x, int y, int w, int h, uint8_t color) {
    if (w <= 0 || h <= 0) return;
    c.fillRect(static_cast<int16_t>(x), static_cast<int16_t>(y),
               static_cast<uint16_t>(w), static_cast<uint16_t>(h), color);
}

// A width x width square pen stamped centred on each Bresenham step: the
// stamps as they were, for a pen wider than StrokeRows holds.
void stampLine(LuaCanvas& c, int x1, int y1, int x2, int y2, uint8_t color, int width) {
    const int off = (width - 1) / 2;
    const int dx = std::abs(x2 - x1), dy = std::abs(y2 - y1);
    const int sx = x1 < x2 ? 1 : -1, sy = y1 < y2 ? 1 : -1;
    int err = dx - dy;
    int x = x1, y = y1;
    while (true) {
        fillSpan(c, x - off, y - off, width, width, color);
        if (x == x2 && y == y2) break;
        const int e2 = 2 * err;
        if (e2 > -dy) { err -= dy; x += sx; }
        if (e2 < dx) { err += dx; y += sy; }
    }
}

// The stamps' union, a row at a time (Tomodachi #488). The Bresenham points
// are cut into runs of one y (x from a to b); a run's stamps cover rows
// y - off .. y - off + width - 1 from a - off to b - off + width - 1. While y
// only moves one way, a step at a time, and each point is next to the last,
// a row's covering runs are the last `width` runs and their union is one
// span. Each row is painted once, when no later run can reach it: the same
// pixels as the stamps, with no row painted width times over.
class StrokeRows {
public:
    static constexpr int kMaxWidth = 32;

    StrokeRows(LuaCanvas& c, uint8_t color, int width, int sy)
        : c_(c), color_(color), width_(width), off_((width - 1) / 2), sy_(sy) {}

    // The next point of the walk.
    void point(int x, int y) {
        if (open_ && y == y_) {
            a_ = x < a_ ? x : a_;
            b_ = x > b_ ? x : b_;
            return;
        }
        if (open_) run();
        open_ = true;
        y_ = y;
        a_ = b_ = x;
    }

    // The walk is over: the last run, then the rows only the last runs
    // still cover, in walk order (the k-th after the last run's row is
    // covered by the last width - k runs).
    void finish() {
        if (!open_) return;
        run();
        for (int k = 1; k < width_; ++k) {
            while (count_ > width_ - k) {
                head_ = (head_ + 1) % kMaxWidth;
                --count_;
            }
            emit(sy_ > 0 ? y_ - off_ + k : y_ - off_ + width_ - 1 - k);
        }
    }

private:
    struct Run { int a, b; };

    void run() {
        const int i = (head_ + count_) % kMaxWidth;
        if (count_ == width_) {
            head_ = (head_ + 1) % kMaxWidth;
        } else {
            ++count_;
        }
        runs_[i] = Run{a_, b_};
        // The row this run finishes: no run after it reaches it.
        emit(sy_ > 0 ? y_ - off_ : y_ - off_ + width_ - 1);
    }

    void emit(int row) {
        int lo = runs_[head_].a, hi = runs_[head_].b;
        for (int k = 1; k < count_; ++k) {
            const Run& r = runs_[(head_ + k) % kMaxWidth];
            lo = r.a < lo ? r.a : lo;
            hi = r.b > hi ? r.b : hi;
        }
        fillSpan(c_, lo - off_, row, hi - lo + width_, 1, color_);
    }

    LuaCanvas& c_;
    uint8_t color_;
    int width_, off_, sy_;
    Run runs_[kMaxWidth];
    int head_ = 0, count_ = 0;
    bool open_ = false;
    int y_ = 0, a_ = 0, b_ = 0;
};

// Every Bresenham point of (x1, y1)-(x2, y2) but the first, to `rows`.
void walkAfterFirst(StrokeRows& rows, int x1, int y1, int x2, int y2) {
    const int dx = std::abs(x2 - x1), dy = std::abs(y2 - y1);
    const int sx = x1 < x2 ? 1 : -1, sy = y1 < y2 ? 1 : -1;
    int err = dx - dy;
    int x = x1, y = y1;
    while (!(x == x2 && y == y2)) {
        const int e2 = 2 * err;
        if (e2 > -dy) { err -= dy; x += sx; }
        if (e2 < dx) { err += dx; y += sy; }
        rows.point(x, y);
    }
}

// A width x width square pen stamped centred on each Bresenham step, painted
// as the stamps' union a row at a time (StrokeRows).
void strokeLine(LuaCanvas& c, int x1, int y1, int x2, int y2, uint8_t color,
                int width) {
    if (width <= 1) {
        c.drawLine(static_cast<int16_t>(x1), static_cast<int16_t>(y1),
                   static_cast<int16_t>(x2), static_cast<int16_t>(y2), color);
        return;
    }
    const int off = (width - 1) / 2;
    if (y1 == y2) {
        fillSpan(c, std::min(x1, x2) - off, y1 - off, std::abs(x2 - x1) + width, width, color);
        return;
    }
    if (x1 == x2) {
        fillSpan(c, x1 - off, std::min(y1, y2) - off, width, std::abs(y2 - y1) + width, color);
        return;
    }
    if (width > StrokeRows::kMaxWidth) {
        stampLine(c, x1, y1, x2, y2, color, width);
        return;
    }
    StrokeRows rows(c, color, width, y1 < y2 ? 1 : -1);
    rows.point(x1, y1);
    walkAfterFirst(rows, x1, y1, x2, y2);
    rows.finish();
}

// Outline rectangle whose border grows inward, so the outer edge stays put:
// the square-cornered solid case of the computed border stroke.
void strokeRect(LuaCanvas& c, int16_t x, int16_t y, uint16_t w, uint16_t h,
                uint8_t color, int width) {
    if (width <= 1) {
        c.drawRect(x, y, w, h, color);
        return;
    }
    BorderStyle style;
    style.color = color;
    style.thickness = static_cast<uint8_t>(width);
    c.strokeBorder(x, y, w, h, style);
}

int circleHalfWidth(int radius, int dy) {
    return static_cast<int>(
        std::sqrt(static_cast<float>(radius * radius - dy * dy)) + 0.5f);
}

// Outline circle as a ring that grows inward from the radius.
void strokeCircle(LuaCanvas& c, int cx, int cy, int radius, uint8_t color, int width) {
    if (width <= 1) {
        c.drawCircle(static_cast<int16_t>(cx), static_cast<int16_t>(cy),
                     static_cast<uint16_t>(radius), color);
        return;
    }
    if (width >= radius) {
        c.fillCircle(static_cast<int16_t>(cx), static_cast<int16_t>(cy),
                     static_cast<uint16_t>(radius), color);
        return;
    }
    const int inner = radius - width;
    for (int dy = -radius; dy <= radius; ++dy) {
        const int ady = std::abs(dy);
        const int outerHw = circleHalfWidth(radius, ady);
        if (ady > inner) {
            fillSpan(c, cx - outerHw, cy + dy, 2 * outerHw + 1, 1, color);
            continue;
        }
        const int innerHw = circleHalfWidth(inner, ady);
        fillSpan(c, cx - outerHw, cy + dy, outerHw - innerHw, 1, color);
        fillSpan(c, cx + innerHw + 1, cy + dy, outerHw - innerHw, 1, color);
    }
}

void strokeTriangle(LuaCanvas& c, int x1, int y1, int x2, int y2, int x3, int y3,
                    uint8_t color, int width) {
    if (width <= 1) {
        c.drawTriangle(static_cast<int16_t>(x1), static_cast<int16_t>(y1),
                       static_cast<int16_t>(x2), static_cast<int16_t>(y2),
                       static_cast<int16_t>(x3), static_cast<int16_t>(y3), color);
        return;
    }
    strokeLine(c, x1, y1, x2, y2, color, width);
    strokeLine(c, x2, y2, x3, y3, color, width);
    strokeLine(c, x3, y3, x1, y1, color, width);
}

}  // namespace

//==============================================================================
// Lua Drawing Functions
//==============================================================================

int LuaBindings::lua_getWidth(lua_State* L) {
    LuaBindings* bindings = getBindings(L);
    if (!bindings || !bindings->currentCanvas) {
        lua_pushinteger(L, 0);
        return 1;
    }
    lua_pushinteger(L, bindings->currentCanvas->getWidth());
    return 1;
}

int LuaBindings::lua_getHeight(lua_State* L) {
    LuaBindings* bindings = getBindings(L);
    if (!bindings || !bindings->currentCanvas) {
        lua_pushinteger(L, 0);
        return 1;
    }
    lua_pushinteger(L, bindings->currentCanvas->getHeight());
    return 1;
}

int LuaBindings::lua_clear(lua_State* L) {
    REQUIRE_CANVAS(bindings, L);

    uint8_t color = bindings->currentColor;
    if (lua_gettop(L) >= 1 && lua_isnumber(L, 1)) {
        color = lua::checkCanvasColour(L, 1, indexedColours(bindings->currentCanvas));
    }

    bindings->currentCanvas->clear(color);
    return 0;
}

int LuaBindings::lua_setColor(lua_State* L) {
    LuaBindings* bindings = getBindings(L);
    if (!bindings) {
        return 0;
    }

    if (lua_gettop(L) >= 1 && lua_isnumber(L, 1)) {
        bindings->currentColor = lua::checkCanvasColour(L, 1, indexedColours(bindings->currentCanvas));
    }

    return 0;
}

int LuaBindings::lua_getColor(lua_State* L) {
    LuaBindings* bindings = getBindings(L);
    if (!bindings) {
        lua_pushinteger(L, 0);
        return 1;
    }

    lua_pushinteger(L, bindings->currentColor);
    return 1;
}

int LuaBindings::lua_setLineWidth(lua_State* L) {
    LuaBindings* bindings = getBindings(L);
    if (!bindings) {
        return 0;
    }

    if (lua_gettop(L) >= 1 && lua_isnumber(L, 1)) {
        // 1..255: the range the border stroke's thickness holds.
        const lua_Integer w = lua_tointeger(L, 1);
        bindings->lineWidth = static_cast<uint16_t>(w < 1 ? 1 : (w > 255 ? 255 : w));
    }

    return 0;
}

int LuaBindings::lua_getLineWidth(lua_State* L) {
    LuaBindings* bindings = getBindings(L);
    if (!bindings) {
        lua_pushinteger(L, 1);
        return 1;
    }

    lua_pushinteger(L, bindings->lineWidth);
    return 1;
}

int LuaBindings::lua_point(lua_State* L) {
    REQUIRE_CANVAS(bindings, L);

    if (lua_gettop(L) >= 2 && lua_isnumber(L, 1) && lua_isnumber(L, 2)) {
        int16_t x = static_cast<int16_t>(lround(lua_tonumber(L, 1)));
        int16_t y = static_cast<int16_t>(lround(lua_tonumber(L, 2)));

        bindings->currentCanvas->setPixel(x, y, bindings->currentColor);
    }

    return 0;
}

int LuaBindings::lua_line(lua_State* L) {
    REQUIRE_CANVAS(bindings, L);

    if (lua_gettop(L) >= 4) {
        int16_t x1 = static_cast<int16_t>(lround(lua_tonumber(L, 1)));
        int16_t y1 = static_cast<int16_t>(lround(lua_tonumber(L, 2)));
        int16_t x2 = static_cast<int16_t>(lround(lua_tonumber(L, 3)));
        int16_t y2 = static_cast<int16_t>(lround(lua_tonumber(L, 4)));

        strokeLine(*bindings->currentCanvas, x1, y1, x2, y2,
                   bindings->currentColor, bindings->lineWidth);
    }

    return 0;
}

// polyline(points)  — gfx.line from each point to the next, points a flat
// {x1, y1, x2, y2, ...} table (Tomodachi #488). One call for a whole stroke:
// while y runs one way the stroke's rows are painted once each (StrokeRows),
// otherwise segment by segment; either way the pixels of the gfx.line calls.
int LuaBindings::lua_polyline(lua_State* L) {
    REQUIRE_CANVAS(bindings, L);
    luaL_checktype(L, 1, LUA_TTABLE);
    const int n = static_cast<int>(lua_rawlen(L, 1)) / 2;
    if (n < 1) return 0;
    // Each number read and rounded once (lround is a software double on the
    // S3): the first kPoints points into a stack buffer.
    auto coord = [L](int i) {
        lua_rawgeti(L, 1, i);
        const int v = static_cast<int16_t>(lround(luaL_checknumber(L, -1)));
        lua_pop(L, 1);
        return v;
    };
    constexpr int kPoints = 128;
    int16_t xy[2 * kPoints];
    const int held = n < kPoints ? n : kPoints;
    for (int i = 0; i < 2 * held; ++i) xy[i] = static_cast<int16_t>(coord(i + 1));
    LuaCanvas& c = *bindings->currentCanvas;
    const uint8_t color = bindings->currentColor;
    const int width = bindings->lineWidth;
    if (n == 1) {
        strokeLine(c, xy[0], xy[1], xy[0], xy[1], color, width);
        return 0;
    }
    // One way in y, and not flat: one walk.
    int dir = 0;
    bool oneWay = n == held && width > 1 && width <= StrokeRows::kMaxWidth;
    for (int i = 1; oneWay && i < n; ++i) {
        const int d = xy[2 * i + 1] - xy[2 * i - 1];
        if (d == 0) continue;
        if (dir == 0) dir = d > 0 ? 1 : -1;
        else if ((d > 0 ? 1 : -1) != dir) oneWay = false;
    }
    if (oneWay && dir != 0) {
        StrokeRows rows(c, color, width, dir);
        rows.point(xy[0], xy[1]);
        for (int i = 1; i < n; ++i)
            walkAfterFirst(rows, xy[2 * i - 2], xy[2 * i - 1], xy[2 * i], xy[2 * i + 1]);
        rows.finish();
        return 0;
    }
    int x = xy[0], y = xy[1];
    for (int i = 1; i < n; ++i) {
        const int x2 = i < held ? xy[2 * i] : coord(2 * i + 1);
        const int y2 = i < held ? xy[2 * i + 1] : coord(2 * i + 2);
        strokeLine(c, x, y, x2, y2, color, width);
        x = x2;
        y = y2;
    }
    return 0;
}

int LuaBindings::lua_rectangle(lua_State* L) {
    REQUIRE_CANVAS(bindings, L);

    if (lua_gettop(L) >= 4) {
        const char* mode = kShapeModeNames[static_cast<int>(ShapeMode::Fill)];
        if (lua_gettop(L) >= 5 && lua_type(L, 1) == LUA_TSTRING) {
            mode = lua_tostring(L, 1);
        }

        int startIdx = (lua_type(L, 1) == LUA_TSTRING) ? 2 : 1;
        int16_t x = static_cast<int16_t>(lround(lua_tonumber(L, startIdx)));
        int16_t y = static_cast<int16_t>(lround(lua_tonumber(L, startIdx + 1)));
        uint16_t width = static_cast<uint16_t>(lua_tointeger(L, startIdx + 2));
        uint16_t height = static_cast<uint16_t>(lua_tointeger(L, startIdx + 3));

        if (isFillMode(mode)) {
            bindings->currentCanvas->fillRect(x, y, width, height, bindings->currentColor);
        } else {
            strokeRect(*bindings->currentCanvas, x, y, width, height,
                       bindings->currentColor, bindings->lineWidth);
        }
    }

    return 0;
}

int LuaBindings::lua_circle(lua_State* L) {
    REQUIRE_CANVAS(bindings, L);

    if (lua_gettop(L) >= 3) {
        const char* mode = kShapeModeNames[static_cast<int>(ShapeMode::Fill)];
        if (lua_gettop(L) >= 4 && lua_type(L, 1) == LUA_TSTRING) {
            mode = lua_tostring(L, 1);
        }

        int startIdx = (lua_type(L, 1) == LUA_TSTRING) ? 2 : 1;
        int16_t x = static_cast<int16_t>(lround(lua_tonumber(L, startIdx)));
        int16_t y = static_cast<int16_t>(lround(lua_tonumber(L, startIdx + 1)));
        uint16_t radius = static_cast<uint16_t>(lua_tointeger(L, startIdx + 2));

        if (isFillMode(mode)) {
            bindings->currentCanvas->fillCircle(x, y, radius, bindings->currentColor);
        } else {
            strokeCircle(*bindings->currentCanvas, x, y, radius,
                         bindings->currentColor, bindings->lineWidth);
        }
    }

    return 0;
}

// gfx.fillEllipse(cx, cy, rx, ry) — filled ellipse in the current colour
// (Tomodachi #43). The launcher's optional ground shadow: set a dark on-ramp
// colour with gfx.setColor first, then draw the blob under an actor.
int LuaBindings::lua_fillEllipse(lua_State* L) {
    REQUIRE_CANVAS(bindings, L);

    if (lua_gettop(L) >= 4) {
        int16_t cx = static_cast<int16_t>(lround(lua_tonumber(L, 1)));
        int16_t cy = static_cast<int16_t>(lround(lua_tonumber(L, 2)));
        int16_t rx = static_cast<int16_t>(lua_tointeger(L, 3));
        int16_t ry = static_cast<int16_t>(lua_tointeger(L, 4));
        bindings->currentCanvas->fillEllipse(cx, cy, rx, ry, bindings->currentColor);
    }

    return 0;
}

int LuaBindings::lua_triangle(lua_State* L) {
    REQUIRE_CANVAS(bindings, L);

    if (lua_gettop(L) >= 6) {
        const char* mode = kShapeModeNames[static_cast<int>(ShapeMode::Fill)];
        if (lua_gettop(L) >= 7 && lua_type(L, 1) == LUA_TSTRING) {
            mode = lua_tostring(L, 1);
        }

        int startIdx = (lua_type(L, 1) == LUA_TSTRING) ? 2 : 1;
        int16_t x1 = static_cast<int16_t>(lround(lua_tonumber(L, startIdx)));
        int16_t y1 = static_cast<int16_t>(lround(lua_tonumber(L, startIdx + 1)));
        int16_t x2 = static_cast<int16_t>(lround(lua_tonumber(L, startIdx + 2)));
        int16_t y2 = static_cast<int16_t>(lround(lua_tonumber(L, startIdx + 3)));
        int16_t x3 = static_cast<int16_t>(lround(lua_tonumber(L, startIdx + 4)));
        int16_t y3 = static_cast<int16_t>(lround(lua_tonumber(L, startIdx + 5)));

        if (isFillMode(mode)) {
            bindings->currentCanvas->fillTriangle(x1, y1, x2, y2, x3, y3, bindings->currentColor);
        } else {
            strokeTriangle(*bindings->currentCanvas, x1, y1, x2, y2, x3, y3,
                           bindings->currentColor, bindings->lineWidth);
        }
    }

    return 0;
}

int LuaBindings::lua_setPixel(lua_State* L) {
    REQUIRE_CANVAS(bindings, L);

    if (lua_gettop(L) >= 3) {
        int16_t x = static_cast<int16_t>(lua_tointeger(L, 1));
        int16_t y = static_cast<int16_t>(lua_tointeger(L, 2));
        uint8_t color = lua::checkCanvasColour(L, 3, indexedColours(bindings->currentCanvas));

        bindings->currentCanvas->setPixel(x, y, color);
    }

    return 0;
}

int LuaBindings::lua_getPixel(lua_State* L) {
    LuaBindings* bindings = getBindings(L);
    if (!bindings || !bindings->currentCanvas) {
        lua_pushinteger(L, 0);
        return 1;
    }

    if (lua_gettop(L) >= 2) {
        int16_t x = static_cast<int16_t>(lua_tointeger(L, 1));
        int16_t y = static_cast<int16_t>(lua_tointeger(L, 2));

        uint8_t color = bindings->currentCanvas->getPixel(x, y);
        lua_pushinteger(L, color);
        return 1;
    }

    lua_pushinteger(L, 0);
    return 1;
}

int LuaBindings::lua_print(lua_State* L) {
    int n = lua_gettop(L);
    for (int i = 1; i <= n; ++i) {
        const char* s = lua_tostring(L, i);
        if (s) {
            printf("%s", s);
        } else {
            printf("(%s)", lua_typename(L, lua_type(L, i)));
        }
        if (i < n) printf("\t");
    }
    printf("\n");
    return 0;
}

//==============================================================================
// High-Performance Drawing Functions
//==============================================================================

int LuaBindings::lua_fastFillRect(lua_State* L) {
    REQUIRE_CANVAS(bindings, L);

    int x = static_cast<int>(luaL_checknumber(L, 1));
    int y = static_cast<int>(luaL_checknumber(L, 2));
    int w = static_cast<int>(luaL_checknumber(L, 3));
    int h = static_cast<int>(luaL_checknumber(L, 4));
    uint8_t color = lua::optCanvasColour(L, 5, bindings->currentColor, indexedColours(bindings->currentCanvas));

    // Fast bulk fill - bypass individual pixel calls
    int x2 = x + w;
    int y2 = y + h;

    // Clamp to canvas bounds
    x = std::max(x, 0);
    y = std::max(y, 0);
    x2 = std::min(x2, static_cast<int>(bindings->currentCanvas->getWidth()));
    y2 = std::min(y2, static_cast<int>(bindings->currentCanvas->getHeight()));

    // Bulk fill
    for (int py = y; py < y2; py++) {
        for (int px = x; px < x2; px++) {
            bindings->currentCanvas->setPixel(px, py, color);
        }
    }

    return 0;
}

int LuaBindings::lua_fastDrawLine(lua_State* L) {
    REQUIRE_CANVAS(bindings, L);

    int x1 = static_cast<int>(luaL_checknumber(L, 1));
    int y1 = static_cast<int>(luaL_checknumber(L, 2));
    int x2 = static_cast<int>(luaL_checknumber(L, 3));
    int y2 = static_cast<int>(luaL_checknumber(L, 4));
    uint8_t color = lua::optCanvasColour(L, 5, bindings->currentColor, indexedColours(bindings->currentCanvas));

    // Fast Bresenham line algorithm
    int dx = abs(x2 - x1);
    int dy = abs(y2 - y1);
    int sx = x1 < x2 ? 1 : -1;
    int sy = y1 < y2 ? 1 : -1;
    int err = dx - dy;

    int x = x1, y = y1;
    int width = static_cast<int>(bindings->currentCanvas->getWidth());
    int height = static_cast<int>(bindings->currentCanvas->getHeight());

    while (true) {
        if (x >= 0 && x < width && y >= 0 && y < height) {
            bindings->currentCanvas->setPixel(x, y, color);
        }
        if (x == x2 && y == y2) break;
        int e2 = 2 * err;
        if (e2 > -dy) { err -= dy; x += sx; }
        if (e2 < dx) { err += dx; y += sy; }
    }

    return 0;
}

//==============================================================================
// Palette Functions: read-only; the palette is the system's (Tomodachi #417)
//==============================================================================

int LuaBindings::lua_getPaletteColor(lua_State* L) {
    RGB c = enjin2::g_palette.getColor(lua::checkPaletteIndex(L, 1));
    lua_pushinteger(L, c.r);
    lua_pushinteger(L, c.g);
    lua_pushinteger(L, c.b);
    return 3;
}

} // namespace enjin2
