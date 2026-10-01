#pragma once

#include "../core/types.hpp"
#include "border.hpp"
#include "canvas.hpp"
#include <algorithm>
#include <cmath>
#include <cstdint>
#include <cstdlib>
namespace enjin2
{

/**
 * @file primitives.hpp
 * @brief Drawing primitives for geometric shapes
 *
 * Provides optimized algorithms for drawing lines, circles,
 * triangles, ellipses, and polygons on any canvas type.
 */

/**
 * @brief Drawing primitives for geometric shapes
 * @tparam TPixel Pixel type (e.g., Pixel4, uint8_t)
 */
template<typename TPixel>
class Primitives {
public:
    /**
     * @brief Draw a line using Bresenham's algorithm
     * @param canvas Target canvas
     * @param x0 Starting X coordinate
     * @param y0 Starting Y coordinate
     * @param x1 Ending X coordinate
     * @param y1 Ending Y coordinate
     * @param color Line color
     */
    static void drawLine(ICanvas<TPixel>& canvas, int16_t x0, int16_t y0,
                        int16_t x1, int16_t y1, TPixel color) {
        int32_t dx = abs(x1 - x0);                   // 32-bit: a span may exceed 32767 px
        int32_t dy = abs(y1 - y0);
        int16_t sx = x0 < x1 ? 1 : -1;
        int16_t sy = y0 < y1 ? 1 : -1;
        int32_t err = dx - dy;
        
        int16_t x = x0, y = y0;
        
        while (true) {
            canvas.setPixel(x, y, color);
            
            if (x == x1 && y == y1) break;
            
            int32_t e2 = 2 * err;
            if (e2 > -dy) {
                err -= dy;
                x += sx;
            }
            if (e2 < dx) {
                err += dx;
                y += sy;
            }
        }
    }
    
    /**
     * @brief Draw a quadratic Bezier curve (Zingl's integer rasteriser)
     * @param canvas Target canvas
     * @param x0 Start X coordinate
     * @param y0 Start Y coordinate
     * @param cx Control point X coordinate
     * @param cy Control point Y coordinate
     * @param x1 End X coordinate
     * @param y1 End Y coordinate
     * @param color Curve color
     *
     * Zingl, "A Rasterizing Algorithm for Drawing Curves" (2012), §2: split
     * where x or y turns back, so each piece steps in one fixed direction, and
     * walk each piece with integer error terms. The pixel rule (Tomodachi
     * #226): a thin 8-connected path, one pixel per step, with no gaps, no
     * doubled "L" corners, no side-by-side steps, and both ends drawn.
     *
     * - A control point on the chord (between the ends) is the straight
     *   segment and draws exactly @ref drawLine's pixels.
     * - When only one axis turns back (a curve bulging past both its ends),
     *   both halves are walked on the curve's own error terms from their true
     *   ends to the turn, rather than re-fitting each half through a rounded
     *   turn point as Zingl does. That keeps every pixel the nearest one to
     *   the curve. A walk that would leave its quadrant is caught by a dry
     *   run first, and falls back to Zingl's split.
     * - Where the slope crosses 45° the curve can pass through a pixel
     *   corner; the corner pixel of the resulting "L" is dropped.
     * - Split points are exact fractions rounded half away from zero from an
     *   end, so mirrored curves come out mirrored.
     *
     * Integer only, no allocation; 64-bit error terms, so any int16_t curve
     * fits. A curve that folds back within a pixel or two of itself (a
     * hairpin) can't be a thin path, and may double up at the fold.
     */
    static void drawQuadBezier(ICanvas<TPixel>& canvas, int16_t x0, int16_t y0,
                               int16_t cx, int16_t cy, int16_t x1, int16_t y1, TPixel color) {
        const int64_t cross = int64_t(cx - x0) * (y1 - y0) - int64_t(cy - y0) * (x1 - x0);
        if (cross == 0 && cx >= std::min(x0, x1) && cx <= std::max(x0, x1) &&
            cy >= std::min(y0, y1) && cy <= std::max(y0, y1)) {
            drawLine(canvas, x0, y0, x1, y1, color);
            return;
        }

        int64_t ax = x0, ay = y0, bx = cx, by = cy, ex = x1, ey = y1;
        const bool turnsX = (ax - bx) * (ex - bx) > 0;
        const bool turnsY = (ay - by) * (ey - by) > 0;
        if (turnsX != turnsY && drawQuadHalves(canvas, ax, ay, bx, by, ex, ey, turnsY, color)) {
            return;
        }

        // Zingl's plotQuadBezier: cut at each turn, re-fit the rest through it.
        if (turnsX) {                                // x turns back: cut at the vertical tangent
            if (turnsY) {                            // y turns back too: cut the nearer one first
                const int64_t d = ax - 2 * bx + ex;
                if (absI64((ay - 2 * by + ey) * (ax - bx)) > absI64(ay - by) * absI64(d)) {
                    std::swap(ax, ex);
                    std::swap(ay, ey);
                }
            }
            const int64_t d = ax - 2 * bx + ex;      // cut at t = (ax - bx) / d
            const int64_t t = ax - bx, u = ex - bx;
            const int64_t px = ax + roundDiv(-t * t, d);
            const int64_t py = atTurn(ay, by, ey, t, u, d);
            drawQuadSegment(canvas, ax, ay, px, ay + roundDiv((by - ay) * t, d), px, py, color);
            by = ey + roundDiv((by - ey) * u, d);
            ax = bx = px;
            ay = py;
        }
        if ((ay - by) * (ey - by) > 0) {             // y turns back: cut at the horizontal tangent
            const int64_t d = ay - 2 * by + ey;      // cut at t = (ay - by) / d
            const int64_t t = ay - by, u = ey - by;
            const int64_t px = atTurn(ax, bx, ex, t, u, d);
            const int64_t py = ay + roundDiv(-t * t, d);
            drawQuadSegment(canvas, ax, ay, ax + roundDiv((bx - ax) * t, d), py, px, py, color);
            bx = ex + roundDiv((bx - ex) * u, d);
            ax = px;
            ay = by = py;
        }
        drawQuadSegment(canvas, ax, ay, bx, by, ex, ey, color);
    }

