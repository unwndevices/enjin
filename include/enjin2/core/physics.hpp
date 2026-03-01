#pragma once

#include <cmath>
#include "collision.hpp"

/**
 * @file physics.hpp
 * @brief Stateless 2D physics helper functions
 *
 * All functions are pure, inline, and allocation-free.
 * Designed for use by Lua scripts or direct C++ game code.
 * No Lua dependency — suitable for both embedded and hosted targets.
 *
 * Phase 45: PHYS-01..PHYS-08
 */

namespace enjin2 {
namespace physics {

/**
 * @brief Apply gravity acceleration to velocity
 *
 * Adds (gx*dt, gy*dt) to velocity each frame.
 *
 * @param vx     Current velocity X
 * @param vy     Current velocity Y
 * @param gx     Gravity acceleration X (e.g. 0 for downward-only)
 * @param gy     Gravity acceleration Y (e.g. 980 pixels/s^2)
 * @param dt     Delta time in seconds
 * @param outVx  Output: new velocity X (may be null)
 * @param outVy  Output: new velocity Y (may be null)
 */
inline void applyGravity(float vx, float vy, float gx, float gy, float dt,
                         float* outVx, float* outVy) {
    if (outVx) *outVx = vx + gx * dt;
    if (outVy) *outVy = vy + gy * dt;
}

/**
 * @brief Reflect velocity against a surface normal, scaled by restitution
 *
 * Uses collision::reflect for the reflection, then scales by restitution.
 * restitution=1.0 = perfect elastic bounce; restitution=0.0 = stop dead.
 *
 * @param vx          Incoming velocity X
 * @param vy          Incoming velocity Y
 * @param nx          Surface normal X (should be unit length)
 * @param ny          Surface normal Y (should be unit length)
 * @param restitution Bounciness coefficient [0..1]
 * @param outVx       Output: reflected velocity X (may be null)
 * @param outVy       Output: reflected velocity Y (may be null)
 */
inline void bounce(float vx, float vy, float nx, float ny, float restitution,
                   float* outVx, float* outVy) {
    float rx, ry;
    collision::reflect(vx, vy, nx, ny, &rx, &ry);
    if (outVx) *outVx = rx * restitution;
    if (outVy) *outVy = ry * restitution;
}

/**
 * @brief Apply velocity damping (drag/friction)
 *
 * Reduces velocity by (drag * dt) fraction per second.
 * Factor is clamped to [0, 1] — velocity never changes sign due to drag.
 *
 * @param vx    Current velocity X
 * @param vy    Current velocity Y
 * @param drag  Drag coefficient (e.g. 1.0 = full stop in 1 second)
 * @param dt    Delta time in seconds
 * @param outVx Output: damped velocity X (may be null)
 * @param outVy Output: damped velocity Y (may be null)
 */
inline void applyDrag(float vx, float vy, float drag, float dt,
                      float* outVx, float* outVy) {
    float factor = 1.0f - drag * dt;
    if (factor < 0.0f) factor = 0.0f;
    if (outVx) *outVx = vx * factor;
    if (outVy) *outVy = vy * factor;
}

/**
 * @brief Apply damped spring force to a 1D position/velocity pair
 *
 * Implements Hooke's law with velocity damping:
 *   force = (target - pos) * stiffness - vel * damping
 *   new_vel = vel + force * dt
 *
 * Settles over time (critically/overdamped when damping >= 2*sqrt(stiffness)).
 * Call for each axis independently (x and y).
 *
 * @param pos       Current position (1D)
 * @param target    Rest/target position
 * @param vel       Current velocity (1D)
 * @param stiffness Spring constant k (higher = stiffer)
 * @param damping   Damping coefficient (higher = faster settling)
 * @param dt        Delta time in seconds
 * @param outVel    Output: new velocity after spring force applied (may be null)
 */
inline void springForce(float pos, float target, float vel,
                        float stiffness, float damping, float dt,
                        float* outVel) {
    float displacement = target - pos;
    float force = displacement * stiffness - vel * damping;
    if (outVel) *outVel = vel + force * dt;
}

/**
 * @brief Compute attraction force from point toward attractor
 *
 * Inverse-square falloff: force = strength / dist^2.
 * Epsilon in distance prevents division by zero for coincident points.
 * Force magnitude is capped at maxForce.
 *
 * @param x        Source point X
 * @param y        Source point Y
 * @param ax       Attractor X
 * @param ay       Attractor Y
 * @param strength Attraction strength (scales force magnitude)
 * @param maxForce Maximum force magnitude cap
 * @param outFx    Output: force vector X (may be null)
 * @param outFy    Output: force vector Y (may be null)
 */
inline void attract(float x, float y, float ax, float ay,
                    float strength, float maxForce,
                    float* outFx, float* outFy) {
    float dx = ax - x;
    float dy = ay - y;
    float distSq = dx * dx + dy * dy + 1e-4f;  // epsilon prevents div-by-zero
    float force = strength / distSq;
    if (force > maxForce) force = maxForce;
    float invDist = 1.0f / std::sqrt(distSq);
    if (outFx) *outFx = dx * invDist * force;
    if (outFy) *outFy = dy * invDist * force;
}

/**
 * @brief Compute tangential velocity for circular orbit
 *
 * Returns the velocity vector perpendicular to the radius vector,
 * which maintains circular orbit when applied each frame.
 *
 * outVx = -(dy / len) * speed
 * outVy =  (dx / len) * speed
 *
 * @param x     Orbiting body X
 * @param y     Orbiting body Y
 * @param cx    Center of orbit X
 * @param cy    Center of orbit Y
 * @param speed Desired orbital speed (pixels/s)
 * @param outVx Output: tangential velocity X (may be null)
 * @param outVy Output: tangential velocity Y (may be null)
 */
inline void orbitVelocity(float x, float y, float cx, float cy,
                          float speed,
                          float* outVx, float* outVy) {
    float dx = x - cx;
    float dy = y - cy;
    float len = std::sqrt(dx * dx + dy * dy);
    if (len < 1e-6f) {
        if (outVx) *outVx = 0.0f;
        if (outVy) *outVy = 0.0f;
        return;
    }
    if (outVx) *outVx = -dy / len * speed;
    if (outVy) *outVy =  dx / len * speed;
}

/**
 * @brief Integrate position by velocity * dt
 *
 * Euler integration: new_pos = pos + vel * dt
 *
 * @param x    Current position X
 * @param y    Current position Y
 * @param vx   Velocity X
 * @param vy   Velocity Y
 * @param dt   Delta time in seconds
 * @param outX Output: new position X (may be null)
 * @param outY Output: new position Y (may be null)
 */
inline void applyVelocity(float x, float y, float vx, float vy, float dt,
                          float* outX, float* outY) {
    if (outX) *outX = x + vx * dt;
    if (outY) *outY = y + vy * dt;
}

}  // namespace physics
}  // namespace enjin2
