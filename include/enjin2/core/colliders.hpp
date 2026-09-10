/**
 * @file colliders.hpp
 * @brief Circle body vs static/kinematic collider set physics (ADR-0003 §4)
 *
 * One dynamic circle (`BodyState`) against one source-agnostic `ColliderSet`
 * (segments, circles, AABBs, and moving flipper segments) drives both the
 * pinball and tilt-maze slices at 160×160 logical, 30 fps, with fixed-dt
 * substeps.
 *
 * The step is the standard 2D combo the wayfinder #54 prototype verified:
 * **swept conservative-advancement TOI (≤4 iters/substep) AND a static
 * depenetration+impulse pass, both every substep.** Swept alone tunnels on
 * resting / grazing / moved-into (flipper) contacts; depenetration alone misses
 * fast entry. Together + restitution = `max(body, collider)` there is no
 * tunnelling at any substep count (N=1..4).
 *
 * Contacts are **buffered** — a list the applet polls (`kind / point / normal /
 * relSpeed`); there is deliberately no C++→Lua callback registry (anti-callback
 * stance, §4). Flippers are kinematic segments whose angle is spring-driven and
 * whose angular velocity becomes a surface velocity `ω × r` that imparts the
 * flip impulse (no rigid-body rotation).
 *
 * Header-only, allocation-light, float math, no Lua dependency — usable from a
 * C++ applet and from the Lua bindings identically (parity by construction).
 */
#pragma once

#include <cmath>
#include <cstdint>
#include <vector>

namespace enjin2 {

/// Collider semantic tags. The field is a full `uint8_t` (256 authored kinds),
/// these are the reserved spatial ones; applets may poll any kind.
namespace ColliderKinds {
constexpr uint8_t Wall    = 0;  ///< Solid wall (pinball rails, maze tiles).
constexpr uint8_t Bouncy  = 1;  ///< Bumper: score + SFX on contact (from relSpeed).
constexpr uint8_t Hazard  = 2;  ///< Drain / spikes.
constexpr uint8_t Goal    = 3;  ///< Maze exit / target.
constexpr uint8_t Flipper = 4;  ///< Kinematic moving segment.
} // namespace ColliderKinds

/// The sole dynamic entity: one circle body. Maps 1:1 to the wayfinder #54 struct.
struct BodyState {
    float x{0};           ///< Centre X (px).
    float y{0};           ///< Centre Y (px).
    float vx{0};          ///< Velocity X (px/s).
    float vy{0};          ///< Velocity Y (px/s).
    float radius{3};      ///< Ball radius (px).
    float restitution{0.5f}; ///< Bounciness [0..1] (can exceed 1 for bumpers).
    float drag{0};        ///< Velocity damping /s (rolling friction).
};

/// Static line segment (pinball walls; freeform auth e.g. Tiled object layer).
struct SegCollider {
    float ax{0}, ay{0}, bx{0}, by{0};
    float restitution{0.5f};
    uint8_t kind{ColliderKinds::Wall};
};

/// Static circle (bumpers, posts, goals).
struct CircleCollider {
    float cx{0}, cy{0}, r{0};
    float restitution{0.5f};
    uint8_t kind{ColliderKinds::Wall};
};

/// Static axis-aligned box (maze walls, derived from SOLID tiles).
struct AabbCollider {
    float minx{0}, miny{0}, maxx{0}, maxy{0};
    float restitution{0.1f};
    uint8_t kind{ColliderKinds::Wall};
};

/**
 * @brief Moving segment = flipper.
 *
 * A segment rotating about `pivot`, its `angle` animated by a damped spring
 * toward `target` (rest/active); `angVel` (rad/s) is read back each frame and
 * turned into a surface velocity at the contact point, which is what imparts
 * the flip impulse. The standard kinematic-mover pinball flipper.
 */
struct Flipper {
    float pivotX{0}, pivotY{0};
    float length{30};
    float restAngle{0};    ///< Radians.
    float activeAngle{0};  ///< Radians.
    float angle{0};        ///< Current angle (radians).
    float angVel{0};       ///< Current omega (rad/s), read-only to physics.
    float restitution{0.2f};
    uint8_t kind{ColliderKinds::Flipper};
    float target{0};       ///< Spring target angle.
    float springVel{0};    ///< Spring derivative state.