    /**
     * @brief Draw a line drooping straight down the screen by @p sag pixels
     * @param canvas Target canvas
     * @param x0 Start X coordinate
     * @param y0 Start Y coordinate
     * @param x1 End X coordinate
     * @param y1 End Y coordinate
     * @param sag Droop at the middle, in pixels (0 = taut; negative bows up)
     * @param color Line color
     *
     * A Tape Span (Tomodachi #226): the quadratic Bezier whose control point
     * is the chord's exact midpoint pushed down by 2·sag, so the middle of the
     * curve hangs exactly @p sag below the chord. With the control point over
     * the midpoint, x moves evenly along the curve, so it is the graph
     * y = chord + 4·sag·k·(w − k) / w² over the columns k = 0..w, and it is
     * drawn column by column on exact integer terms: odd-width Spans droop
     * symmetrically too.
     *
     * - The steps are @ref drawLine's decisions on the exact curve, so Sag 0
     *   is exactly drawLine and more Sag only ever moves pixels down.
     * - A curve that turns back (its lowest point lies between the ends) is
     *   walked from both ends toward the turn, so a symmetric Span comes out
     *   symmetric.
     * - A vertical chord has no room to droop: it draws drawLine at any Sag.
     * - On a steep chord, a Sag of more than a quarter of its height hangs
     *   below the lower end and comes back up to it. On Spans 8 px wide or
     *   more that is still a thin path; on narrower ones a lot of Sag folds
     *   into a hairpin, which can't be (and can bottom out a pixel short).
     *
     * Integer only, no allocation: 64-bit terms, evaluated once per column.
     */
    static void drawSaggedLine(ICanvas<TPixel>& canvas, int16_t x0, int16_t y0,
                               int16_t x1, int16_t y1, int16_t sag, TPixel color) {
        if (sag == 0 || x0 == x1) {
            drawLine(canvas, x0, y0, x1, y1, color);
            return;
        }
        const SagSpan span{x0, y0, x1 > x0 ? 1 : -1, std::abs(int32_t(x1) - x0),
                           int32_t(y1) - y0, sag};
        const int32_t w = span.w, dy = span.dy;
        if (4 * std::abs(int32_t(sag)) <= std::abs(dy)) {
            // No turn: one walk, from the steep end to the flat one.
            Stroke stroke(canvas, color);
            const int32_t down = dy > 0 ? 1 : -1;
            if ((sag > 0) == (dy > 0)) {
                const int32_t row = walkSag(span, stroke, 0, 0, 1, down, w - 1);
                span.fillColumn(stroke, w, row, dy, down);
            } else {
                const int32_t row = walkSag(span, stroke, w, dy, -1, -down, 1);
                span.fillColumn(stroke, 0, row, 0, -down);
            }
            stroke.finish();
            return;
        }
        // The curve turns back at column n / d; both halves walk toward it.
        const int32_t toward = sag > 0 ? 1 : -1;
        const int64_t n = int64_t(w) * (dy + 4 * sag) * toward, d = 8 * int64_t(sag) * toward;
        Stroke half1(canvas, color), half2(canvas, color);
        if (2 * n % (2 * d) == d) {
            // The turn is on the edge between columns c and c + 1 (a level
            // Span of odd width): each half ends at its side of the edge.
            const int32_t c = static_cast<int32_t>((2 * n - d) / (2 * d));
            walkSag(span, half1, 0, 0, 1, toward, c);
            walkSag(span, half2, w, dy, -1, toward, c + 1);
            Stroke::meet(half1, half2);
            return;
        }
        // Column t holds the turn. Both halves enter it; the one entering
        // nearer the chord draws the column down to the other's entry.
        const int32_t t = static_cast<int32_t>((2 * n + d) / (2 * d));
        const int32_t in1 = walkSag(span, half1, 0, 0, 1, toward, t - 1);
        const int32_t in2 = walkSag(span, half2, w, dy, -1, toward, t + 1);
        const bool firstRuns = toward * in1 < toward * in2;
        span.fillColumn(firstRuns ? half1 : half2, t, firstRuns ? in1 : in2,
                        firstRuns ? in2 : in1, toward);
        Stroke::meet(half1, half2);
    }

