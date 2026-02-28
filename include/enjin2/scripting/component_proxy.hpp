#pragma once

namespace enjin2 {

class Component;  // Forward declaration only

/**
 * @brief Lua proxy userdata wrapping a raw Component* from self:get().
 *
 * Placed in a standalone header to avoid circular includes between component.hpp
 * and bindings.hpp. Component::~Component() sets valid = false before the component
 * is freed, preventing dangling-pointer access from stale Lua proxy references.
 *
 * Only one ComponentProxy should be active per Component at a time.
 * If self:get() is called multiple times for the same Component, the last
 * call overwrites Component::m_luaProxy — previous proxies will not receive
 * destructor notification (accepted v1.6 limitation, documented in STATE.md).
 */
struct ComponentProxy {
    Component* component;   ///< Non-owning. Do NOT dereference if valid == false.
    bool valid;             ///< Set false by Component::~Component() when component is destroyed.
};

} // namespace enjin2