    /// Default spring tuning (ADR-0002 Spring — slightly under-damped for a kick).
    static constexpr float kStiffness = 1400.0f;
    static constexpr float kDamping   = 2.0f * std::sqrt(kStiffness) * 0.55f;
};

/// The whole static/kinematic world for one slice.
struct ColliderSet {
    std::vector<SegCollider>    segments;
    std::vector<CircleCollider> circles;
    std::vector<AabbCollider>   aabbs;
    std::vector<Flipper>        flippers;

    /// Discard every collider (keeps capacity).
    void clear() {
        segments.clear();
        circles.clear();
        aabbs.clear();
        flippers.clear();
    }
    bool empty() const {
        return segments.empty() && circles.empty() && aabbs.empty() && flippers.empty();
    }

    /// Append an AABB collider (min/max corners). The tile-derived shape:
    /// C_Tilemap::buildSolidRects() (ADR-0003 §3) feeds these from SOLID tiles.
    void addAabb(float minx, float miny, float maxx, float maxy,
                 float restitution, uint8_t kind) {
        aabbs.push_back({minx, miny, maxx, maxy, restitution, kind});
    }
};

/// One resolved hit this step. The step buffers these; the applet reads the
/// list each frame (polling) and reacts (e.g. Bouncy → score + SFX from relSpeed).
struct Contact {
    uint8_t kind{0};   ///< Collider semantic tag (ColliderKinds).
    float px{0};       ///< Contact point (px).
    float py{0};
    float nx{0};       ///< Contact normal (unit, points away from collider).
    float ny{0};
    float relSpeed{0}; ///< Approach speed along the normal (for SFX/score scaling).
};

/// Per-frame step health counters.
struct StepStats {
    int toiIters{0};   ///< TOI iterations resolved this frame (should stay low).
    float maxSpeed{0}; ///< Peak speed seen this frame.
};

/// A swept hit through one substep's remaining displacement.
struct SweepHit {
    float t{1};
    float nx{0}, ny{0};
    uint8_t kind{0};
    float restitution{0};
    const Flipper* flipper{nullptr};  ///< Non-null when the surface is a moving segment.
};

namespace detail {

/// Velocity damping (drag = fractional per second, clamped so velocity never
/// reverses sign).
inline void applyDrag(BodyState& body, float h) {
    const float factor = 1.0f - body.drag * h;
    if (factor < 0.0f) { body.vx = 0.0f; body.vy = 0.0f; return; }
    body.vx *= factor;
    body.vy *= factor;
}

/// Ray (o + d·t) vs sphere(c, r): earliest t∈[0,1] + contact normal, or false.
inline bool raySphere(float ox, float oy, float dx, float dy,
                      float cx, float cy, float r,
                      float& t, float& nx, float& ny) {
    const float a = dx * dx + dy * dy;
    if (a < 1e-12f) return false;
    const float mx = ox - cx, my = oy - cy;
    const float b = 2.0f * (mx * dx + my * dy);
    const float c = mx * mx + my * my - r * r;
    const float disc = b * b - 4.0f * a * c;
    if (disc < 0.0f) return false;
    const float tt = (-b - std::sqrt(disc)) / (2.0f * a);
    if (tt < 0.0f || tt > 1.0f) return false;
    const float hx = ox + dx * tt, hy = oy + dy * tt;
    float nnx = hx - cx, nny = hy - cy;
    const float l = std::hypot(nnx, nny);
    if (l < 1e-9f) { nnx = 1.0f; nny = 0.0f; } else { nnx /= l; nny /= l; }
    t = tt; nx = nnx; ny = nny;
    return true;
}

/// Ray vs a segment's flat face offset out by rad (the capsule side). false if
/// moving away, parallel, or contact lands outside the segment span.
inline bool rayOffsetLine(float ox, float oy, float dx, float dy,
                          float ax, float ay, float bx, float by, float rad,
                          float& t, float& nx, float& ny) {
    float ex = bx - ax, ey = by - ay;
    const float el = std::hypot(ex, ey);
    if (el < 1e-9f) return false;
    ex /= el; ey /= el;
    float nnx = -ey, nny = ex;  // segment normal
    if ((ox - ax) * nnx + (oy - ay) * nny < 0.0f) { nnx = -nnx; nny = -nny; }
    const float oax = ax + nnx * rad, oay = ay + nny * rad;
    const float denom = dx * nnx + dy * nny;
    if (denom >= -1e-9f) return false;  // moving away or parallel
    const float tt = ((oax - ox) * nnx + (oay - oy) * nny) / denom;
    if (tt < 0.0f || tt > 1.0f) return false;
    const float hx = ox + dx * tt, hy = oy + dy * tt;
    const float proj = (hx - oax) * ex + (hy - oay) * ey;
    if (proj < 0.0f || proj > el) return false;
    t = tt; nx = nnx; ny = nny;
    return true;
}

/// Sweep a circle against a static segment (capsule = flat line + 2 end spheres).
inline bool sweepSegment(float ox, float oy, float dx, float dy, float radius,
                         float ax, float ay, float bx, float by,
                         float& t, float& nx, float& ny) {
    float bt = 1.0f, bnx = 0.0f, bny = 0.0f;
    bool hit = false;
    float tt, nnx, nny;
    if (rayOffsetLine(ox, oy, dx, dy, ax, ay, bx, by, radius, tt, nnx, nny)) {
        hit = true; bt = tt; bnx = nnx; bny = nny;
    }
    if (raySphere(ox, oy, dx, dy, ax, ay, radius, tt, nnx, nny) && (!hit || tt < bt)) {
        hit = true; bt = tt; bnx = nnx; bny = nny;
    }
    if (raySphere(ox, oy, dx, dy, bx, by, radius, tt, nnx, nny) && (!hit || tt < bt)) {
        hit = true; bt = tt; bnx = nnx; bny = nny;
    }
    if (hit) { t = bt; nx = bnx; ny = bny; }
    return hit;
}

/// Sweep a circle against an AABB (Minkowski-expanded box, rounded corners).
inline bool sweepAabb(float ox, float oy, float dx, float dy, float radius,
                      float minx, float miny, float maxx, float maxy,
                      float& t, float& nx, float& ny) {
    const float exMinX = minx - radius, exMinY = miny - radius;
    const float exMaxX = maxx + radius, exMaxY = maxy + radius;
    float tmin = 0.0f, tmax = 1.0f;
    float nnx = 0.0f, nny = 0.0f;
    for (int axis = 0; axis < 2; ++axis) {
        const float o = axis == 0 ? ox : oy;
        const float d = axis == 0 ? dx : dy;
        const float lo = axis == 0 ? exMinX : exMinY;
        const float hi = axis == 0 ? exMaxX : exMaxY;
        if (std::fabs(d) < 1e-9f) {
            if (o < lo || o > hi) return false;
        } else {
            float t1 = (lo - o) / d, t2 = (hi - o) / d;
            float sign = -1.0f;
            if (t1 > t2) { float tmp = t1; t1 = t2; t2 = tmp; sign = 1.0f; }
            if (t1 > tmin) {
                tmin = t1;
                nnx = axis == 0 ? sign : 0.0f;
                nny = axis == 1 ? sign : 0.0f;
            }
            if (t2 < tmax) tmax = t2;
            if (tmin > tmax) return false;
        }
    }
    if (tmin <= 0.0f || tmin > 1.0f) return false;
    const float hx = ox + dx * tmin, hy = oy + dy * tmin;
    const bool inX = hx > minx && hx < maxx;
    const bool inY = hy > miny && hy < maxy;
    if (!inX && !inY) {  // corner region → round with the corner sphere
        const float cornerX = hx < minx ? minx : maxx;
        const float cornerY = hy < miny ? miny : maxy;
        return raySphere(ox, oy, dx, dy, cornerX, cornerY, radius, t, nx, ny);
    }
    t = tmin; nx = nnx; ny = nny;
    return true;
}

/// Closest point on segment (ax,ay)-(bx,by) to (px,py).
inline void closestOnSeg(float px, float py, float ax, float ay, float bx, float by,
                         float& cx, float& cy) {
    const float ex = bx - ax, ey = by - ay;
    const float len2 = ex * ex + ey * ey;
    float t = len2 < 1e-9f ? 0.0f : ((px - ax) * ex + (py - ay) * ey) / len2;
    t = t < 0.0f ? 0.0f : (t > 1.0f ? 1.0f : t);
    cx = ax + ex * t;
    cy = ay + ey * t;
}

/// Velocity of a flipper's surface at a world point (ω × r) — the flip impulse.
inline void flipperSurfaceVel(const Flipper& f, float px, float py, float& vx, float& vy) {
    const float rx = px - f.pivotX, ry = py - f.pivotY;
    vx = -f.angVel * ry;
    vy =  f.angVel * rx;
}

/// The flipper's segment at its current angle.
inline void flipperSeg(const Flipper& f, float& ax, float& ay, float& bx, float& by) {
    ax = f.pivotX; ay = f.pivotY;
    bx = f.pivotX + std::cos(f.angle) * f.length;
    by = f.pivotY + std::sin(f.angle) * f.length;
}

/// Push the ball out of penetration along the normal, then reflect its velocity
/// relative to the surface (flipper surfaceVel for movers, 0 for static). This
/// is the depenetration+impulse step swept alone lacks.
inline void resolveContact(BodyState& body, float nx, float ny, float pen, float e,
                           uint8_t kind, float sx, float sy,
                           std::vector<Contact>& contacts) {
    constexpr float SLOP = 0.005f;
    if (pen > SLOP) {
        body.x += nx * pen;
        body.y += ny * pen;
    }
    float rvx = body.vx - sx, rvy = body.vy - sy;
    const float vn = rvx * nx + rvy * ny;
    if (vn < 0.0f) {
        rvx -= (1.0f + e) * vn * nx;
        rvy -= (1.0f + e) * vn * ny;
        body.vx = rvx + sx;
        body.vy = rvy + sy;
        contacts.push_back({kind, body.x, body.y, nx, ny, -vn});
    }
}

/// Static depenetration pass over the whole set at its current pose (flippers
/// leave surface velocity). Runs every substep alongside the swept loop.
inline void depenetrate(BodyState& body, ColliderSet& set,
                        std::vector<Contact>& contacts) {
    const float r = body.radius;
    for (const auto& cc : set.circles) {
        const float dx = body.x - cc.cx, dy = body.y - cc.cy;
        const float d = std::hypot(dx, dy);
        const float sum = cc.r + r;
        if (d < sum) {
            const float nx = d > 1e-6f ? dx / d : 1.0f;
            const float ny = d > 1e-6f ? dy / d : 0.0f;
            const float e = body.restitution > cc.restitution ? body.restitution : cc.restitution;
            resolveContact(body, nx, ny, sum - d, e, cc.kind, 0.0f, 0.0f, contacts);
        }
    }
    for (const auto& s : set.segments) {
        float cx, cy;
        closestOnSeg(body.x, body.y, s.ax, s.ay, s.bx, s.by, cx, cy);
        const float dx = body.x - cx, dy = body.y - cy;
        const float d = std::hypot(dx, dy);
        if (d < r) {
            const float nx = d > 1e-6f ? dx / d : 1.0f;
            const float ny = d > 1e-6f ? dy / d : 0.0f;
            const float e = body.restitution > s.restitution ? body.restitution : s.restitution;
            resolveContact(body, nx, ny, r - d, e, s.kind, 0.0f, 0.0f, contacts);
        }
    }
    for (const auto& a : set.aabbs) {
        const float cx = a.minx > body.x ? a.minx : (body.x < a.maxx ? body.x : a.maxx);
        const float cy = a.miny > body.y ? a.miny : (body.y < a.maxy ? body.y : a.maxy);
        if (cx == body.x && cy == body.y) {
            // centre buried in the box: push out the nearest face
            const float dl = body.x - a.minx, dr = a.maxx - body.x;
            const float dt = body.y - a.miny, db = a.maxy - body.y;
            const float m = std::min(std::min(dl, dr), std::min(dt, db));
            float nx = 0.0f, ny = 0.0f;
            if (m == dl) nx = -1.0f;
            else if (m == dr) nx = 1.0f;
            else if (m == dt) ny = -1.0f;
            else ny = 1.0f;
            const float e = body.restitution > a.restitution ? body.restitution : a.restitution;
            resolveContact(body, nx, ny, m + r, e, a.kind, 0.0f, 0.0f, contacts);
        } else {
            const float dx = body.x - cx, dy = body.y - cy;
            const float d = std::hypot(dx, dy);
            if (d < r && d > 1e-6f) {
                const float e = body.restitution > a.restitution ? body.restitution : a.restitution;
                resolveContact(body, dx / d, dy / d, r - d, e, a.kind, 0.0f, 0.0f, contacts);
            }
        }
    }
    for (const auto& f : set.flippers) {
        float sax, say, sbx, sby;
        flipperSeg(f, sax, say, sbx, sby);
        float cx, cy;
        closestOnSeg(body.x, body.y, sax, say, sbx, sby, cx, cy);
        const float dx = body.x - cx, dy = body.y - cy;
        const float d = std::hypot(dx, dy);
        if (d < r) {
            const float nx = d > 1e-6f ? dx / d : 0.0f;
            const float ny = d > 1e-6f ? dy / d : -1.0f;
            float svx, svy;
            flipperSurfaceVel(f, cx, cy, svx, svy);
            const float e = body.restitution > f.restitution ? body.restitution : f.restitution;
            resolveContact(body, nx, ny, r - d, e, f.kind, svx, svy, contacts);
        }
    }
}

} // namespace detail

/**
 * @brief Advance the world by one FULL frame using N fixed-dt substeps.
 *
 * Each substep: spring the flipper angles (derive ω), integrate gravity, then run
 * a swept conservative-advancement TOI loop (≤4 iters/substep) followed by a full
 * depenetration+impulse pass, then apply drag. Contacts are appended to @p
 * contacts (the caller polls/clears them per frame).
 *
 * @param body      The dynamic body (mutated).
 * @param set       The collider set (flipper angles / springs mutated).
 * @param gx,gy     Gravity acceleration (px/s²).
 * @param frameDt   Full frame time (s).
 * @param substeps  N fixed-dt substeps (default 4).
 * @param swept     true = swept+depenetration; false = naive discrete (tunnels).
 * @param contacts  Output buffer (appended; caller clears each frame).
 * @param stats     Output health counters.
 */
inline void stepFrame(BodyState& body, ColliderSet& set,
                      float gx, float gy, float frameDt, int substeps, bool swept,
                      std::vector<Contact>& contacts, StepStats& stats) {
    const float h = frameDt / static_cast<float>(substeps > 0 ? substeps : 1);

    for (int step = 0; step < substeps; ++step) {
        // 1. spring flipper angles toward target, derive angular velocity
        for (auto& f : set.flippers) {
            const float a = -Flipper::kStiffness * (f.angle - f.target) - Flipper::kDamping * f.springVel;
            f.springVel += a * h;
            const float prev = f.angle;
            f.angle += f.springVel * h;
            f.angVel = (f.angle - prev) / h;
        }

        // 2. integrate gravity
        body.vx += gx * h;
        body.vy += gy * h;

        const float sp = std::hypot(body.vx, body.vy);
        if (sp > stats.maxSpeed) stats.maxSpeed = sp;

        if (!swept) {
            // NAIVE path: discrete push-out only (the anti-pattern that tunnels).
            body.x += body.vx * h;
            body.y += body.vy * h;
            detail::depenetrate(body, set, contacts);
            detail::applyDrag(body, h);
            continue;
        }

        // 3. swept conservative-advancement TOI loop
        float remaining = 1.0f;
        constexpr int MAX_TOI = 4;
        for (int iter = 0; iter < MAX_TOI; ++iter) {
            const float dx = body.vx * h * remaining;
            const float dy = body.vy * h * remaining;
            if (dx == 0.0f && dy == 0.0f) break;

            SweepHit best;
            bool any = false;
            auto consider = [&](float t, float nx, float ny, uint8_t kind, float rest,
                                const Flipper* flipper) {
                if (!any || t < best.t) {
                    best = {t, nx, ny, kind, rest, flipper};
                    any = true;
                }
            };

            float tt, nx, ny;
            for (const auto& s : set.segments) {
                if (detail::sweepSegment(body.x, body.y, dx, dy, body.radius,
                                         s.ax, s.ay, s.bx, s.by, tt, nx, ny))
                    consider(tt, nx, ny, s.kind, s.restitution, nullptr);
            }
            for (const auto& cc : set.circles) {
                if (detail::raySphere(body.x, body.y, dx, dy, cc.cx, cc.cy,
                                      cc.r + body.radius, tt, nx, ny))
                    consider(tt, nx, ny, cc.kind, cc.restitution, nullptr);
            }
            for (const auto& a : set.aabbs) {
                if (detail::sweepAabb(body.x, body.y, dx, dy, body.radius,
                                      a.minx, a.miny, a.maxx, a.maxy, tt, nx, ny))
                    consider(tt, nx, ny, a.kind, a.restitution, nullptr);
            }
            for (const auto& f : set.flippers) {
                float sax, say, sbx, sby;
                detail::flipperSeg(f, sax, say, sbx, sby);
                if (detail::sweepSegment(body.x, body.y, dx, dy, body.radius,
                                         sax, say, sbx, sby, tt, nx, ny))
                    consider(tt, nx, ny, ColliderKinds::Flipper, f.restitution, &f);
            }

            if (!any) {
                body.x += dx;
                body.y += dy;
                break;
            }

            ++stats.toiIters;
            constexpr float SKIN = 0.02f;
            const float segLen = std::hypot(dx, dy);
            const float tAdv = segLen > 1e-9f
                ? (best.t - SKIN / segLen > 0.0f ? best.t - SKIN / segLen : 0.0f)
                : 0.0f;
            const float px = body.x + dx * tAdv;
            const float py = body.y + dy * tAdv;
            body.x = px;
            body.y = py;

            float sx = 0.0f, sy = 0.0f;
            if (best.flipper) detail::flipperSurfaceVel(*best.flipper, px, py, sx, sy);
            float rvx = body.vx - sx, rvy = body.vy - sy;
            const float vn = rvx * best.nx + rvy * best.ny;
            if (vn < 0.0f) {
                // Approaching: reflect and buffer the contact. A separating hit
                // (vn >= 0) yields no impulse and no contact — the same gate the
                // depenetration pass applies in resolveContact — so a polled
                // contact's relSpeed is always >= 0.
                const float e = body.restitution > best.restitution ? body.restitution : best.restitution;
                rvx -= (1.0f + e) * vn * best.nx;
                rvy -= (1.0f + e) * vn * best.ny;
                body.vx = rvx + sx;
                body.vy = rvy + sy;
                contacts.push_back({best.kind, px, py, best.nx, best.ny, -vn});
            }

            remaining *= 1.0f - best.t;
            if (remaining <= 1e-4f) break;
        }

        detail::depenetrate(body, set, contacts);
        detail::applyDrag(body, h);
    }
}

} // namespace enjin2