    /**
     * @brief Draw rectangle outline
     * @param canvas Target canvas
     * @param rect Rectangle bounds
     * @param color Outline color
     */
    static void drawRect(ICanvas<TPixel>& canvas, const Rect& rect, TPixel color) {
        int16_t x1 = rect.x;
        int16_t y1 = rect.y;
        int16_t x2 = rect.x + rect.width - 1;
        int16_t y2 = rect.y + rect.height - 1;
        
        // Top and bottom
        drawLine(canvas, x1, y1, x2, y1, color);
        drawLine(canvas, x1, y2, x2, y2, color);
        
        // Left and right
        drawLine(canvas, x1, y1, x1, y2, color);
        drawLine(canvas, x2, y1, x2, y2, color);
    }
    
    /**
     * @brief Fill a rectangle
     * @param canvas Target canvas
     * @param rect Rectangle bounds
     * @param color Fill color
     */
    static void fillRect(ICanvas<TPixel>& canvas, const Rect& rect, TPixel color) {
        canvas.fill(rect, color);
    }

    /**
     * @brief Draw circle outline using midpoint circle algorithm
     * @param canvas Target canvas
     * @param cx Center X coordinate
     * @param cy Center Y coordinate
     * @param radius Circle radius
     * @param color Outline color
     *
     * The exact pre-migration Canvas8::drawCircle octant walk (unwn #161
     * restore): Eisei's UI shipped against these pixels, and the bench's
     * circle guard pins this body against re-divergence.
     */
    static void drawCircle(ICanvas<TPixel>& canvas, int16_t cx, int16_t cy,
                          int16_t radius, TPixel color) {
        int16_t x = radius;
        int16_t y = 0;
        int16_t radiusError = 1 - x;

        while (x >= y) {
            canvas.setPixel(cx + x, cy + y, color);
            canvas.setPixel(cx + y, cy + x, color);
            canvas.setPixel(cx - y, cy + x, color);
            canvas.setPixel(cx - x, cy + y, color);
            canvas.setPixel(cx - x, cy - y, color);
            canvas.setPixel(cx - y, cy - x, color);
            canvas.setPixel(cx + y, cy - x, color);
            canvas.setPixel(cx + x, cy - y, color);

            y++;
            if (radiusError < 0) {
                radiusError += 2 * y + 1;
            } else {
                x--;
                radiusError += 2 * (y - x + 1);
            }
        }
    }

    /**
     * @brief Fill circle using midpoint circle algorithm
     * @param canvas Target canvas
     * @param cx Center X coordinate
     * @param cy Center Y coordinate
     * @param radius Circle radius
     * @param color Fill color
     *
     * The exact pre-migration Canvas8::fillCircle midpoint-octant fill (unwn
     * #161 restore). It over-fills relative to the Euclidean disc (e.g. r=2
     * lights (±2,±1)) — that fatter disc is what Eisei shipped and what the
     * bench's circle guard pins. Keep in sync with drawCircle: Eisei draws
     * disc and outline at the same radius and they must agree.
     */
    static void fillCircle(ICanvas<TPixel>& canvas, int16_t cx, int16_t cy,
                          int16_t radius, TPixel color) {
        int16_t x = radius;
        int16_t y = 0;
        int16_t radiusError = 1 - x;

        while (x >= y) {
            // Draw horizontal lines for each octant
            for (int16_t i = cx - x; i <= cx + x; i++) {
                canvas.setPixel(i, cy + y, color);
                canvas.setPixel(i, cy - y, color);
            }
            for (int16_t i = cx - y; i <= cx + y; i++) {
                canvas.setPixel(i, cy + x, color);
                canvas.setPixel(i, cy - x, color);
            }

            y++;
            if (radiusError < 0) {
                radiusError += 2 * y + 1;
            } else {
                x--;
                radiusError += 2 * (y - x + 1);
            }
        }
    }
    
    /**
     * @brief Draw a rounded-rectangle outline
     * @param canvas Target canvas
     * @param rect Rectangle bounds
     * @param radius Corner radius (clamped to half the smaller extent)
     * @param color Outline color
     *
     * The old corner-arc annulus (`(r-1)^2 <= i^2+j^2 <= r^2`) is deleted: this
     * is now a 1px `strokeBorder` (Tomodachi #39), so there is one border
     * rasteriser, not two. A zero radius still degrades to the square outline
     * byte-for-byte; the rounded corners diverge from the Canvas8 annulus by a
     * bounded amount, waived in the parity bench (`geom.drawRoundRect`, r > 0).
     */
    static void drawRoundRect(ICanvas<TPixel>& canvas, const Rect& rect, int16_t radius, TPixel color) {
        BorderStyle style;
        style.color = static_cast<uint8_t>(color);
        style.thickness = 1;
        style.radius = static_cast<uint8_t>(radius < 0 ? 0 : radius);
        style.kind = BorderKind::Solid;
        strokeBorder(canvas, rect, style);
    }

