#pragma once

#include <cstdint>

namespace enjin2 {

// Forward declaration
class Object;

/**
 * @brief Abstract component interface for component lifecycle
 *
 * Both enjin1 and enjin2 can implement this interface for compile-time polymorphism.
 * Provides the standard component lifecycle methods (awake, start, update, etc.).
 */
class IComponent {
public:
    /**
     * @brief Virtual destructor for proper cleanup through base pointer
     */
    virtual ~IComponent() = default;

    // ===== Component lifecycle methods =====

    /**
     * @brief Awake is called when component is created
     *
     * Use this for initialization that doesn't depend on other components.
     * This is called before Start().
     */
    virtual void awake() = 0;

    /**
     * @brief Start is called before first frame update
     *
     * Use this for initialization that depends on other components
     * or objects being fully set up.
     */
    virtual void start() = 0;

    /**
     * @brief Update is called once per frame
     * @param deltaTime Time since last frame in milliseconds
     */
    virtual void update(uint16_t deltaTime) = 0;

    /**
     * @brief LateUpdate is called after all Update calls
     * @param deltaTime Time since last frame in milliseconds
     */
    virtual void lateUpdate(uint16_t deltaTime) = 0;

    /**
     * @brief Called when component is enabled
     */
    virtual void onEnable() = 0;

    /**
     * @brief Called when component is disabled
     */
    virtual void onDisable() = 0;

    // ===== Component state queries =====

    /**
     * @brief Get owner object
     * @return Pointer to owner object
     */
    virtual Object* getOwner() const = 0;

    /**
     * @brief Check if component is enabled
     * @return True if enabled
     */
    virtual bool isEnabled() const = 0;

    /**
     * @brief Set component enabled state
     * @param isEnabled New enabled state
     */
    virtual void setEnabled(bool isEnabled) = 0;
};

} // namespace enjin2
