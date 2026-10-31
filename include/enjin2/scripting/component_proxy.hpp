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
 * One ComponentProxy per Component: get/add hand out the component's live
 * proxy again (a weak registry cache in bindings_proxy.cpp), so every Lua
 * reference goes stale together. The proxy's __gc clears
 * Component::m_luaProxy, so a component that outlives its collected proxy
 * does not write into freed Lua memory.
 */
struct ComponentProxy {
    Component* component;   ///< Non-owning. Do NOT dereference if valid == false.
    bool valid;             ///< Set false by Component::~Component() when component is destroyed.
};

} // namespace enjin2