    /**
     * @brief Fill a rounded rectangle
     * @param canvas Target canvas
     * @param rect Rectangle bounds
     * @param radius Corner radius (no clamp — Canvas8 semantics, unwn #168)
     * @param color Fill color
     *
     * A full-height center band, two side bands, and quarter-disc corners
     * from the distance test `i^2+j^2 <= r^2` — the Canvas8 original,
     * byte-for-byte (sweep adjudication, unwn #168). No radius clamp, no
     * zero-radius degrade; negative extents still paint the corner discs,
     * exactly as shipped.
     */
    static void fillRoundRect(ICanvas<TPixel>& canvas, const Rect& rect, int16_t radius, TPixel color) {
        const int16_t x = rect.x;
        const int16_t y = rect.y;
        const int16_t w = static_cast<int16_t>(rect.width);
        const int16_t h = static_cast<int16_t>(rect.height);
        // Bands (Canvas8::fillRect loop shape: a non-positive extent is a no-op).
        fillBand(canvas, x + radius, y, w - 2 * radius, h, color);              // center
        fillBand(canvas, x, y + radius, radius, h - 2 * radius, color);         // left edge
        fillBand(canvas, x + w - radius, y + radius, radius, h - 2 * radius, color); // right edge
        // Corner discs.
        for (int16_t i = 0; i <= radius; i++) {
            for (int16_t j = 0; j <= radius; j++) {
                if (i * i + j * j <= radius * radius) {
                    canvas.setPixel(x + radius - i, y + radius - j, color);                 // top-left
                    canvas.setPixel(x + w - radius - 1 + i, y + radius - j, color);         // top-right
                    canvas.setPixel(x + radius - i, y + h - radius - 1 + j, color);         // bottom-left
                    canvas.setPixel(x + w - radius - 1 + i, y + h - radius - 1 + j, color); // bottom-right
                }
            }
        }
    }

    /**
     * @brief Draw triangle outline
     * @param canvas Target canvas
     * @param x0 First vertex X
     * @param y0 First vertex Y
     * @param x1 Second vertex X
     * @param y1 Second vertex Y
     * @param x2 Third vertex X
     * @param y2 Third vertex Y
     * @param color Outline color
     */
    static void drawTriangle(ICanvas<TPixel>& canvas, int16_t x0, int16_t y0,
                            int16_t x1, int16_t y1, int16_t x2, int16_t y2, TPixel color) {
        drawLine(canvas, x0, y0, x1, y1, color);
        drawLine(canvas, x1, y1, x2, y2, color);
        drawLine(canvas, x2, y2, x0, y0, color);
    }
    
    /**
     * @brief Fill triangle using scanline algorithm
     * @param canvas Target canvas
     * @param x0 First vertex X
     * @param y0 First vertex Y
     * @param x1 Second vertex X
     * @param y1 Second vertex Y
     * @param x2 Third vertex X
     * @param y2 Third vertex Y
     * @param color Fill color
     */
    static void fillTriangle(ICanvas<TPixel>& canvas, int16_t x0, int16_t y0,
                            int16_t x1, int16_t y1, int16_t x2, int16_t y2, TPixel color) {
        // Canvas8's single scanline walk, byte-for-byte (sweep adjudication,
        // unwn #168). One loop over the full y extent with a mid-vertex edge
        // switch — its degenerate collapse (collinear vertices give a point,
        // not a span) is the shipped behavior the earlier two-loop version
        // diverged from.
        if (y0 > y1) { std::swap(x0, x1); std::swap(y0, y1); }
        if (y1 > y2) { std::swap(x1, x2); std::swap(y1, y2); }
        if (y0 > y1) { std::swap(x0, x1); std::swap(y0, y1); }

        for (int16_t y = y0; y <= y2; y++) {
            int16_t xa;
            if (y <= y1) {
                xa = (y1 - y0 != 0) ? x0 + (x1 - x0) * (y - y0) / (y1 - y0) : x0;
            } else {
                xa = (y2 - y1 != 0) ? x1 + (x2 - x1) * (y - y1) / (y2 - y1) : x1;
            }
            int16_t xb = (y2 - y0 != 0) ? x0 + (x2 - x0) * (y - y0) / (y2 - y0) : x0;
            if (xa > xb) std::swap(xa, xb);
            for (int16_t x = xa; x <= xb; x++) {
                canvas.setPixel(x, y, color);
            }
        }
    }
    
