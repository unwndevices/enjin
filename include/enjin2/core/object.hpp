#pragma once

#include "types.hpp"
#include <array>
#include <memory>
#include <functional>
#include <type_traits>

namespace enjin2 {

// Forward declarations
class Component;
class C_Position;

/**
 * @brief Anchor point enumeration for positioning
 */
enum class Anchor {
    TOP_LEFT,      ///< Top-left corner
    TOP_CENTER,    ///< Top center
    TOP_RIGHT,     ///< Top-right corner
    CENTER_LEFT,   ///< Center left
    CENTER,        ///< Center
    CENTER_RIGHT,  ///< Center right
    BOTTOM_LEFT,   ///< Bottom-left corner
    BOTTOM_CENTER, ///< Bottom center
    BOTTOM_RIGHT   ///< Bottom-right corner
};

/**
 * @brief Object base class for game entities
 * 
 * The Object class is the base class for all game entities in the Enjin system.
 * It manages components using static allocation and provides lifecycle methods.
 */
class Object {
private:
    static constexpr size_t MAX_COMPONENTS = 16;    ///< Maximum components per object
    
    std::array<std::unique_ptr<Component>, MAX_COMPONENTS> components;
    size_t componentCount;
    bool awoken;        ///< Whether Awake() has been called
    bool started;       ///< Whether Start() has been called
    bool active;        ///< Whether object is active
    
    // Cache frequently accessed components
    C_Position* position;
    
public:
    /**
     * @brief Constructor
     */
    Object();
    
    /**
     * @brief Virtual destructor
     */
    virtual ~Object() = default;
    
    /**
     * @brief Awake is called when object is created
     * 
     * Use this to ensure required components are present and
     * initialize component relationships.
     */
    virtual void awake();
    
    /**
     * @brief Start is called before the first frame update
     * 
     * Use this for initialization that depends on other objects
     * being fully set up.
     */
    virtual void start();
    
    /**
     * @brief Update is called once per frame
     * @param dt Time since last frame in seconds
     */
    virtual void update(float dt);

    /**
     * @brief LateUpdate is called after all Update calls
     * @param dt Time since last frame in seconds
     */
    virtual void lateUpdate(float dt);
    
    /**
     * @brief Check if object is queued for removal (matches original Enjin)
     * @return True if object should be removed
     */
    bool isQueuedForRemoval() const { return queued_for_removal; }
    
    /**
     * @brief Add a component to this object
     * @tparam T Component type (must derive from Component)
     * @tparam Args Constructor argument types
     * @param args Constructor arguments
     * @return Pointer to the created component or nullptr if failed
     */
    template<typename T, typename... Args>
    T* addComponent(Args&&... args) {
        static_assert(std::is_base_of<Component, T>::value, "T must derive from Component");
        
        if (componentCount >= MAX_COMPONENTS) {
            return nullptr;
        }
        
        // Create component and store in array
        std::unique_ptr<T> component(new T(this, std::forward<Args>(args)...));
        T* componentPtr = component.get();
        components[componentCount++] = std::move(component);
        
        // Cache position component using template specialization helper
        cachePositionIfType<T>(componentPtr);

        // Call awake if object has already been awoken
        if (awoken) {
            componentPtr->awake();
        }
        
        // Call start if object has already been started
        if (started) {
            componentPtr->start();
        }
        
        return componentPtr;
    }
    
    /**
     * @brief Get a component of specified type
     * @tparam T Component type
     * @return Pointer to component or nullptr if not found
     */
    template<typename T>
    T* getComponent() {
        static_assert(std::is_base_of<Component, T>::value, "T must derive from Component");
        
        for (size_t i = 0; i < componentCount; ++i) {
            if (auto component = dynamic_cast<T*>(components[i].get())) {
                return component;
            }
        }
        return nullptr;
    }
    
