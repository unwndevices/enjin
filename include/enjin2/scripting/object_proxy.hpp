#pragma once

namespace enjin2 {

class Object;  // Forward declaration only

/**
 * @brief Lua proxy userdata wrapping a raw Object* from engine.scene.find().
 *
 * Placed in a standalone header to avoid circular includes between object.hpp
 * and bindings.hpp. Object::~Object() sets valid = false before the Object is
 * freed, preventing dangling-pointer access from stale Lua proxy references.
 *
 * Only one ObjectProxy should be active per Object at a time. If
 * engine.scene.find() is called multiple times for the same Object, the last
 * call overwrites Object::m_luaProxy — the previous proxy is NOT invalidated,
 * but also will not receive the destructor notification.
 */
struct ObjectProxy {
    Object* object;   ///< Non-owning. Do NOT dereference if valid == false.
    bool valid;       ///< Set false by Object::~Object() when object is destroyed.
};

} // namespace enjin2
