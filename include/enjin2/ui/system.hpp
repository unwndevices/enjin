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
template<size_t MAX_ENTITIES = 4096>
class EntityManager {
private:
    uint8_t generations[MAX_ENTITIES];  ///< Generation counters for each entity slot
    size_t freeList[MAX_ENTITIES];      ///< Free entity slot indices
    size_t freeCount;                   ///< Number of free slots
    size_t entityCount;                 ///< Total active entities

public:
    /// @brief Maximum number of entity slots (the entity-id space).
    static constexpr size_t kMaxEntities = MAX_ENTITIES;

    /**
     * @brief Constructor initializes entity manager
     */
    EntityManager() : freeCount(MAX_ENTITIES), entityCount(0) {
        // Generation counters start at 1 (0 is reserved for the invalid entity).
        for (size_t i = 0; i < MAX_ENTITIES; ++i) {
            generations[i] = 1;
            freeList[i] = i;
        }
    }

    /**
     * @brief Create new entity
     * @return Entity handle, invalid if no slots available
     */
    Entity createEntity() {
        if (freeCount == 0) {
            return Entity(); // Invalid entity
        }
        size_t index = freeList[--freeCount];
        entityCount++;
        return Entity(static_cast<uint32_t>(index), generations[index]);
    }

    /**
     * @brief Destroy entity and free its slot
     * @param entity Entity to destroy
     */
    void destroyEntity(Entity entity) {
        if (!isValid(entity)) return;
        size_t index = entity.id;

        // Bump the generation so existing handles to this slot become stale.
        generations[index]++;
        if (generations[index] == 0) {
            generations[index] = 1; // Skip 0 (invalid)
        }

        freeList[freeCount++] = index;
        entityCount--;
    }

    /**
     * @brief Check if entity is valid
     * @param entity Entity to validate
     * @return true if entity is valid
     */
    bool isValid(Entity entity) const {
        if (entity.id >= MAX_ENTITIES) return false;
        return generations[entity.id] == entity.generation;
    }

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
 * @brief Lazily filtered view over a span of entities
 * @tparam Components... Component types this query represents (documentation only)
 *
 * A query scans a contiguous span of candidate entities (typically the packed
 * entity list of the smallest component's storage) and yields only those that
 * satisfy a predicate — usually "has every queried component". The predicate is
 * supplied by @ref World::query, which knows how to test component membership.
 *
 * Iteration is O(span) with the predicate evaluated once per candidate; nothing
 * is materialised, so a query is cheap to construct and safe to range-for over.
 */
template<typename... Components>
class ComponentQuery {
private:
    const Entity* entities_;             ///< Candidate span (borrowed, not owned)
    size_t count_;                       ///< Number of candidates in the span
    std::function<bool(Entity)> filter_; ///< Predicate an entity must satisfy

public:
    /**
     * @brief Construct a query over an entity span
     * @param entities Pointer to the first candidate entity (may be null if count is 0)
     * @param count Number of candidate entities
     * @param filter Predicate returning true for entities that match the query
     */
    ComponentQuery(const Entity* entities, size_t count, std::function<bool(Entity)> filter)
        : entities_(entities), count_(count), filter_(std::move(filter)) {}

    /**
     * @brief Forward iterator that skips non-matching entities
     */
    class Iterator {
    private:
        const Entity* entities_;                 ///< Candidate span
        size_t count_;                           ///< Span length
        const std::function<bool(Entity)>* filter_; ///< Predicate (borrowed from the query)
        size_t index_;                           ///< Current position in the span

        /**
         * @brief Advance @ref index_ to the next matching entity (or to the end)
         *
         * Scans forward from the current position, skipping entities the predicate
         * rejects, and stops on the first match or when the span is exhausted.
         */
        void findNext() {
            while (index_ < count_ && !(*filter_)(entities_[index_])) {
                ++index_;
            }
        }

    public:
        /**
         * @brief Construct iterator at a position and settle on the next match
         * @param entities Candidate span
         * @param count Span length
         * @param filter Predicate (borrowed; must outlive the iterator)
         * @param index Starting index into the span
         */
        Iterator(const Entity* entities, size_t count,
                 const std::function<bool(Entity)>* filter, size_t index)
            : entities_(entities), count_(count), filter_(filter), index_(index) {
            findNext();
        }

        /// @brief Dereference to the current matching entity
        /// @return Current entity
        Entity operator*() const { return entities_[index_]; }

        /// @brief Advance to the next matching entity
        /// @return Reference to this iterator
        Iterator& operator++() {
            ++index_;
            findNext();
            return *this;
        }

        /// @brief Inequality comparison
        /// @param other Iterator to compare with
        /// @return True if iterators are at different positions
        bool operator!=(const Iterator& other) const {
            return index_ != other.index_;
        }
    };

    /**
     * @brief Get iterator to the first matching entity
     * @return Iterator to first match
     */
    Iterator begin() const {
        return Iterator(entities_, count_, &filter_, 0);
    }

    /**
     * @brief Get past-the-end iterator
     * @return Past-the-end iterator
     */
    Iterator end() const {
        return Iterator(entities_, count_, &filter_, count_);
    }
};

} // namespace enjin2