#pragma once

#include "../core/memory.hpp"
#include "../core/types.hpp"
#include <cstdint>
#include <type_traits>
#include <utility>
#include <algorithm>

namespace enjin2 {

/**
 * @brief Component ID type for type-safe component identification
 */
using ComponentID = uint32_t;

/**
 * @brief Base class for all components in the ECS system
 * 
 * Components are pure data containers without behavior.
 * This base class provides RTTI and basic lifecycle management.
 */
class ComponentBase {
public:
    /**
     * @brief Get unique component type ID
     * @return Component type identifier
     */
    virtual ComponentID getComponentID() const = 0;
    
    /**
     * @brief Virtual destructor for proper cleanup
     */
    virtual ~ComponentBase() = default;

protected:
    /**
     * @brief Generate unique component type ID
     * @tparam T Component type
     * @return Unique ID for component type T
     */
    template<typename T>
    static ComponentID getComponentTypeID() {
        static ComponentID id = nextComponentID++;
        return id;
    }

private:
    static ComponentID nextComponentID; ///< Counter for generating unique IDs
};

/**
 * @brief Template base for typed components
 * @tparam T Derived component type
 * 
 * Provides automatic type ID generation and type safety.
 */
template<typename T>
class Component : public ComponentBase {
public:
    /**
     * @brief Get component type ID for this component type
     * @return Component type identifier
     */
    ComponentID getComponentID() const override {
        return getComponentTypeID<T>();
    }
    
    /**
     * @brief Get static component type ID
     * @return Component type identifier
     */
    static ComponentID getStaticComponentID() {
        return getComponentTypeID<T>();
    }
};

/**
 * @brief Entity handle for referencing game objects
 * 
 * Entities are lightweight handles that reference collections of components.
 * Uses generation counters to detect stale references.
 */
struct Entity {
    uint32_t id : 24;        ///< Entity index (16M entities max)
    uint32_t generation : 8; ///< Generation counter (prevents stale references)
    
    /**
     * @brief Default constructor creates invalid entity
     */
    Entity() : id(0), generation(0) {}
    
    /**
     * @brief Constructor with ID and generation
     * @param id_ Entity ID
     * @param gen Generation counter
     */
    Entity(uint32_t id_, uint8_t gen) : id(id_), generation(gen) {}
    
    /**
     * @brief Check if entity handle is valid
     * @return true if entity is valid
     */
    bool isValid() const { return generation != 0; }
    
    /**
     * @brief Invalidate entity handle
     */
    void invalidate() { generation = 0; }
    
    /**
     * @brief Equality comparison
     * @param other Entity to compare with
     * @return true if both entities are identical
     */
    bool operator==(const Entity& other) const {
        return id == other.id && generation == other.generation;
    }

    /**
     * @brief Inequality comparison
     * @param other Entity to compare with
     * @return true if entities differ
     */
    bool operator!=(const Entity& other) const {
        return !(*this == other);
    }
};

/**
 * @brief Component storage using static memory pools
 * @tparam T Component type
 * @tparam CAPACITY Maximum number of components
 * 
 * Efficient storage for components with O(1) allocation/deallocation.
 * Uses packed arrays for cache-friendly iteration.
 */
template<typename T, size_t CAPACITY>
class ComponentStorage {
private:
    StaticPool<T, CAPACITY> pool;           ///< Object pool for components
    Entity entities[CAPACITY];              ///< Entity handles for each component
    uint32_t sparseToCompact[CAPACITY];     ///< Sparse to compact index mapping
    uint32_t compactToSparse[CAPACITY];     ///< Compact to sparse index mapping
    size_t compactCount;                    ///< Number of active components
    
public:
    /**
     * @brief Constructor initializes empty storage
     */
    ComponentStorage() : compactCount(0) {
        for (size_t i = 0; i < CAPACITY; ++i) {
            sparseToCompact[i] = UINT32_MAX;
            compactToSparse[i] = UINT32_MAX;
        }
    }
    
