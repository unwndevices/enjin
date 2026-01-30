#pragma once

#include <enjin2/core/component.hpp>

namespace enjin2 {

/**
 * ComponentSeam - Strangler Fig pattern seam for component boundaries
 *
 * Allows enjin1 and enjin2 implementations to coexist during incremental migration,
 * with runtime switching capability between legacy and new implementations.
 */
class ComponentSeam {
public:
    /// Implementation type selector
    enum class Implementation {
        LEGACY,  ///< Use enjin1 legacy implementation
        NEW      ///< Use enjin2 new implementation
    };

private:
    Implementation impl;      ///< Current implementation type
    Component* newImpl;        ///< Pointer to enjin2 implementation
    void* legacyImpl;          ///< Pointer to enjin1 implementation (opaque)

public:
    /**
     * Construct component seam with specified implementation type
     * @param implementation Which implementation to use
     */
    explicit ComponentSeam(Implementation implementation)
        : impl(implementation), newImpl(nullptr), legacyImpl(nullptr) {}

    /**
     * Initialize component (called when entity awakens)
     * Routes to appropriate implementation based on current backend
     */
    void awake() {
        if (impl == Implementation::NEW && newImpl != nullptr) {
            newImpl->awake();
        } else if (impl == Implementation::LEGACY) {
            // TODO: Route to legacy implementation when enjin1 integration is ready
            // This is intentionally stubbed - legacy routing will be implemented
            // when enjin1 headers are integrated
        }
    }

    /**
     * Start component (called after all awake() calls)
     * Routes to appropriate implementation based on current backend
     */
    void start() {
        if (impl == Implementation::NEW && newImpl != nullptr) {
            newImpl->start();
        } else if (impl == Implementation::LEGACY) {
            // TODO: Route to legacy implementation when enjin1 integration is ready
        }
    }

    /**
     * Update component each frame
     * @param deltaTime Time since last frame (in milliseconds)
     */
    void update(uint16_t deltaTime) {
        if (impl == Implementation::NEW && newImpl != nullptr) {
            newImpl->update(deltaTime);
        } else if (impl == Implementation::LEGACY) {
            // TODO: Route to legacy implementation when enjin1 integration is ready
        }
    }

    /**
     * Switch to new enjin2 implementation
     * @param component Pointer to enjin2 component to use
     */
    void switchToNew(Component* component) {
        newImpl = component;
        impl = Implementation::NEW;
    }

    /**
     * Get current implementation type
     * @return Current implementation being used
     */
    Implementation getImplementation() const {
        return impl;
    }
};

} // namespace enjin2
