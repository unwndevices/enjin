#pragma once

#include <enjin2/abstract/icomponent.hpp>
#include <enjin2/core/component.hpp>

namespace enjin2 {

/**
 * ComponentSeam - Strangler Fig pattern seam for component boundaries
 *
 * Allows enjin1 and enjin2 implementations to coexist during incremental migration.
 * Uses compile-time backend selection via USE_ENJIN1_BACKEND macro.
 */
class ComponentSeam : public IComponent {
public:
    /// Implementation type selector (deprecated - kept for backward compatibility)
    [[deprecated("Use compile-time USE_ENJIN1_BACKEND macro instead")]]
    enum class Implementation {
        LEGACY,  ///< Use enjin1 legacy implementation
        NEW      ///< Use enjin2 new implementation
    };

private:
    Implementation impl;      ///< Current implementation type (deprecated)
    Component* newImpl;        ///< Pointer to enjin2 implementation
    void* legacyImpl;          ///< Pointer to enjin1 implementation (opaque)
    bool enabled;              ///< Component enabled state

public:
    /**
     * Construct component seam with specified implementation type
     * @param implementation Which implementation to use (deprecated)
     */
    explicit ComponentSeam(Implementation implementation = Implementation::NEW)
        : impl(implementation), newImpl(nullptr), legacyImpl(nullptr), enabled(true) {}

    /**
     * Initialize component (called when entity awakens)
     * Routes to appropriate implementation based on compile-time backend
     */
    void awake() override {
#if USE_ENJIN1_BACKEND
        #error "enjin1 backend not yet integrated"
        // When enjin1 is integrated:
        // if (impl == Implementation::LEGACY && legacyImpl != nullptr) {
        //     // Route to enjin1 implementation
        // }
#else
        if (impl == Implementation::NEW && newImpl != nullptr) {
            newImpl->awake();
        }
#endif
    }

    /**
     * Start component (called after all awake() calls)
     * Routes to appropriate implementation based on compile-time backend
     */
    void start() override {
#if USE_ENJIN1_BACKEND
        #error "enjin1 backend not yet integrated"
        // When enjin1 is integrated:
        // if (impl == Implementation::LEGACY && legacyImpl != nullptr) {
        //     // Route to enjin1 implementation
        // }
#else
        if (impl == Implementation::NEW && newImpl != nullptr) {
            newImpl->start();
        }
#endif
    }

    /**
     * Update component each frame
     * @param deltaTime Time since last frame (in milliseconds)
     */
    void update(uint16_t deltaTime) override {
#if USE_ENJIN1_BACKEND
        #error "enjin1 backend not yet integrated"
        // When enjin1 is integrated:
        // if (impl == Implementation::LEGACY && legacyImpl != nullptr) {
        //     // Route to enjin1 implementation
        // }
#else
        if (impl == Implementation::NEW && newImpl != nullptr) {
            newImpl->update(deltaTime);
        }
#endif
    }

    /**
     * Late update component (called after all Update calls)
     * @param deltaTime Time since last frame (in milliseconds)
     */
    void lateUpdate(uint16_t deltaTime) override {
#if USE_ENJIN1_BACKEND
        #error "enjin1 backend not yet integrated"
        // When enjin1 is integrated:
        // if (impl == Implementation::LEGACY && legacyImpl != nullptr) {
        //     // Route to enjin1 implementation
        // }
#else
        if (impl == Implementation::NEW && newImpl != nullptr) {
            newImpl->lateUpdate(deltaTime);
        }
#endif
    }

    /**
     * Called when component is enabled
     */
    void onEnable() override {
#if USE_ENJIN1_BACKEND
        #error "enjin1 backend not yet integrated"
        // When enjin1 is integrated:
        // if (impl == Implementation::LEGACY && legacyImpl != nullptr) {
        //     // Route to enjin1 implementation
        // }
#else
        if (impl == Implementation::NEW && newImpl != nullptr) {
            newImpl->onEnable();
        }
#endif
    }

    /**
     * Called when component is disabled
     */
    void onDisable() override {
#if USE_ENJIN1_BACKEND
        #error "enjin1 backend not yet integrated"
        // When enjin1 is integrated:
        // if (impl == Implementation::LEGACY && legacyImpl != nullptr) {
        //     // Route to enjin1 implementation
        // }
#else
        if (impl == Implementation::NEW && newImpl != nullptr) {
            newImpl->onDisable();
        }
#endif
    }

    /**
     * Get owner object
     * @return Pointer to owner object
     */
    Object* getOwner() const override {
#if USE_ENJIN1_BACKEND
        #error "enjin1 backend not yet integrated"
        // When enjin1 is integrated:
        // if (impl == Implementation::LEGACY && legacyImpl != nullptr) {
        //     // Route to enjin1 implementation
        // }
        return nullptr;
#else
        if (impl == Implementation::NEW && newImpl != nullptr) {
            return newImpl->getOwner();
        }
        return nullptr;
#endif
    }

    /**
     * Check if component is enabled
     * @return True if enabled
     */
    bool isEnabled() const override {
        return enabled;
    }

    /**
     * Set component enabled state
     * @param isEnabled New enabled state
     */
    void setEnabled(bool isEnabled) override {
        if (enabled != isEnabled) {
            enabled = isEnabled;
            if (enabled) {
                onEnable();
            } else {
                onDisable();
            }
        }
    }

    /**
     * Switch to new enjin2 implementation (deprecated - kept for backward compatibility)
     * @param component Pointer to enjin2 component to use
     */
    [[deprecated("Use compile-time USE_ENJIN1_BACKEND macro for backend selection")]]
    void switchToNew(Component* component) {
        newImpl = component;
        impl = Implementation::NEW;
    }

    /**
     * Get current implementation type (deprecated - kept for backward compatibility)
     * @return Current implementation being used
     */
    [[deprecated("Use compile-time USE_ENJIN1_BACKEND macro for backend selection")]]
    Implementation getImplementation() const {
        return impl;
    }
};

} // namespace enjin2
