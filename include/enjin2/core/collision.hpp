#pragma once

#include <cmath>

namespace enjin2 {

/**
 * @file collision.hpp
 * @brief Pure 2D collision detection utilities
 *
 * All functions use float coordinates. No Lua dependency.
 * Usable from C++ game code and Lua bindings.
 */
namespace collision {

/**
 * @brief AABB overlap test (axis-aligned bounding boxes)
 * @return true if the two rectangles overlap
 */
inline bool aabb(float x1, float y1, float w1, float h1,
                 float x2, float y2, float w2, float h2) {
    return !(x1 >= x2 + w2 || x2 >= x1 + w1 ||
             y1 >= y2 + h2 || y2 >= y1 + h1);
}

/**
 * @brief Circle vs circle overlap test
 * @return true if the circles overlap (or touch)
 */
inline bool circleCircle(float x1, float y1, float r1,
                        float x2, float y2, float r2) {
    float dx = x2 - x1;
    float dy = y2 - y1;
    float distSq = dx * dx + dy * dy;
    float sumR = r1 + r2;
    return distSq <= sumR * sumR;
}

/**
 * @brief Point inside rectangle test
 * @return true if point (px,py) is inside rect [rx,ry,rw,rh]
 */
inline bool pointInRect(float px, float py,
                        float rx, float ry, float rw, float rh) {
    return px >= rx && px < rx + rw && py >= ry && py < ry + rh;
}

/**
 * @brief Point inside circle test
 * @return true if point (px,py) is inside or on circle (cx,cy,r)
 */
inline bool pointInCircle(float px, float py,
                          float cx, float cy, float r) {
    float dx = px - cx;
    float dy = py - cy;
    return dx * dx + dy * dy <= r * r;
}

/**
 * @brief Line segment vs line segment intersection
 * @param ix Optional output: intersection X (only written if non-null and segments intersect)
 * @param iy Optional output: intersection Y
 * @return true if segments intersect
 */
inline bool lineLine(float x1, float y1, float x2, float y2,
                    float x3, float y3, float x4, float y4,
                    float* ix = nullptr, float* iy = nullptr) {
    float d1x = x2 - x1;
    float d1y = y2 - y1;
    float d2x = x4 - x3;
    float d2y = y4 - y3;

    float denom = d1x * d2y - d1y * d2x;
    if (std::fabs(denom) < 1e-10f)
        return false;  // parallel

    float t = ((x3 - x1) * d2y - (y3 - y1) * d2x) / denom;
    float s = ((x3 - x1) * d1y - (y3 - y1) * d1x) / denom;

    if (t < 0.0f || t > 1.0f || s < 0.0f || s > 1.0f)
        return false;

    if (ix) *ix = x1 + t * d1x;
    if (iy) *iy = y1 + t * d1y;
    return true;
}

/**
 * @brief Line segment vs circle intersection
 * @return true if the segment intersects (or touches) the circle
 */
inline bool lineCircle(float x1, float y1, float x2, float y2,
                       float cx, float cy, float r) {
    float dx = x2 - x1;
    float dy = y2 - y1;
    float lenSq = dx * dx + dy * dy;

    if (lenSq < 1e-10f) {
        // Degenerate segment: treat as point
        return pointInCircle(x1, y1, cx, cy, r);
    }

    // Closest point on segment to circle center
    float t = ((cx - x1) * dx + (cy - y1) * dy) / lenSq;
    if (t < 0.0f) t = 0.0f;
    if (t > 1.0f) t = 1.0f;

    float closestX = x1 + t * dx;
    float closestY = y1 + t * dy;
    float distSq = (cx - closestX) * (cx - closestX) + (cy - closestY) * (cy - closestY);
    return distSq <= r * r;
}

/**
 * @brief AABB overlap response — returns intersection rectangle
 * @param overlapX/Y/W/H Output: overlap rectangle (only written on hit)
 * @return true if the two rectangles overlap
 */
inline bool aabbOverlap(float x1, float y1, float w1, float h1,
                        float x2, float y2, float w2, float h2,
                        float* overlapX, float* overlapY,
                        float* overlapW, float* overlapH) {
    float left   = (x1 > x2) ? x1 : x2;
    float top    = (y1 > y2) ? y1 : y2;
    float right  = ((x1 + w1) < (x2 + w2)) ? (x1 + w1) : (x2 + w2);
    float bottom = ((y1 + h1) < (y2 + h2)) ? (y1 + h1) : (y2 + h2);

    if (left >= right || top >= bottom) return false;

    if (overlapX) *overlapX = left;
    if (overlapY) *overlapY = top;
    if (overlapW) *overlapW = right - left;
    if (overlapH) *overlapH = bottom - top;
    return true;
}

/**
 * @brief Circle vs circle response — returns separation normal and penetration depth
 * @param normalX/Y Output: unit normal from circle1 to circle2 (only written on hit)
 * @param depth Output: penetration depth (positive when overlapping)
 * @return true if the circles overlap
 */
inline bool circleCircleResponse(float x1, float y1, float r1,
                                 float x2, float y2, float r2,
                                 float* normalX, float* normalY,
                                 float* depth) {
    float dx = x2 - x1;
    float dy = y2 - y1;
    float distSq = dx * dx + dy * dy;
    float sumR = r1 + r2;
    if (distSq > sumR * sumR) return false;

    float dist = std::sqrt(distSq);
    if (dist < 1e-10f) {
        // Circles exactly overlap — pick arbitrary normal
        if (normalX) *normalX = 1.0f;
        if (normalY) *normalY = 0.0f;
        if (depth) *depth = sumR;
    } else {
        float invDist = 1.0f / dist;
        if (normalX) *normalX = dx * invDist;
        if (normalY) *normalY = dy * invDist;
        if (depth) *depth = sumR - dist;
    }
    return true;
}

/**
 * @brief Reflect a velocity vector against a surface normal
 * v' = v - 2(v·n)n
 * @param vx/vy Input velocity
 * @param nx/ny Surface normal (should be unit length)
 * @param outVx/outVy Output: reflected velocity
 */
inline void reflect(float vx, float vy, float nx, float ny,
                    float* outVx, float* outVy) {
    float dot = vx * nx + vy * ny;
    if (outVx) *outVx = vx - 2.0f * dot * nx;
    if (outVy) *outVy = vy - 2.0f * dot * ny;
}

}  // namespace collision
}  // namespace enjin2