    /**
     * @brief Draw ellipse using midpoint algorithm
     * @param canvas Target canvas
     * @param cx Center X coordinate
     * @param cy Center Y coordinate
     * @param rx Horizontal radius
     * @param ry Vertical radius
     * @param color Outline color
     */
    static void drawEllipse(ICanvas<TPixel>& canvas, int16_t cx, int16_t cy,
                           int16_t rx, int16_t ry, TPixel color) {
        int32_t rx2 = rx * rx;
        int32_t ry2 = ry * ry;
        int32_t two_rx2 = 2 * rx2;
        int32_t two_ry2 = 2 * ry2;
        
        int16_t x = 0;
        int16_t y = ry;
        int32_t px = 0;
        int32_t py = two_rx2 * y;
        
        // Region 1
        int32_t p = ry2 - (rx2 * ry) + (rx2 / 4);
        while (px < py) {
            canvas.setPixel(cx + x, cy + y, color);
            canvas.setPixel(cx - x, cy + y, color);
            canvas.setPixel(cx + x, cy - y, color);
            canvas.setPixel(cx - x, cy - y, color);
            
            x++;
            px += two_ry2;
            if (p < 0) {
                p += ry2 + px;
            } else {
                y--;
                py -= two_rx2;
                p += ry2 + px - py;
            }
        }
        
        // Region 2
        p = ry2 * (x + 0.5) * (x + 0.5) + rx2 * (y - 1) * (y - 1) - rx2 * ry2;
        while (y > 0) {
            canvas.setPixel(cx + x, cy + y, color);
            canvas.setPixel(cx - x, cy + y, color);
            canvas.setPixel(cx + x, cy - y, color);
            canvas.setPixel(cx - x, cy - y, color);
            
            y--;
            py -= two_rx2;
            if (p > 0) {
                p += rx2 - py;
            } else {
                x++;
                px += two_ry2;
                p += rx2 - py + px;
            }
        }
    }
    
    /**
     * @brief Draw arc segment
     * @param canvas Target canvas
     * @param cx Center X coordinate
     * @param cy Center Y coordinate
     * @param radius Arc radius
     * @param start_angle Start angle in radians
     * @param end_angle End angle in radians
     * @param color Outline color
     */
    static void drawArc(ICanvas<TPixel>& canvas, int16_t cx, int16_t cy,
                       int16_t radius, float start_angle, float end_angle, TPixel color) {
        // Convert angles to 0-2π range
        while (start_angle < 0) start_angle += 2 * M_PI;
        while (end_angle < 0) end_angle += 2 * M_PI;
        while (start_angle >= 2 * M_PI) start_angle -= 2 * M_PI;
        while (end_angle >= 2 * M_PI) end_angle -= 2 * M_PI;
        
        float step = 1.0f / radius; // Adaptive step size
        for (float angle = start_angle; 
             (end_angle > start_angle) ? (angle <= end_angle) : (angle <= end_angle + 2 * M_PI); 
             angle += step) {
            int16_t x = cx + static_cast<int16_t>(radius * cos(angle));
            int16_t y = cy + static_cast<int16_t>(radius * sin(angle));
            canvas.setPixel(x, y, color);
        }
    }
    
    /**
     * @brief Draw polygon outline
     * @param canvas Target canvas
     * @param vertices Array of polygon vertices
     * @param vertex_count Number of vertices
     * @param color Outline color
     */
    static void drawPolygon(ICanvas<TPixel>& canvas, const Point* vertices,
                           size_t vertex_count, TPixel color) {
        if (vertex_count < 3) return;
        
        for (size_t i = 0; i < vertex_count; ++i) {
            size_t next = (i + 1) % vertex_count;
            drawLine(canvas, vertices[i].x, vertices[i].y,
                    vertices[next].x, vertices[next].y, color);
        }
    }
private:
    static int64_t absI64(int64_t v) { return v < 0 ? -v : v; }

    /// @brief n / d rounded to nearest, halves away from zero (mirror-symmetric).
    static int64_t roundDiv(int64_t n, int64_t d) {
        const int64_t q = (2 * absI64(n) + absI64(d)) / (2 * absI64(d));
        return (n < 0) != (d < 0) ? -q : q;
    }

    /**
     * @brief One coordinate of a curve at its turn in the other coordinate
     * @param a Start value of this coordinate
     * @param b Control value of this coordinate
     * @param e End value of this coordinate
     * @param t,u,d The other coordinate turns at t / d, with u = d − t
     *
     * The Bezier a·(1−τ)² + 2b·τ(1−τ) + e·τ² at τ = t / d, rounded:
     * a + (2·t·u·(b − a) + t²·(e − a)) / d².
     */
    static int64_t atTurn(int64_t a, int64_t b, int64_t e, int64_t t, int64_t u, int64_t d) {
        return a + roundDiv(2 * t * u * (b - a) + t * t * (e - a), d * d);
    }

