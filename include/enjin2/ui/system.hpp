#pragma once

#include "component.hpp"
#include "../core/types.hpp"
#include <array>
#include <functional>

namespace enjin2 {

/**
 * @brief System ID type for system identification
 */
using SystemID = uint32_t;

/**
 * @brief Base class for all systems in the ECS architecture
 * 
 * Systems contain the logic and operate on components.
 * They are stateless and process entities that have required components.
 */
class SystemBase {
public:
    /**
     * @brief Virtual destructor
     */
    virtual ~SystemBase() = default;
    
    /**
     * @brief Update system with delta time
     * @param dt Time since last update in seconds
     */
    virtual void update(float dt) = 0;
    
    /**
     * @brief Get system priority for update ordering
     * @return Priority value (lower = earlier execution)
     */
    virtual int getPriority() const { return 0; }
    
    /**
     * @brief Get unique system ID
     * @return System identifier
     */
    virtual SystemID getSystemID() const = 0;

protected:
    /**
     * @brief Generate unique system type ID
     * @tparam T System type
     * @return Unique ID for system type T
     */
    template<typename T>
    static SystemID getSystemTypeID() {
        static SystemID id = nextSystemID++;
        return id;
    }

private:
    static SystemID nextSystemID; ///< Counter for generating unique IDs
};

/**
 * @brief Template base for typed systems
 * @tparam T Derived system type
 */
template<typename T>
class System : public SystemBase {
public:
    /**
     * @brief Get system type ID
     * @return System type identifier
     */
    SystemID getSystemID() const override {
        return getSystemTypeID<T>();
    }
    
    /**
     * @brief Get static system type ID
     * @return System type identifier
     */
    static SystemID getStaticSystemID() {
        return getSystemTypeID<T>();
    }
};

/**
 * @brief Entity manager for creating and destroying entities
 * 
 * Manages entity lifecycle and component associations.
 * Uses generation counters to prevent accessing destroyed entities.
 */
class EntityManager {
private:
    static constexpr size_t MAX_ENTITIES = 4096;
    
    uint8_t generations[MAX_ENTITIES];  ///< Generation counters for each entity slot
    size_t freeList[MAX_ENTITIES];      ///< Free entity slot indices  
    size_t freeCount;                   ///< Number of free slots
    size_t entityCount;                 ///< Total active entities
    
public:
    /**
     * @brief Constructor initializes entity manager
     */
    EntityManager();
    
    /**
     * @brief Create new entity
     * @return Entity handle, invalid if no slots available
     */
    Entity createEntity();
    
    /**
     * @brief Destroy entity and free its slot
     * @param entity Entity to destroy
     */
    void destroyEntity(Entity entity);
    
    /**
     * @brief Check if entity is valid
     * @param entity Entity to validate
     * @return true if entity is valid
     */
    bool isValid(Entity entity) const;
    
    /**
     * @brief Get total number of active entities
     * @return Entity count
     */
    size_t getEntityCount() const { return entityCount; }
    
    /**
     * @brief Get maximum entity capacity
     * @return Maximum entities
     */
    size_t getMaxEntities() const { return MAX_ENTITIES; }
};

/**
 * @brief System manager for organizing and updating systems
 * @tparam MAX_SYSTEMS Maximum number of systems
 * 
 * Manages system lifecycle and provides ordered updating.
 * Systems are automatically sorted by priority.
 */
template<size_t MAX_SYSTEMS = 32>
class SystemManager {
private:
    std::array<SystemBase*, MAX_SYSTEMS> systems;   ///< System pointers
    std::array<SystemID, MAX_SYSTEMS> systemIDs;    ///< System type IDs
    size_t systemCount;                              ///< Number of active systems
    bool needsSort;                                  ///< Flag indicating sort needed
    
public:
    /**
     * @brief Constructor initializes empty system manager
     */
    SystemManager() : systemCount(0), needsSort(false) {
        systems.fill(nullptr);
        systemIDs.fill(0);
    }
    
    /**
     * @brief Add system to manager
     * @tparam T System type
     * @param system System instance
     * @return true if system was added successfully
     */
    template<typename T>
    bool addSystem(T* system) {
        if (systemCount >= MAX_SYSTEMS) return false;
        
        SystemID id = T::getStaticSystemID();
        
        // Check for duplicate system
        for (size_t i = 0; i < systemCount; ++i) {
            if (systemIDs[i] == id) return false;
        }
        
        systems[systemCount] = system;
        systemIDs[systemCount] = id;
        systemCount++;
        needsSort = true;
        
        return true;
    }
    
