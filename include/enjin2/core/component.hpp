#pragma once

#include <cstdint>

namespace enjin2 {

// Forward declaration
class Object;

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