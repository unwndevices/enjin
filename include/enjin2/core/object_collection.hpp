#pragma once

#include "object.hpp"
#include <array>
#include <memory>
#include <functional>

namespace enjin2 {

/**
 * @brief Collection for managing multiple objects
 * 
 * Provides static allocation-based management of objects with
 * lifecycle control and organized updates.
 */
class ObjectCollection {
private:
    static constexpr size_t MAX_OBJECTS = 128;  ///< Maximum objects in collection
    
    std::array<std::unique_ptr<Object>, MAX_OBJECTS> objects;
    size_t objectCount;
    bool initialized;
    
public:
    /**
     * @brief Constructor
     */
    ObjectCollection() : objectCount(0), initialized(false) {
        for (auto& obj : objects) {
            obj = nullptr;
        }
    }
    
    /**
     * @brief Destructor
     */
    ~ObjectCollection() = default;
    
    /**
     * @brief Initialize the collection
     * 
     * Calls awake() on all objects in the collection.
     */
    void initialize() {
        if (initialized) return;
        
        for (size_t i = 0; i < objectCount; ++i) {
            if (objects[i]) {
                objects[i]->awake();
            }
        }
        
        initialized = true;
    }
    
    /**
     * @brief Start the collection
     * 
     * Calls start() on all objects in the collection.
     */
    void start() {
        if (!initialized) {
            initialize();
        }
        
        for (size_t i = 0; i < objectCount; ++i) {
            if (objects[i]) {
                objects[i]->start();
            }
        }
    }
    
    /**
     * @brief Update all objects in the collection
     * @param dt Time since last frame in seconds
     */
    void update(float dt) {
        for (size_t i = 0; i < objectCount; ++i) {
            if (objects[i] && objects[i]->isActive()) {
                objects[i]->update(dt);
            }
        }
    }

    /**
     * @brief Late update all objects in the collection
     * @param dt Time since last frame in seconds
     */
    void lateUpdate(float dt) {
        for (size_t i = 0; i < objectCount; ++i) {
            if (objects[i] && objects[i]->isActive()) {
                objects[i]->lateUpdate(dt);
            }
        }
    }
    
    /**
     * @brief Add an object to the collection
     * @tparam T Object type (must derive from Object)
     * @tparam Args Constructor argument types
     * @param args Constructor arguments
     * @return Pointer to created object or nullptr if failed
     */
    template<typename T, typename... Args>
    T* addObject(Args&&... args) {
        static_assert(std::is_base_of<Object, T>::value, "T must derive from Object");
        
        if (objectCount >= MAX_OBJECTS) {
            return nullptr;
        }
        
        std::unique_ptr<T> object(new T(std::forward<Args>(args)...));
        T* objectPtr = object.get();
        objects[objectCount++] = std::move(object);
        
        // If collection is already initialized, initialize the new object
        if (initialized) {
            objectPtr->awake();
            objectPtr->start();
        }
        
        return objectPtr;
    }
    
    /**
     * @brief Remove an object from the collection
     * @param object Object to remove
     * @return True if object was removed
     */
    bool removeObject(Object* object) {
        for (size_t i = 0; i < objectCount; ++i) {
            if (objects[i].get() == object) {
                // Shift remaining objects
                for (size_t j = i; j < objectCount - 1; ++j) {
                    objects[j] = std::move(objects[j + 1]);
                }
                objects[objectCount - 1] = nullptr;  // clear trailing slot
                objectCount--;
                return true;
            }
        }
        return false;
    }
    
    /**
     * @brief Find first object of specified type
     * @tparam T Object type
     * @return Pointer to object or nullptr if not found
     */
    template<typename T>
    T* findObject() {
        static_assert(std::is_base_of<Object, T>::value, "T must derive from Object");
        
        for (size_t i = 0; i < objectCount; ++i) {
            if (auto obj = dynamic_cast<T*>(objects[i].get())) {
                return obj;
            }
        }
        return nullptr;
    }
    