    /**
     * @brief Pixel sink for one curve walk that drops doubled "L" corners
     *
     * Holds the last two pixels back. When the next one touches the older,
     * the newer is dropped: the path stays 8-connected without it. That is
     * the corner of an "L" (the exact curve can pass through a pixel corner
     * where its slope crosses 45°, and the walk then takes an x step and a
     * y step one after the other), or a one-pixel spur where a curve dips
     * less than a pixel past an end and comes back.
     */
    struct Stroke {
        ICanvas<TPixel>& canvas;
        TPixel color;
        int64_t ax = 0, ay = 0, bx = 0, by = 0;
        int held = 0;

        Stroke(ICanvas<TPixel>& c, TPixel col) : canvas(c), color(col) {}

        void plot(int64_t x, int64_t y) {
            if (held == 2 && x == bx && y == by) return;
            if (held == 2 && x == ax && y == ay) { held = 1; return; }
            if (held == 2 && absI64(x - ax) <= 1 && absI64(y - ay) <= 1) { bx = x; by = y; return; }
            if (held == 2) { set(ax, ay); ax = bx; ay = by; bx = x; by = y; return; }
            if (held == 1) { if (x == ax && y == ay) return; bx = x; by = y; held = 2; return; }
            ax = x; ay = y; held = 1;
        }

        /// @brief Bresenham from (x0,y0) to (x1,y1): @ref drawLine's steps.
        void line(int64_t x0, int64_t y0, int64_t x1, int64_t y1) {
            const int64_t dx = absI64(x1 - x0), dy = absI64(y1 - y0);
            const int64_t sx = x0 < x1 ? 1 : -1, sy = y0 < y1 ? 1 : -1;
            int64_t err = dx - dy;
            while (true) {
                plot(x0, y0);
                if (x0 == x1 && y0 == y1) break;
                const int64_t e2 = 2 * err;
                if (e2 > -dy) { err -= dy; x0 += sx; }
                if (e2 < dx)  { err += dx; y0 += sy; }
            }
        }

        void finish() {
            if (held >= 1) set(ax, ay);
            if (held == 2) set(bx, by);
        }

        /// @brief Finish where the path runs on into (x,y), drawn by another
        /// stroke: (x,y) still decides which corner to drop, but isn't drawn.
        void finishBefore(int64_t x, int64_t y) {
            plot(x, y);
            if (held == 2) set(ax, ay);
        }

        /// @brief Finish two strokes walked toward each other, whose ends are
        /// distinct neighbouring pixels: each drops its "L" corner against the
        /// other's end, so the join is treated the same from either side.
        static void meet(Stroke& one, Stroke& two) {
            int64_t x1 = 0, y1 = 0, x2 = 0, y2 = 0;
            const bool has1 = one.newest(x1, y1), has2 = two.newest(x2, y2);
            if (has2) one.finishBefore(x2, y2); else one.finish();
            if (has1) two.finishBefore(x1, y1); else two.finish();
        }

        bool newest(int64_t& x, int64_t& y) const {
            if (held == 0) return false;
            x = held == 2 ? bx : ax;
            y = held == 2 ? by : ay;
            return true;
        }

        void set(int64_t x, int64_t y) {
            if (x < INT16_MIN || x > INT16_MAX || y < INT16_MIN || y > INT16_MAX) return;
            canvas.setPixel(static_cast<int16_t>(x), static_cast<int16_t>(y), color);
        }
    };

    /**
     * @brief A sagged Span as a graph over its columns (@ref drawSaggedLine)
     *
     * Columns k = 0..w run from (x0,y0) toward the far end, @p dy rows below.
     * `height` is the curve's depth below y0 at half-column h (h = 2k at a
     * column's centre, odd at the edge between two columns) in units of
     * 1 / (2w²) row, which makes it exact: 2w²·y(h/2) = w·dy·h + 2·sag·h·(2w − h).
     */
    struct SagSpan {
        int32_t x0, y0, sx, w, dy, sag;

        int64_t height(int64_t h) const { return h * (int64_t(w) * dy + 2 * int64_t(sag) * (2 * w - h)); }
        int64_t row() const { return 2 * int64_t(w) * w; }
        int64_t x(int32_t col) const { return x0 + int64_t(sx) * col; }
        int64_t y(int32_t row) const { return y0 + int64_t(row); }

        /// @brief Plot column @p col from @p from to @p to, stepping @p dir.
        void fillColumn(Stroke& stroke, int32_t col, int32_t from, int32_t to, int32_t dir) const {
            for (; dir * (to - from) > 0; from += dir) stroke.plot(x(col), y(from));
            stroke.plot(x(col), y(to));
        }
    };

