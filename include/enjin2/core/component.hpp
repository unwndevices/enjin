#pragma once

#include <cassert>
#include <cstdio>
#include <cstdint>
#include <type_traits>
#include "object.hpp"

namespace enjin2 {

/**
 * @brief Component base class
 *
 * All components in the Enjin system derive from this base class.
 * Components provide specific functionality to Objects through composition.
 */
class Component {
protected:
    Object* owner;      ///< The object that owns this component
    bool enabled;       ///< Whether the component is enabled

    /**
     * @brief Assert that a sibling component of type T exists on the same Object.
     *
     * In debug builds (NDEBUG not defined): calls assert(false) with a message naming the
     * requirement. The call stack identifies the failing component. No abort on missing
     * dep when the dep IS present (no-op).
     *
     * In release builds (NDEBUG defined): logs once via printf and disables this component.
     * No process abort — safe for ESP32.
     *
     * Call from awake() to declare component dependencies loudly.
     *
     * Naming note: assertRequires<T>() chosen over requires<T>() — 'requires' is a C++20
     * keyword and causes a compile error (Phase 26 decision).
     *
     * @tparam T Required component type (must derive from Component)
     */
    template<typename T>
    void assertRequires() {
        static_assert(std::is_base_of<Component, T>::value,
                      "T must derive from Component");
        if (owner->getComponent<T>() == nullptr) {
#ifndef NDEBUG
            assert(false && "assertRequires<T> failed: required component not present on owner Object");
#else
            // Release: log once and disable. No abort on ESP32.
            printf("[enjin2] assertRequires<T> failed: required component not present — disabling component\n");
            setEnabled(false);
#endif
        }
    }

public:
    /**
     * @brief Constructor
     * @param owner The object that owns this component
     */
    explicit Component(Object* owner) : owner(owner), enabled(true) {}

    /**
     * @brief Virtual destructor
     */
    virtual ~Component() = default;

    /**
     * @brief Get the owner object
     * @return Pointer to owner object
     */
    Object* getOwner() const { return owner; }

    /**
     * @brief Check if component is enabled
     * @return True if enabled
     */
    bool isEnabled() const { return enabled; }

    /**
     * @brief Set component enabled state
     * @param isEnabled New enabled state
     */
    void setEnabled(bool isEnabled) { enabled = isEnabled; }

    /**
     * @brief Awake is called when the component is created
     *
     * Use this for initialization that doesn't depend on other components.
     * This is called before Start().
     */
    virtual void awake() {}

    /**
     * @brief Start is called before the first frame update
     *
     * Use this for initialization that depends on other components
     * or objects being fully set up.
     */
    virtual void start() {}

    /**
     * @brief Update is called once per frame
     * @param dt Time since last frame in seconds
     */
    virtual void update(float dt) {}

    /**
     * @brief LateUpdate is called after all Update calls
     * @param dt Time since last frame in seconds
     */
    virtual void lateUpdate(float dt) {}

    /**
     * @brief Called when component is enabled
     */
    virtual void onEnable() {}

    /**
     * @brief Called when component is disabled
     */
    virtual void onDisable() {}
};

} // namespace enjin2