    /**
     * @brief Add component for entity
     * @param entity Entity to add component to
     * @param args Constructor arguments for component
     * @return Pointer to created component, nullptr if pool full
     */
    template<typename... Args>
    T* addComponent(Entity entity, Args&&... args) {
        if (compactCount >= CAPACITY) return nullptr;
        
        // Set up sparse/compact mapping
        uint32_t sparseIndex = entity.id % CAPACITY;
        uint32_t compactIndex = compactCount++;
        
        sparseToCompact[sparseIndex] = compactIndex;
        compactToSparse[compactIndex] = sparseIndex;
        entities[compactIndex] = entity;
        
        // For simplified implementation, store components in order
        // In practice, this would use the pool allocator properly
        static T componentArray[CAPACITY];
        T* component = &componentArray[compactIndex];
        
        // Construct component in-place
        new (component) T(std::forward<Args>(args)...);
        
        return component;
    }
    
    /**
     * @brief Get component for entity
     * @param entity Entity to get component for
     * @return Pointer to component, nullptr if not found
     */
    T* getComponent(Entity entity) const {
        uint32_t sparseIndex = entity.id % CAPACITY;
        uint32_t compactIndex = sparseToCompact[sparseIndex];
        
        if (compactIndex == UINT32_MAX || compactIndex >= compactCount) {
            return nullptr;
        }
        
        if (entities[compactIndex] != entity) {
            return nullptr;
        }
        
        // Access component from static array
        static T componentArray[CAPACITY];
        return &componentArray[compactIndex];
    }
    
    /**
     * @brief Remove component for entity
     * @param entity Entity to remove component from
     * @return true if component was removed
     */
    bool removeComponent(Entity entity) {
        uint32_t sparseIndex = entity.id % CAPACITY;
        uint32_t compactIndex = sparseToCompact[sparseIndex];
        
        if (compactIndex == UINT32_MAX || compactIndex >= compactCount) {
            return false;
        }
        
        if (entities[compactIndex] != entity) {
            return false;
        }
        
        // Get component and deallocate
        T* component = getComponent(entity);
        if (component) {
            pool.deallocate(component);
        }
        
        // Swap with last element for packed array
        uint32_t lastIndex = compactCount - 1;
        if (compactIndex != lastIndex) {
            entities[compactIndex] = entities[lastIndex];
            uint32_t movedSparseIndex = compactToSparse[lastIndex];
            sparseToCompact[movedSparseIndex] = compactIndex;
            compactToSparse[compactIndex] = movedSparseIndex;
        }
        
        // Clear removed entry
        sparseToCompact[sparseIndex] = UINT32_MAX;
        compactToSparse[lastIndex] = UINT32_MAX;
        compactCount--;
        
        return true;
    }
    
    /**
     * @brief Check if entity has component
     * @param entity Entity to check
     * @return true if entity has component
     */
    bool hasComponent(Entity entity) const {
        return getComponent(entity) != nullptr;
    }
    
    /**
     * @brief Get number of active components
     * @return Component count
     */
    size_t size() const { return compactCount; }
    
    /**
     * @brief Check if storage is empty
     * @return true if no components
     */
    bool empty() const { return compactCount == 0; }
    
    /**
     * @brief Iterator for efficient component iteration
     */
    class Iterator {
    private:
        const ComponentStorage* storage;
        size_t index;
        
    public:
        /**
         * @brief Construct iterator at position
         * @param s Storage to iterate
         * @param i Starting index
         */
        Iterator(const ComponentStorage* s, size_t i) : storage(s), index(i) {}

        /**
         * @brief Dereference iterator
         * @return Pair of entity and component pointer
         */
        std::pair<Entity, T*> operator*() const {
            return {storage->entities[index],
                    storage->getComponent(storage->entities[index])};
        }

        /// @brief Advance iterator
        /// @return Reference to this iterator
        Iterator& operator++() { ++index; return *this; }
        /**
         * @brief Inequality comparison
         * @param other Iterator to compare with
         * @return true if iterators differ
         */
        bool operator!=(const Iterator& other) const { return index != other.index; }
    };
    
    /**
     * @brief Get iterator to beginning
     * @return Iterator to first component
     */
    Iterator begin() const { return Iterator(this, 0); }

    /**
     * @brief Get iterator to end
     * @return Iterator past last component
     */
    Iterator end() const { return Iterator(this, compactCount); }
};

} // namespace enjin2