    /**
     * @brief Walk a sagged Span column by column, on @ref drawLine's decisions
     * @param span The sagged Span being walked
     * @param stroke Pixel sink
     * @param col Start column
     * @param row Start row (relative to the Span's y0)
     * @param dc Column step, +1 or -1
     * @param dir Row step: the way the curve moves along the walk
     * @param last Last column walked; the walk ends by stepping out of it
     * @return Row at which the walk enters the column after @p last
     *
     * Stay in the column (step the row) while the curve at its far edge is a
     * whole row or more past the current one; leave it otherwise, stepping
     * the row too if the curve at the next column's centre is more than half
     * a row past. On a straight chord that is drawLine exactly. Walks run
     * from a steep end toward a flat end or the turn, where a run of pixels
     * leaves its column diagonally, except where the slope crosses 45°: the
     * Stroke drops that "L" corner.
     */
    static int32_t walkSag(const SagSpan& span, Stroke& stroke, int32_t col, int32_t row,
                           int32_t dc, int32_t dir, int32_t last) {
        const int64_t unit = span.row();
        int64_t at = dir * unit * row;               // the row, oriented, in height() units
        for (; col != last + dc; col += dc) {
            const int64_t edge = dir * span.height(2 * int64_t(col) + dc);
            while (true) {
                stroke.plot(span.x(col), span.y(row));
                if (edge - at < unit) break;
                row += dir;
                at += unit;
            }
            if (dir * span.height(2 * int64_t(col) + 2 * dc) - at > unit / 2) {
                row += dir;
                at += unit;
            }
        }
        return row;
    }

    /**
     * @brief Walk one quadratic Bezier piece whose gradient keeps its sign
     *
     * Zingl's plotQuadBezierSeg with 64-bit error terms. It starts from the
     * end farther from the control point and finishes with a straight line
     * once the error terms can no longer tell the steps apart (or at once,
     * for a straight piece).
     */
    static void drawQuadSegment(ICanvas<TPixel>& canvas, int64_t x0, int64_t y0,
                                int64_t x1, int64_t y1, int64_t x2, int64_t y2, TPixel color) {
        Stroke stroke(canvas, color);
        int64_t sx = x2 - x1, sy = y2 - y1;
        int64_t xx = x0 - x1, yy = y0 - y1;
        int64_t cur = xx * sy - yy * sx;             // curvature
        if (sx * sx + sy * sy > xx * xx + yy * yy) { // begin with the longer part
            x2 = x0; x0 = sx + x1; y2 = y0; y0 = sy + y1; cur = -cur;
        }
        if (cur != 0) {
            xx += sx; sx = x0 < x2 ? 1 : -1; xx *= sx;
            yy += sy; sy = y0 < y2 ? 1 : -1; yy *= sy;
            int64_t xy = 2 * xx * yy;                 // 2nd-degree differences
            xx *= xx; yy *= yy;
            if (cur * sx * sy < 0) { xx = -xx; yy = -yy; xy = -xy; cur = -cur; }
            int64_t dx = 4 * sy * cur * (x1 - x0) + xx - xy; // 1st-degree differences
            int64_t dy = 4 * sx * cur * (y0 - y1) + yy - xy;
            xx += xx; yy += yy;
            int64_t err = dx + dy + xy;
            do {
                stroke.plot(x0, y0);
                if (x0 == x2 && y0 == y2) { stroke.finish(); return; }
                const bool stepY = 2 * err < dx;
                if (2 * err > dy) { x0 += sx; dx -= xy; dy += yy; err += dy; }
                if (stepY)        { y0 += sy; dy -= xy; dx += xx; err += dx; }
            } while (dy < 0 && dx > 0);              // gradient turned: finish straight
        }
        stroke.line(x0, y0, x2, y2);
        stroke.finish();
    }

    static bool touches(int64_t ax, int64_t ay, int64_t bx, int64_t by) {
        return absI64(ax - bx) <= 1 && absI64(ay - by) <= 1;
    }

    /**
     * @brief Draw a curve with one turn as two walks that meet at the turn
     * @return false (nothing drawn) if either walk would leave its quadrant
     *
     * The turn's column (y turns back) or row (x turns back) is rounded from
     * the exact fraction; each half walks from its end until it steps onto
     * that line. The two stop pixels are joined by whichever one touches both
     * halves (the usual case is that they're the same pixel), else by a line.
     */
    static bool drawQuadHalves(ICanvas<TPixel>& canvas, int64_t ax, int64_t ay, int64_t bx,
                               int64_t by, int64_t ex, int64_t ey, bool turnsY, TPixel color) {
        int64_t turn;
        if (turnsY) {                                // a column: x at the horizontal tangent
            const int64_t d = ay - 2 * by + ey;
            turn = atTurn(ax, bx, ex, ay - by, ey - by, d);
        } else {                                     // a row: y at the vertical tangent
            const int64_t d = ax - 2 * bx + ex;
            turn = atTurn(ay, by, ey, ax - bx, ex - bx, d);
        }
        HalfWalk one{ax, ay, ax, ay}, two{ex, ey, ex, ey};
        if (!walkQuadHalf(nullptr, one, bx, by, ex, ey, turn, turnsY) ||
            !walkQuadHalf(nullptr, two, bx, by, ax, ay, turn, turnsY)) {
            return false;
        }
        Stroke half1(canvas, color), half2(canvas, color);
        one = {ax, ay, ax, ay};
        two = {ex, ey, ex, ey};
        walkQuadHalf(&half1, one, bx, by, ex, ey, turn, turnsY);
        walkQuadHalf(&half2, two, bx, by, ax, ay, turn, turnsY);
        const bool walked = !(one.nextX == ax && one.nextY == ay) && !(two.nextX == ex && two.nextY == ey);
        if (walked && touches(two.lastX, two.lastY, one.nextX, one.nextY)) {
            half1.plot(one.nextX, one.nextY);
            half1.finish();
            half2.finishBefore(one.nextX, one.nextY);
        } else if (walked && touches(one.lastX, one.lastY, two.nextX, two.nextY)) {
            half2.plot(two.nextX, two.nextY);
            half2.finish();
            half1.finishBefore(two.nextX, two.nextY);
        } else {
            half1.line(one.nextX, one.nextY, two.nextX, two.nextY);
            half1.finish();
            half2.finishBefore(two.nextX, two.nextY);
        }
        return true;
    }

