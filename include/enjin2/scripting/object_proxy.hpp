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
 * One ObjectProxy per Object: find/spawn hand out the Object's live proxy
 * again (a weak registry cache in bindings_proxy.cpp), so every Lua reference
 * goes stale together. The proxy's __gc clears Object::m_luaProxy.
 */
struct ObjectProxy {
    Object* object;   ///< Non-owning. Do NOT dereference if valid == false.
    bool valid;       ///< Set false by Object::~Object() when object is destroyed.
};

} // namespace enjin2
