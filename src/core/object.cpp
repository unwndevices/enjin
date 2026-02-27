#include "../../include/enjin2/core/object.hpp"
#include "../../include/enjin2/core/component.hpp"
#include "../../include/enjin2/components/position.hpp"

namespace enjin2 {

Object::~Object() {
    // Invalidate the associated Lua ObjectProxy (if any) before memory is freed.
    // This prevents dangling-pointer dereference when a stale Lua proxy attempts
    // to access this Object after destruction.
    if (m_luaProxy) {
        m_luaProxy->valid = false;
        m_luaProxy = nullptr;
    }
}

Object::Object()
    : componentCount(0), awoken(false), started(false), active(true),
      position(nullptr), name(nullptr), tagCount(0) {

    // Initialize component array
    for (auto& component : components) {
        component = nullptr;
    }
    tags.fill(nullptr);
    
    // Automatically add position component - most objects need this
    addComponent<C_Position>();
}

void Object::awake() {
    if (awoken) return;
    
    // Call awake on all components
    for (size_t i = 0; i < componentCount; ++i) {
        if (components[i]) {
            components[i]->awake();
        }
    }
    
    awoken = true;
}

void Object::start() {
    if (started) return;
    
    // Ensure awake has been called first
    if (!awoken) {
        awake();
    }
    
    // Call start on all components
    for (size_t i = 0; i < componentCount; ++i) {
        if (components[i]) {
            components[i]->start();
        }
    }
    
    started = true;
}

void Object::update(float dt) {
    if (!active) return;

    // Ensure start has been called
    if (!started) {
        start();
    }

    // Update all enabled components
    for (size_t i = 0; i < componentCount; ++i) {
        if (components[i] && components[i]->isEnabled()) {
            components[i]->update(dt);
        }
    }
}

void Object::lateUpdate(float dt) {
    if (!active) return;

    // Late update all enabled components
    for (size_t i = 0; i < componentCount; ++i) {
        if (components[i] && components[i]->isEnabled()) {
            components[i]->lateUpdate(dt);
        }
    }
}

void Object::initializeComponentCache() {
    // Reset caches
    position = nullptr;

    // Rebuild position cache
    for (size_t i = 0; i < componentCount; ++i) {
        if (!components[i]) continue;

        // Cache position component
        if (!position) {
            position = dynamic_cast<C_Position*>(components[i].get());
        }
    }
}

} // namespace enjin2