    /// @brief Where a half-curve walk got to: the first pixel it didn't draw
    /// (it starts at the curve's end) and the last one it did.
    struct HalfWalk {
        int64_t nextX, nextY, lastX, lastY;
    };

    /**
     * @brief Walk a curve from one end toward its control point, up to a turn
     * @param stroke Pixel sink, or nullptr for a dry run that only checks
     * @param walk In: nextX/nextY = the end to start from. Out: the first
     *        pixel not drawn, and the last pixel drawn before @p turn
     *        (lastX/lastY unchanged if none)
     * @param x1,y1 Control point
     * @param x2,y2 The curve's far end
     * @param turn Column (@p turnsY) or row where the curve turns back
     * @param turnsY y turns back, so @p turn is a column; else a row
     * @return true if the walk reached @p turn, or turned within a pixel of it
     *         (the turn lies between), without leaving its quadrant
     *
     * `drawQuadSegment`'s error terms, but for the whole curve (x2,y2 is
     * the far end), so the walk tracks the true curve right up to the turn.
     */
    static bool walkQuadHalf(Stroke* stroke, HalfWalk& walk,
                             int64_t x1, int64_t y1, int64_t x2, int64_t y2,
                             int64_t turn, bool turnsY) {
        int64_t& x0 = walk.nextX;
        int64_t& y0 = walk.nextY;
        const int64_t sx = x1 != x0 ? (x1 > x0 ? 1 : -1) : (x2 > x0 ? 1 : -1);
        const int64_t sy = y1 != y0 ? (y1 > y0 ? 1 : -1) : (y2 > y0 ? 1 : -1);
        int64_t cur = (x0 - x1) * (y2 - y1) - (y0 - y1) * (x2 - x1);
        if (cur == 0) return false;
        int64_t xx = (x0 - 2 * x1 + x2) * sx, yy = (y0 - 2 * y1 + y2) * sy;
        int64_t xy = 2 * xx * yy;
        xx *= xx; yy *= yy;
        if (cur * sx * sy < 0) { xx = -xx; yy = -yy; xy = -xy; cur = -cur; }
        int64_t dx = 4 * sy * cur * (x1 - x0) + xx - xy;
        int64_t dy = 4 * sx * cur * (y0 - y1) + yy - xy;
        xx += xx; yy += yy;
        int64_t err = dx + dy + xy;
        while (dy < 0 && dx > 0) {
            if ((turnsY ? x0 : y0) == turn) return true;
            if (stroke) stroke->plot(x0, y0);
            walk.lastX = x0; walk.lastY = y0;
            const bool stepY = 2 * err < dx;
            if (2 * err > dy) { x0 += sx; dx -= xy; dy += yy; err += dy; }
            if (stepY)        { y0 += sy; dy -= xy; dx += xx; err += dx; }
        }
        return absI64((turnsY ? x0 : y0) - turn) <= 1; // turned just short of it
    }

    /**
     * @brief Fill a signed-extent rectangle (rounded-rect band helper)
     * @param canvas Target canvas
     * @param x Top-left X coordinate
     * @param y Top-left Y coordinate
     * @param w Width in pixels (non-positive = no-op)
     * @param h Height in pixels (non-positive = no-op)
     * @param color Fill color
     *
     * Canvas8::fillRect's exact loop shape, kept separate from @ref fillRect
     * because Rect's unsigned extents can't express the negative widths
     * @ref fillRoundRect's band arithmetic produces.
     */
    static void fillBand(ICanvas<TPixel>& canvas, int16_t x, int16_t y,
                         int16_t w, int16_t h, TPixel color) {
        for (int16_t py = y; py < y + h && py < static_cast<int16_t>(canvas.getHeight()); py++) {
            for (int16_t px = x; px < x + w && px < static_cast<int16_t>(canvas.getWidth()); px++) {
                if (px >= 0 && py >= 0) {
                    canvas.setPixel(px, py, color);
                }
            }
        }
    }
};

/// @brief Drawing primitives for 4-bit pixel type
using Primitives4 = Primitives<Pixel4>;
/// @brief Drawing primitives for 8-bit pixel type
using Primitives8 = Primitives<uint8_t>;

} // namespace enjin2
