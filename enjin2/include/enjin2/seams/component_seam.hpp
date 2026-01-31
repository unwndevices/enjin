#pragma once

#include <enjin2/abstract/icomponent.hpp>
#include <enjin2/core/component.hpp>

namespace enjin2 {

/**
 * ComponentSeam - Component boundary seam for enjin2
 *
 * Provides clean interface between component system and enjin2 implementation.
 * All enjin1 backend infrastructure removed - enjin2-only implementation.
 */
class ComponentSeam : public IComponent {
private:
    Component* newImpl;  ///< Pointer to enjin2 implementation
    bool enabled;         ///< Component enabled state

public:
    /**
     * Construct component seam
     */
    explicit ComponentSeam()
        : newImpl(nullptr), enabled(true) {}

    /**
     * Initialize component (called when entity awakens)
     */
    void awake() override {
        if (newImpl != nullptr) {
            newImpl->awake();
        }
    }

    /**
     * Start component (called after all awake() calls)
     */
    void start() override {
        if (newImpl != nullptr) {
            newImpl->start();
        }
    }

    /**
     * Update component each frame
     * @param deltaTime Time since last frame (in milliseconds)
     */
    void update(uint16_t deltaTime) override {
        if (newImpl != nullptr) {
            newImpl->update(deltaTime);
        }
    }

    /**
     * Late update component (called after all Update calls)
     * @param deltaTime Time since last frame (in milliseconds)
     */
    void lateUpdate(uint16_t deltaTime) override {
        if (newImpl != nullptr) {
            newImpl->lateUpdate(deltaTime);
        }
    }

    /**
     * Called when component is enabled
     */
    void onEnable() override {
        if (newImpl != nullptr) {
            newImpl->onEnable();
        }
    }

    /**
     * Called when component is disabled
     */
    void onDisable() override {
        if (newImpl != nullptr) {
            newImpl->onDisable();
        }
    }

    /**
     * Get owner object
     * @return Pointer to owner object
     */
    Object* getOwner() const override {
        if (newImpl != nullptr) {
            return newImpl->getOwner();
        }
        return nullptr;
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
};

} // namespace enjin2