    /**
     * @brief Get all components of specified type
     * @tparam T Component type (must derive from Component)
     * @param out Caller-provided array to write matching component pointers into
     * @param maxOut Maximum number of results to write
     * @return Number of components written into out
     */
    template<typename T>
    size_t getComponents(T** out, size_t maxOut) const {
        static_assert(std::is_base_of<Component, T>::value, "T must derive from Component");
        size_t found = 0;
        for (size_t i = 0; i < componentCount && found < maxOut; ++i) {
            if (auto c = dynamic_cast<T*>(components[i].get())) {
                out[found++] = c;
            }
        }
        return found;
    }

    /**
     * @brief Check if object has a component of specified type
     * @tparam T Component type
     * @return True if component exists
     */
    template<typename T>
    bool hasComponent() const {
        return getComponent<T>() != nullptr;
    }
    
    /**
     * @brief Remove a component of specified type
     * @tparam T Component type
     * @return True if component was removed
     */
    template<typename T>
    bool removeComponent() {
        static_assert(std::is_base_of<Component, T>::value, "T must derive from Component");
        
        for (size_t i = 0; i < componentCount; ++i) {
            if (auto component = dynamic_cast<T*>(components[i].get())) {
                // Clear position cache if needed
                if (component == position) {
                    position = nullptr;
                }
                
                // Shift remaining components
                for (size_t j = i; j < componentCount - 1; ++j) {
                    components[j] = std::move(components[j + 1]);
                }
                componentCount--;
                return true;
            }
        }
        return false;
    }
    
    /**
     * @brief Get position component (cached for performance)
     * @return Position component pointer or nullptr
     */
    C_Position* getPosition() const { return position; }

    /**
     * @brief Check if object is active
     * @return True if active
     */
    bool isActive() const { return active; }
    
    /**
     * @brief Set object active state
     * @param isActive New active state
     */
    void setActive(bool isActive) { active = isActive; }
    
    /**
     * @brief Get total number of components
     * @return Component count
     */
    size_t getComponentCount() const { return componentCount; }

    /**
     * @brief Set object name (stores pointer — caller owns lifetime)
     * @param n Null-terminated name string or nullptr to clear
     */
    void setName(const char* n) { name = n; }

    /**
     * @brief Get object name
     * @return Pointer to name string or nullptr if not set
     */
    const char* getName() const { return name; }

    /**
     * @brief Add a tag to this object (up to MAX_TAGS = 8)
     * @param tag Null-terminated tag string (caller owns lifetime)
     * @return true if tag was added, false if tag array is full
     */
    bool addTag(const char* tag) {
        if (tagCount >= MAX_TAGS) return false;
        tags[tagCount++] = tag;
        return true;
    }

    /**
     * @brief Check if this object has a given tag
     * @param tag Tag string to look for
     * @return true if tag is present
     */
    bool hasTag(const char* tag) const {
        for (size_t i = 0; i < tagCount; ++i) {
            if (tags[i] && strcmp(tags[i], tag) == 0) return true;
        }
        return false;
    }

    /**
     * @brief Clear all tags from this object
     */
    void clearTags() { tags.fill(nullptr); tagCount = 0; }

    /**
     * @brief Get current number of tags
     * @return Tag count
     */
    size_t getTagCount() const { return tagCount; }

private:
    /**
     * @brief Helper to cache position component only if T is C_Position using SFINAE
     */
    template<typename T>
    typename std::enable_if<std::is_same<T, C_Position>::value>::type
    cachePositionIfType(T* componentPtr) {
        position = componentPtr;
    }
    
    template<typename T>
    typename std::enable_if<!std::is_same<T, C_Position>::value>::type
    cachePositionIfType(T* componentPtr) {
        // Do nothing for non-position components
    }
    bool queued_for_removal = false;  ///< Flag for object removal

    // Name and tag identity (zero heap allocation — raw const char* pointers only)
    const char* name = nullptr;
    static constexpr size_t MAX_TAGS = 8;
    std::array<const char*, MAX_TAGS> tags;
    size_t tagCount;

    /**
     * @brief Initialize cached component pointers
     */
    void initializeComponentCache();
};

} // namespace enjin2