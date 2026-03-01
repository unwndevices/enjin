/**
 * @file bindings_physics.cpp
 * @brief Lua bindings for engine.physics.* sub-table
 *
 * Exposes all enjin2::physics:: helpers to Lua as engine.physics.* functions.
 * Global gravity state is stored in LuaBindings (m_gravityX, m_gravityY).
 * All position/velocity parameters accept both plain number pairs and Vec2 userdata.
 *
 * Phase 45: PHYS-09..PHYS-13
 */
#include "../../include/enjin2/scripting/bindings.hpp"
#include "../../include/enjin2/scripting/bind_helpers.hpp"
#include "../../include/enjin2/core/physics.hpp"
#include "../../include/enjin2/core/scene.hpp"
#include "../../include/enjin2/core/object.hpp"
#include "../../include/enjin2/components/position.hpp"
#include "../../include/enjin2/components/tilemap.hpp"
#include <cmath>
#include <cstdio>

namespace enjin2 {

//==============================================================================
// engine.physics.setGravity / getGravity
//==============================================================================

// --- engine.physics.setGravity(gx, gy) ---
// Stores global gravity in LuaBindings members.
int LuaBindings::lua_engine_physics_setGravity(lua_State* L) {
    LuaBindings* b = getBindings(L);
    if (!b) return 0;
    b->m_gravityX = static_cast<float>(luaL_checknumber(L, 1));
    b->m_gravityY = static_cast<float>(luaL_checknumber(L, 2));
    return 0;
}

// --- engine.physics.getGravity() -> gx, gy ---
// Returns current global gravity as two numbers.
int LuaBindings::lua_engine_physics_getGravity(lua_State* L) {
    LuaBindings* b = getBindings(L);
    if (!b) {
        lua_pushnumber(L, 0.0);
        lua_pushnumber(L, 0.0);
        return 2;
    }
    lua_pushnumber(L, static_cast<lua_Number>(b->m_gravityX));
    lua_pushnumber(L, static_cast<lua_Number>(b->m_gravityY));
    return 2;
}

//==============================================================================
// engine.physics.applyGravity
//
// Overloads (disambiguated by lua_gettop):
//   applyGravity(vx, vy, dt)              — 3 args: use global gravity
//   applyGravity(vx, vy, gx, gy, dt)     — 5 args: use override gravity
//   applyGravity(Vec2, dt)               — 2 args: Vec2 + dt, global gravity
//   applyGravity(Vec2, gx, gy, dt)       — 4 args: Vec2 + override gravity
//==============================================================================

int LuaBindings::lua_engine_physics_applyGravity(lua_State* L) {
    LuaBindings* b = getBindings(L);
    if (!b) {
        lua_pushnumber(L, 0.0);
        lua_pushnumber(L, 0.0);
        return 2;
    }

    float vx, vy, gx, gy, dt;

    // Check if first argument is Vec2 userdata
    auto* v = static_cast<Vec2*>(luaL_testudata(L, 1, "Vec2"));
    if (v) {
        vx = v->x;
        vy = v->y;
        int nargs = lua_gettop(L);
        if (nargs >= 4) {
            // applyGravity(Vec2, gx, gy, dt) — override gravity
            gx = static_cast<float>(luaL_checknumber(L, 2));
            gy = static_cast<float>(luaL_checknumber(L, 3));
            dt = static_cast<float>(luaL_checknumber(L, 4));
        } else {
            // applyGravity(Vec2, dt) — global gravity
            gx = b->m_gravityX;
            gy = b->m_gravityY;
            dt = static_cast<float>(luaL_checknumber(L, 2));
        }
    } else {
        vx = static_cast<float>(luaL_checknumber(L, 1));
        vy = static_cast<float>(luaL_checknumber(L, 2));
        int nargs = lua_gettop(L);
        if (nargs >= 5) {
            // applyGravity(vx, vy, gx, gy, dt) — override gravity
            gx = static_cast<float>(luaL_checknumber(L, 3));
            gy = static_cast<float>(luaL_checknumber(L, 4));
            dt = static_cast<float>(luaL_checknumber(L, 5));
        } else {
            // applyGravity(vx, vy, dt) — global gravity
            gx = b->m_gravityX;
            gy = b->m_gravityY;
            dt = static_cast<float>(luaL_checknumber(L, 3));
        }
    }

    float outVx, outVy;
    physics::applyGravity(vx, vy, gx, gy, dt, &outVx, &outVy);
    lua_pushnumber(L, static_cast<lua_Number>(outVx));
    lua_pushnumber(L, static_cast<lua_Number>(outVy));
    return 2;
}

//==============================================================================
// engine.physics.bounce(vx, vy, nx, ny, restitution) -> outVx, outVy
// Also accepts: bounce(Vec2 vel, Vec2 normal, restitution)
//==============================================================================

int LuaBindings::lua_engine_physics_bounce(lua_State* L) {
    LuaBindings* b = getBindings(L);
    if (!b) {
        lua_pushnumber(L, 0.0);
        lua_pushnumber(L, 0.0);
        return 2;
    }

    float vx, vy, nx, ny, restitution;

    // Check if first argument is Vec2 (velocity)
    auto* vel = static_cast<Vec2*>(luaL_testudata(L, 1, "Vec2"));
    if (vel) {
        vx = vel->x;
        vy = vel->y;
        // Check if second argument is Vec2 (normal)
        auto* norm = static_cast<Vec2*>(luaL_testudata(L, 2, "Vec2"));
        if (norm) {
            nx = norm->x;
            ny = norm->y;
            restitution = static_cast<float>(luaL_checknumber(L, 3));
        } else {
            nx = static_cast<float>(luaL_checknumber(L, 2));
            ny = static_cast<float>(luaL_checknumber(L, 3));
            restitution = static_cast<float>(luaL_checknumber(L, 4));
        }
    } else {
        vx = static_cast<float>(luaL_checknumber(L, 1));
        vy = static_cast<float>(luaL_checknumber(L, 2));
        nx = static_cast<float>(luaL_checknumber(L, 3));
        ny = static_cast<float>(luaL_checknumber(L, 4));
        restitution = static_cast<float>(luaL_checknumber(L, 5));
    }

    float outVx, outVy;
    physics::bounce(vx, vy, nx, ny, restitution, &outVx, &outVy);
    lua_pushnumber(L, static_cast<lua_Number>(outVx));
    lua_pushnumber(L, static_cast<lua_Number>(outVy));
    return 2;
}

//==============================================================================
// engine.physics.applyDrag(vx, vy, drag, dt) -> outVx, outVy
// Also accepts: applyDrag(Vec2, drag, dt)
//==============================================================================

int LuaBindings::lua_engine_physics_applyDrag(lua_State* L) {
    LuaBindings* b = getBindings(L);
    if (!b) {
        lua_pushnumber(L, 0.0);
        lua_pushnumber(L, 0.0);
        return 2;
    }

    float vx, vy, drag, dt;

    auto* v = static_cast<Vec2*>(luaL_testudata(L, 1, "Vec2"));
    if (v) {
        vx   = v->x;
        vy   = v->y;
        drag = static_cast<float>(luaL_checknumber(L, 2));
        dt   = static_cast<float>(luaL_checknumber(L, 3));
    } else {
        vx   = static_cast<float>(luaL_checknumber(L, 1));
        vy   = static_cast<float>(luaL_checknumber(L, 2));
        drag = static_cast<float>(luaL_checknumber(L, 3));
        dt   = static_cast<float>(luaL_checknumber(L, 4));
    }

    float outVx, outVy;
    physics::applyDrag(vx, vy, drag, dt, &outVx, &outVy);
    lua_pushnumber(L, static_cast<lua_Number>(outVx));
    lua_pushnumber(L, static_cast<lua_Number>(outVy));
    return 2;
}

//==============================================================================
// engine.physics.springForce(pos, target, vel, stiffness, damping, dt) -> outVel
// 1D scalar — call once per axis.
//==============================================================================

int LuaBindings::lua_engine_physics_springForce(lua_State* L) {
    LuaBindings* b = getBindings(L);
    if (!b) { lua_pushnumber(L, 0.0); return 1; }

    float pos       = static_cast<float>(luaL_checknumber(L, 1));
    float target    = static_cast<float>(luaL_checknumber(L, 2));
    float vel       = static_cast<float>(luaL_checknumber(L, 3));
    float stiffness = static_cast<float>(luaL_checknumber(L, 4));
    float damping   = static_cast<float>(luaL_checknumber(L, 5));
    float dt        = static_cast<float>(luaL_checknumber(L, 6));

    float outVel;
    physics::springForce(pos, target, vel, stiffness, damping, dt, &outVel);
    lua_pushnumber(L, static_cast<lua_Number>(outVel));
    return 1;
}

//==============================================================================
// engine.physics.attract(x, y, ax, ay, strength, maxForce) -> fx, fy
// Also accepts: attract(Vec2 point, Vec2 attractor, strength, maxForce)
//==============================================================================

int LuaBindings::lua_engine_physics_attract(lua_State* L) {
    LuaBindings* b = getBindings(L);
    if (!b) {
        lua_pushnumber(L, 0.0);
        lua_pushnumber(L, 0.0);
        return 2;
    }

    float x, y, ax, ay, strength, maxForce;

    auto* p = static_cast<Vec2*>(luaL_testudata(L, 1, "Vec2"));
    auto* a = static_cast<Vec2*>(luaL_testudata(L, 2, "Vec2"));
    if (p && a) {
        x        = p->x;
        y        = p->y;
        ax       = a->x;
        ay       = a->y;
        strength = static_cast<float>(luaL_checknumber(L, 3));
        maxForce = static_cast<float>(luaL_checknumber(L, 4));
    } else {
        x        = static_cast<float>(luaL_checknumber(L, 1));
        y        = static_cast<float>(luaL_checknumber(L, 2));
        ax       = static_cast<float>(luaL_checknumber(L, 3));
        ay       = static_cast<float>(luaL_checknumber(L, 4));
        strength = static_cast<float>(luaL_checknumber(L, 5));
        maxForce = static_cast<float>(luaL_checknumber(L, 6));
    }

    float outFx, outFy;
    physics::attract(x, y, ax, ay, strength, maxForce, &outFx, &outFy);
    lua_pushnumber(L, static_cast<lua_Number>(outFx));
    lua_pushnumber(L, static_cast<lua_Number>(outFy));
    return 2;
}

//==============================================================================
// engine.physics.orbitVelocity(x, y, cx, cy, speed) -> outVx, outVy
// Also accepts: orbitVelocity(Vec2 body, Vec2 center, speed)
//==============================================================================

int LuaBindings::lua_engine_physics_orbitVelocity(lua_State* L) {
    LuaBindings* b = getBindings(L);
    if (!b) {
        lua_pushnumber(L, 0.0);
        lua_pushnumber(L, 0.0);
        return 2;
    }

    float x, y, cx, cy, speed;

    auto* body   = static_cast<Vec2*>(luaL_testudata(L, 1, "Vec2"));
    auto* center = static_cast<Vec2*>(luaL_testudata(L, 2, "Vec2"));
    if (body && center) {
        x     = body->x;
        y     = body->y;
        cx    = center->x;
        cy    = center->y;
        speed = static_cast<float>(luaL_checknumber(L, 3));
    } else {
        x     = static_cast<float>(luaL_checknumber(L, 1));
        y     = static_cast<float>(luaL_checknumber(L, 2));
        cx    = static_cast<float>(luaL_checknumber(L, 3));
        cy    = static_cast<float>(luaL_checknumber(L, 4));
        speed = static_cast<float>(luaL_checknumber(L, 5));
    }

    float outVx, outVy;
    physics::orbitVelocity(x, y, cx, cy, speed, &outVx, &outVy);
    lua_pushnumber(L, static_cast<lua_Number>(outVx));
    lua_pushnumber(L, static_cast<lua_Number>(outVy));
    return 2;
}

//==============================================================================
// engine.physics.applyVelocity(x, y, vx, vy, dt) -> outX, outY
// Also accepts: applyVelocity(Vec2 pos, Vec2 vel, dt)
//==============================================================================

int LuaBindings::lua_engine_physics_applyVelocity(lua_State* L) {
    LuaBindings* b = getBindings(L);
    if (!b) {
        lua_pushnumber(L, 0.0);
        lua_pushnumber(L, 0.0);
        return 2;
    }

    float x, y, vx, vy, dt;

    auto* pos = static_cast<Vec2*>(luaL_testudata(L, 1, "Vec2"));
    auto* vel = static_cast<Vec2*>(luaL_testudata(L, 2, "Vec2"));
    if (pos && vel) {
        x  = pos->x;
        y  = pos->y;
        vx = vel->x;
        vy = vel->y;
        dt = static_cast<float>(luaL_checknumber(L, 3));
    } else {
        x  = static_cast<float>(luaL_checknumber(L, 1));
        y  = static_cast<float>(luaL_checknumber(L, 2));
        vx = static_cast<float>(luaL_checknumber(L, 3));
        vy = static_cast<float>(luaL_checknumber(L, 4));
        dt = static_cast<float>(luaL_checknumber(L, 5));
    }

    float outX, outY;
    physics::applyVelocity(x, y, vx, vy, dt, &outX, &outY);
    lua_pushnumber(L, static_cast<lua_Number>(outX));
    lua_pushnumber(L, static_cast<lua_Number>(outY));
    return 2;
}

//==============================================================================
// engine.physics.raycast(x1, y1, x2, y2)
//
// Returns: hit [bool], hitX [number], hitY [number], dist [number], what [string]
//   - "tilemap" when a non-zero tile blocks the ray (DDA)
//   - "object"  when a scene object's point is within 8px of the ray segment
// Returns just false (1 value) when no hit.
// Silent no-op (returns false) when no active scene is set.
//==============================================================================

int LuaBindings::lua_engine_physics_raycast(lua_State* L) {
    LuaBindings* b = getBindings(L);
    if (!b || !b->m_activeScene) {
        lua_pushboolean(L, 0);
        return 1;
    }

    float rx1 = static_cast<float>(luaL_checknumber(L, 1));
    float ry1 = static_cast<float>(luaL_checknumber(L, 2));
    float rx2 = static_cast<float>(luaL_checknumber(L, 3));
    float ry2 = static_cast<float>(luaL_checknumber(L, 4));

    Scene* scene = b->m_activeScene;

    float rayDx = rx2 - rx1;
    float rayDy = ry2 - ry1;
    float rayLen = std::sqrt(rayDx * rayDx + rayDy * rayDy);
    if (rayLen < 1e-6f) {
        lua_pushboolean(L, 0);
        return 1;
    }

    // ── Stage 1: DDA tilemap scan ────────────────────────────────────────────
    {
        // Scan for first tilemap via forEach (avoids const/non-const getComponent issue)
        C_Tilemap* tm = nullptr;
        scene->getObjects().forEach([&](Object* obj) {
            if (!tm) { tm = obj->getComponent<C_Tilemap>(); }
        });
        if (tm) {
            const SpriteSheet& sheet = tm->getSheet();
                float tileW = static_cast<float>(sheet.cellW > 0 ? sheet.cellW : 8);
                float tileH = static_cast<float>(sheet.cellH > 0 ? sheet.cellH : 8);

                // Convert ray start/end to tile coordinates (world space)
                // World = screen + scroll (tilemap already applies scrollX/scrollY in draw,
                // but for the raycast we operate in screen/world coords directly).
                float wx1 = rx1;
                float wy1 = ry1;

                float mapStartX = wx1 / tileW;
                float mapStartY = wy1 / tileH;

                int tileX = static_cast<int>(std::floor(mapStartX));
                int tileY = static_cast<int>(std::floor(mapStartY));

                int stepX = (rayDx > 0.0f) ? 1 : -1;
                int stepY = (rayDy > 0.0f) ? 1 : -1;

                // tMax = parameter t at which ray crosses next vertical/horizontal boundary
                float tMaxX, tMaxY, tDeltaX, tDeltaY;

                if (std::fabs(rayDx) < 1e-6f) {
                    tMaxX   = 1e30f;
                    tDeltaX = 1e30f;
                } else {
                    float nextBX = (stepX > 0)
                        ? (static_cast<float>(tileX + 1) * tileW)
                        : (static_cast<float>(tileX) * tileW);
                    tMaxX   = std::fabs((nextBX - wx1) / rayDx);
                    tDeltaX = std::fabs(tileW / rayDx);
                }

                if (std::fabs(rayDy) < 1e-6f) {
                    tMaxY   = 1e30f;
                    tDeltaY = 1e30f;
                } else {
                    float nextBY = (stepY > 0)
                        ? (static_cast<float>(tileY + 1) * tileH)
                        : (static_cast<float>(tileY) * tileH);
                    tMaxY   = std::fabs((nextBY - wy1) / rayDy);
                    tDeltaY = std::fabs(tileH / rayDy);
                }

                // Walk the grid until we hit a solid tile or exit the map
                static constexpr int MAX_DDA_STEPS = 256;
                for (int step = 0; step < MAX_DDA_STEPS; ++step) {
                    // Check bounds
                    if (tileX < 0 || tileY < 0
                        || tileX >= static_cast<int>(tm->getMapWidth())
                        || tileY >= static_cast<int>(tm->getMapHeight())) {
                        break;
                    }

                    uint8_t tid = tm->getTile(
                        static_cast<uint8_t>(tileX),
                        static_cast<uint8_t>(tileY));

                    if (tid != 0) {
                        // Hit — compute world-space intersection point
                        float t = (tMaxX < tMaxY) ? tMaxX - tDeltaX : tMaxY - tDeltaY;
                        if (t < 0.0f) t = 0.0f;
                        float hitX = rx1 + rayDx * t;
                        float hitY = ry1 + rayDy * t;
                        float dist = t;  // t is already distance along ray in world units

                        lua_pushboolean(L, 1);
                        lua_pushnumber(L, static_cast<lua_Number>(hitX));
                        lua_pushnumber(L, static_cast<lua_Number>(hitY));
                        lua_pushnumber(L, static_cast<lua_Number>(dist));
                        lua_pushstring(L, "tilemap");
                        return 5;
                    }

                    // Advance to next tile
                    if (tMaxX < tMaxY) {
                        tMaxX += tDeltaX;
                        tileX += stepX;
                    } else {
                        tMaxY += tDeltaY;
                        tileY += stepY;
                    }
                }
        }
    }

    // ── Stage 2: Linear object scan (ray-to-point proximity) ────────────────
    {
        static constexpr float HIT_RADIUS = 8.0f;   // Fixed hit radius in pixels
        static constexpr float HIT_RADIUS_SQ = HIT_RADIUS * HIT_RADIUS;

        float bestT   = rayLen + 1.0f;  // init beyond ray end
        float bestHX  = 0.0f;
        float bestHY  = 0.0f;
        bool  bestHit = false;

        // Iterate all objects in the scene via forEach
        scene->getObjects().forEach([&](Object* obj) {
            C_Position* posComp = obj->getComponent<C_Position>();
            if (!posComp) return;

            float px = static_cast<float>(posComp->getPosition().x);
            float py = static_cast<float>(posComp->getPosition().y);

            // Closest point on segment (rx1,ry1)-(rx2,ry2) to point (px,py)
            float dx = px - rx1;
            float dy = py - ry1;
            float t  = (dx * rayDx + dy * rayDy) / (rayLen * rayLen);
            if (t < 0.0f) t = 0.0f;
            if (t > 1.0f) t = 1.0f;

            float closestX = rx1 + t * rayDx;
            float closestY = ry1 + t * rayDy;
            float ddx = px - closestX;
            float ddy = py - closestY;
            float distSq = ddx * ddx + ddy * ddy;

            if (distSq <= HIT_RADIUS_SQ) {
                float tWorld = t * rayLen;
                if (tWorld < bestT) {
                    bestT   = tWorld;
                    bestHX  = closestX;
                    bestHY  = closestY;
                    bestHit = true;
                }
            }
        });

        if (bestHit) {
            lua_pushboolean(L, 1);
            lua_pushnumber(L, static_cast<lua_Number>(bestHX));
            lua_pushnumber(L, static_cast<lua_Number>(bestHY));
            lua_pushnumber(L, static_cast<lua_Number>(bestT));
            lua_pushstring(L, "object");
            return 5;
        }
    }

    // No hit
    lua_pushboolean(L, 0);
    return 1;
}

} // namespace enjin2
