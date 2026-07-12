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
 * @brief Packed component storage backed by a sparse set
 * @tparam T Component type (must be default-constructible and move-assignable)
 * @tparam CAPACITY Maximum number of live components *and* the entity-id space
 *
 * Data lives in a real, per-instance member array (@ref components_) rather than
 * a shared function-local static, so distinct storages never alias. Lookup is a
 * classic sparse set: @ref sparse_ maps an entity's id directly to its compact
 * slot (O(1), no hashing), and removal swaps the last element into the freed slot
 * to keep the packed array contiguous for cache-friendly iteration.
 *
 * The entity id is used as a direct index into @ref sparse_, so ids must satisfy
 * `id < CAPACITY`; @ref World sizes its EntityManager to the same CAPACITY to
 * guarantee this. Out-of-range ids are rejected rather than silently wrapped.
 */
template<typename T, size_t CAPACITY>
class ComponentStorage {
private:
    static constexpr uint32_t kInvalid = UINT32_MAX; ///< "no slot" sentinel

    T components_[CAPACITY];        ///< Packed component data (compact-indexed)
    Entity entities_[CAPACITY];     ///< Entity owning each compact slot
    uint32_t sparse_[CAPACITY];     ///< entity.id -> compact index (kInvalid = absent)
    size_t compactCount_;           ///< Number of active components

    /// @brief True if a live component for @p entity sits at @p idx.
    bool matches(uint32_t idx, Entity entity) const {
        return idx != kInvalid && idx < compactCount_ && entities_[idx] == entity;
    }

public:
    /**
     * @brief Constructor initializes empty storage
     */
    ComponentStorage() : compactCount_(0) {
        for (size_t i = 0; i < CAPACITY; ++i) {
            sparse_[i] = kInvalid;
        }
    }

    /**
     * @brief Add (or replace) the component for an entity
     * @param entity Entity to attach the component to (id must be < CAPACITY)
     * @param args Constructor arguments forwarded to T
     * @return Pointer to the stored component, or nullptr if id is out of range
     *         or the storage is full
     */
    template<typename... Args>
    T* addComponent(Entity entity, Args&&... args) {
        if (entity.id >= CAPACITY) return nullptr;

        // Replace in place if this entity already has the component.
        uint32_t existing = sparse_[entity.id];
        if (matches(existing, entity)) {
            components_[existing] = T(std::forward<Args>(args)...);
            return &components_[existing];
        }

        if (compactCount_ >= CAPACITY) return nullptr;

        uint32_t compactIndex = static_cast<uint32_t>(compactCount_++);
        sparse_[entity.id] = compactIndex;
        entities_[compactIndex] = entity;
        components_[compactIndex] = T(std::forward<Args>(args)...);
        return &components_[compactIndex];
    }

    /**
     * @brief Get the component for an entity
     * @param entity Entity to look up
     * @return Pointer to the component, or nullptr if absent / stale / out of range
     */
    T* getComponent(Entity entity) {
        if (entity.id >= CAPACITY) return nullptr;
        uint32_t idx = sparse_[entity.id];
        return matches(idx, entity) ? &components_[idx] : nullptr;
    }

    /// @brief Const overload of @ref getComponent.
    const T* getComponent(Entity entity) const {
        if (entity.id >= CAPACITY) return nullptr;
        uint32_t idx = sparse_[entity.id];
        return matches(idx, entity) ? &components_[idx] : nullptr;
    }

    /**
     * @brief Remove the component for an entity
     * @param entity Entity to detach the component from
     * @return true if a component was removed
     */
    bool removeComponent(Entity entity) {
        if (entity.id >= CAPACITY) return false;
        uint32_t idx = sparse_[entity.id];
        if (!matches(idx, entity)) return false;

        // Swap the last packed element into the freed slot to stay contiguous.
        uint32_t last = static_cast<uint32_t>(compactCount_ - 1);
        if (idx != last) {
            components_[idx] = std::move(components_[last]);
            entities_[idx] = entities_[last];
            sparse_[entities_[idx].id] = idx; // repoint the moved entity
        }

        sparse_[entity.id] = kInvalid;
        compactCount_--;
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
    size_t size() const { return compactCount_; }

    /**
     * @brief Check if storage is empty
     * @return true if no components
     */
    bool empty() const { return compactCount_ == 0; }

    /**
     * @brief Entity owning the component at a packed index (unchecked)
     * @param i Packed index in [0, size())
     * @return Entity handle stored at that slot
     */
    Entity entityAt(size_t i) const { return entities_[i]; }

    /**
     * @brief Pointer to the contiguous packed entity array
     * @return Base of the [0, size()) entity span (for queries); never null
     */
    const Entity* entityData() const { return entities_; }

    /**
     * @brief Component at a packed index (unchecked)
     * @param i Packed index in [0, size())
     * @return Reference to the mutable component at that slot
     */
    T& componentAt(size_t i) { return components_[i]; }

    /**
     * @brief Component at a packed index (unchecked, const overload)
     * @param i Packed index in [0, size())
     * @return Const reference to the component at that slot
     */
    const T& componentAt(size_t i) const { return components_[i]; }

    /**
     * @brief Iterator over the packed component range
     *
     * Dereferences to `{Entity, T*}`, exposing mutable component data directly
     * from the packed array (no per-step sparse lookup).
     */
    class Iterator {
    private:
        ComponentStorage* storage_;
        size_t index_;

    public:
        /**
         * @brief Construct iterator at position
         * @param s Storage to iterate
         * @param i Starting packed index
         */
        Iterator(ComponentStorage* s, size_t i) : storage_(s), index_(i) {}

        /**
         * @brief Dereference iterator
         * @return Pair of entity and mutable component pointer
         */
        std::pair<Entity, T*> operator*() const {
            return {storage_->entities_[index_], &storage_->components_[index_]};
        }

        /// @brief Advance iterator
        /// @return Reference to this iterator
        Iterator& operator++() { ++index_; return *this; }

        /**
         * @brief Inequality comparison
         * @param other Iterator to compare with
         * @return true if iterators differ
         */
        bool operator!=(const Iterator& other) const { return index_ != other.index_; }
    };

    /**
     * @brief Get iterator to beginning
     * @return Iterator to first component
     */
    Iterator begin() { return Iterator(this, 0); }

    /**
     * @brief Get iterator past the last component
     * @return Iterator past last component
     */
    Iterator end() { return Iterator(this, compactCount_); }
};

} // namespace enjin2