    /**
     * @brief Find all objects of specified type
     * @tparam T Object type
     * @param results Array to store results
     * @param maxResults Maximum number of results
     * @return Number of objects found
     */
    template<typename T>
    size_t findObjects(T** results, size_t maxResults) {
        static_assert(std::is_base_of<Object, T>::value, "T must derive from Object");
        
        size_t found = 0;
        for (size_t i = 0; i < objectCount && found < maxResults; ++i) {
            if (auto obj = dynamic_cast<T*>(objects[i].get())) {
                results[found++] = obj;
            }
        }
        return found;
    }
    
    /**
     * @brief Find object with component of specified type
     * @tparam T Component type
     * @return Pointer to object or nullptr if not found
     */
    template<typename T>
    Object* findObjectWithComponent() {
        static_assert(std::is_base_of<Component, T>::value, "T must derive from Component");
        
        for (size_t i = 0; i < objectCount; ++i) {
            if (objects[i] && objects[i]->hasComponent<T>()) {
                return objects[i].get();
            }
        }
        return nullptr;
    }
    
    /**
     * @brief Apply function to all objects
     * @param func Function to apply (takes Object* parameter)
     */
    void forEach(std::function<void(Object*)> func) {
        for (size_t i = 0; i < objectCount; ++i) {
            if (objects[i]) {
                func(objects[i].get());
            }
        }
    }
    
    /**
     * @brief Find object by name
     * @param name Name to search for (nullptr returns nullptr immediately)
     * @return Pointer to first matching Object or nullptr if not found
     */
    Object* findByName(const char* name) {
        if (!name) return nullptr;
        for (size_t i = 0; i < objectCount; ++i) {
            if (objects[i] && objects[i]->getName() &&
                strcmp(objects[i]->getName(), name) == 0) {
                return objects[i].get();
            }
        }
        return nullptr;
    }

    /**
     * @brief Find all objects with a given tag
     * @param tag Tag to search for
     * @param results Caller-provided buffer to receive Object pointers
     * @param maxResults Maximum number of results to write into buffer
     * @return Number of matching objects written into buffer
     */
    size_t findAllWithTag(const char* tag, Object** results, size_t maxResults) {
        if (!tag || !results || maxResults == 0) return 0;
        size_t found = 0;
        for (size_t i = 0; i < objectCount && found < maxResults; ++i) {
            if (objects[i] && objects[i]->hasTag(tag)) {
                results[found++] = objects[i].get();
            }
        }
        return found;
    }

    /**
     * @brief Apply function to all active objects
     * @param func Function to apply (takes Object* parameter)
     */
    void forEachActive(std::function<void(Object*)> func) {
        for (size_t i = 0; i < objectCount; ++i) {
            if (objects[i] && objects[i]->isActive()) {
                func(objects[i].get());
            }
        }
    }
    
    /**
     * @brief Clear all objects from the collection
     */
    void clear() {
        for (auto& obj : objects) {
            obj = nullptr;
        }
        objectCount = 0;
        initialized = false;
    }
    
    /**
     * @brief Get number of objects in collection
     * @return Object count
     */
    size_t size() const {
        return objectCount;
    }
    
    /**
     * @brief Check if collection is empty
     * @return True if empty
     */
    bool empty() const {
        return objectCount == 0;
    }
    
    /**
     * @brief Get object at index
     * @param index Object index
     * @return Pointer to object or nullptr if invalid index
     */
    Object* getObject(size_t index) {
        if (index >= objectCount) return nullptr;
        return objects[index].get();
    }
    
    /**
     * @brief Get const object at index
     * @param index Object index
     * @return Pointer to object or nullptr if invalid index
     */
    const Object* getObject(size_t index) const {
        if (index >= objectCount) return nullptr;
        return objects[index].get();
    }
    
    /**
     * @brief Remove inactive objects from the collection
     * 
     * This compacts the array by removing objects that are not active,
     * which can help with performance.
     */
    void removeInactiveObjects() {
        size_t writeIndex = 0;
        for (size_t readIndex = 0; readIndex < objectCount; ++readIndex) {
            if (objects[readIndex] && objects[readIndex]->isActive()) {
                if (writeIndex != readIndex) {
                    objects[writeIndex] = std::move(objects[readIndex]);
                }
                writeIndex++;
            }
        }
        
        // Clear remaining slots
        for (size_t i = writeIndex; i < objectCount; ++i) {
            objects[i] = nullptr;
        }
        
        objectCount = writeIndex;
    }
};

} // namespace enjin2