    /**
     * @brief Remove system from manager
     * @tparam T System type
     * @return Pointer to removed system, nullptr if not found
     */
    template<typename T>
    T* removeSystem() {
        SystemID id = T::getStaticSystemID();
        
        for (size_t i = 0; i < systemCount; ++i) {
            if (systemIDs[i] == id) {
                T* system = static_cast<T*>(systems[i]);
                
                // Shift remaining systems down
                for (size_t j = i; j < systemCount - 1; ++j) {
                    systems[j] = systems[j + 1];
                    systemIDs[j] = systemIDs[j + 1];
                }
                
                systems[systemCount - 1] = nullptr;
                systemIDs[systemCount - 1] = 0;
                systemCount--;
                
                return system;
            }
        }
        
        return nullptr;
    }
    
    /**
     * @brief Get system by type
     * @tparam T System type
     * @return Pointer to system, nullptr if not found
     */
    template<typename T>
    T* getSystem() {
        SystemID id = T::getStaticSystemID();
        
        for (size_t i = 0; i < systemCount; ++i) {
            if (systemIDs[i] == id) {
                return static_cast<T*>(systems[i]);
            }
        }
        
        return nullptr;
    }
    
    /**
     * @brief Update all systems in priority order
     * @param dt Time since last update in seconds
     */
    void update(float dt) {
        if (needsSort) {
            sortSystems();
            needsSort = false;
        }

        for (size_t i = 0; i < systemCount; ++i) {
            if (systems[i]) {
                systems[i]->update(dt);
            }
        }
    }
    
    /**
     * @brief Get number of active systems
     * @return System count
     */
    size_t getSystemCount() const { return systemCount; }

private:
    /**
     * @brief Sort systems by priority (bubble sort for small arrays)
     */
    void sortSystems() {
        for (size_t i = 0; i < systemCount - 1; ++i) {
            for (size_t j = 0; j < systemCount - i - 1; ++j) {
                if (systems[j] && systems[j + 1] && 
                    systems[j]->getPriority() > systems[j + 1]->getPriority()) {
                    std::swap(systems[j], systems[j + 1]);
                    std::swap(systemIDs[j], systemIDs[j + 1]);
                }
            }
        }
    }
};

/**
 * @brief Query builder for component-based entity selection
 * @tparam Components... Component types to query for
 * 
 * Provides efficient iteration over entities with specific component combinations.
 */
template<typename... Components>
class ComponentQuery {
private:
    std::function<bool(Entity)> filter; ///< Entity filter function
    
public:
    /**
     * @brief Constructor with entity filter
     * @param entityFilter Function to test if entity matches query
     */
    ComponentQuery(std::function<bool(Entity)> entityFilter) 
        : filter(entityFilter) {}
    
    /**
     * @brief Iterator for query results
     */
    class Iterator {
    private:
        std::function<bool(Entity)> filter;
        Entity currentEntity;
        size_t entityIndex;
        
    public:
        /**
         * @brief Construct iterator with filter and starting position
         * @param f Entity filter function
         * @param start Starting entity
         * @param index Starting entity index
         */
        Iterator(std::function<bool(Entity)> f, Entity start, size_t index)
            : filter(f), currentEntity(start), entityIndex(index) {
            findNext();
        }

        /// @brief Dereference to get current entity
        /// @return Current entity
        Entity operator*() const { return currentEntity; }

        /// @brief Advance to next matching entity
        /// @return Reference to this iterator
        Iterator& operator++() {
            entityIndex++;
            findNext();
            return *this;
        }
        
        /// @brief Inequality comparison
        /// @param other Iterator to compare with
        /// @return True if iterators are at different positions
        bool operator!=(const Iterator& other) const {
            return entityIndex != other.entityIndex;
        }
        
    private:
        void findNext() {
            // This would need integration with EntityManager
            // Implementation depends on how entities are stored
        }
    };
    
    /**
     * @brief Get iterator to beginning of query results
     * @return Iterator to first matching entity
     */
    Iterator begin() const {
        return Iterator(filter, Entity(), 0);
    }
    
    /**
     * @brief Get iterator to end of query results
     * @return Past-the-end iterator
     */
    Iterator end() const {
        return Iterator(filter, Entity(), SIZE_MAX);
    }
};

} // namespace enjin2