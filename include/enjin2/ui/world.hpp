#pragma once

#include "component.hpp"
#include "system.hpp"
#include <tuple>
#include <cstddef>
#include <functional>
#include <utility>

namespace enjin2 {

/// @brief Value-free type marker handed to @ref World::forEachComponentType visitors.
template<typename T>
struct TypeTag {
    using type = T;
};

/**
 * @brief Minimal fixed-capacity ECS registry for the ui module
 * @tparam CAPACITY Entity-id space and per-component storage capacity
 * @tparam Components... Component types this world holds (must be distinct)
 *
 * A World is the connective tissue the ui systems were missing: it owns one
 * @ref EntityManager and one @ref ComponentStorage per component type, and lets
 * callers create entities, attach/detach components, and iterate entities that
 * hold a given combination via @ref query.
 *
 * Everything is statically sized — no dynamic allocation — so a World is a plain
 * value type suitable for embedded targets. CAPACITY bounds both the number of
 * live entities and the entity-id space, which keeps @ref ComponentStorage's
 * sparse map a direct id index (ids are always `< CAPACITY`).
 *
 * The component set is a template parameter rather than a fixed global, so a
 * feature can compose exactly the world it needs (e.g. a list-widget context vs.
 * a full-screen scene) without a single monolithic registry growing unbounded.
 *
 * @note A query borrows a pointer into a component storage; do not add or remove
 *       components of the queried primary type while iterating it.
 */
template<size_t CAPACITY, typename... Components>
class World {
    static_assert(sizeof...(Components) > 0, "World must hold at least one component type");

private:
    EntityManager<CAPACITY> entities_;
    std::tuple<ComponentStorage<Components, CAPACITY>...> storages_;

    /// @brief Access the storage for component type @p T (by unique tuple type).
    template<typename T>
    ComponentStorage<T, CAPACITY>& storage() {
        return std::get<ComponentStorage<T, CAPACITY>>(storages_);
    }
    template<typename T>
    const ComponentStorage<T, CAPACITY>& storage() const {
        return std::get<ComponentStorage<T, CAPACITY>>(storages_);
    }

    /// @brief True if @p e holds every one of @p Ts (vacuously true for none).
    template<typename... Ts>
    bool hasAll(Entity e) const {
        return (has<Ts>(e) && ...);
    }

public:
    /// @brief Entity-id space and per-storage capacity.
    static constexpr size_t kCapacity = CAPACITY;

    /**
     * @brief Invoke @p f with a @ref TypeTag for every component type this world composes
     *
     * The pack is otherwise unnamable from outside; serializers walk it to
     * discover which component types a world can hold (unwn #182).
     */
    template<typename F>
    static constexpr void forEachComponentType(F&& f) {
        (f(TypeTag<Components>{}), ...);
    }

    /**
     * @brief Create a new entity
     * @return A valid entity handle, or an invalid one if the world is full
     */
    Entity create() { return entities_.createEntity(); }

    /**
     * @brief Destroy an entity and detach all of its components
     * @param e Entity to destroy
     */
    void destroy(Entity e) {
        if (!entities_.isValid(e)) return;
        (storage<Components>().removeComponent(e), ...); // drop every component
        entities_.destroyEntity(e);
    }

    /**
     * @brief Check whether an entity handle is still live
     * @param e Entity to validate
     * @return true if the handle refers to a live entity
     */
    bool valid(Entity e) const { return entities_.isValid(e); }

    /**
     * @brief Number of live entities
     * @return Active entity count
     */
    size_t entityCount() const { return entities_.getEntityCount(); }

    /**
     * @brief Attach (or replace) a component on an entity
     * @tparam T Component type
     * @param e Entity to attach to (must be live)
     * @param args Constructor arguments forwarded to T
     * @return Pointer to the stored component, or nullptr if @p e is invalid / storage full
     */
    template<typename T, typename... Args>
    T* add(Entity e, Args&&... args) {
        if (!entities_.isValid(e)) return nullptr;
        return storage<T>().addComponent(e, std::forward<Args>(args)...);
    }

    /**
     * @brief Get a mutable component pointer for an entity
     * @tparam T Component type
     * @param e Entity to look up
     * @return Pointer to the component, or nullptr if absent
     */
    template<typename T>
    T* get(Entity e) { return storage<T>().getComponent(e); }

    /**
     * @brief Get a const component pointer for an entity
     * @tparam T Component type
     * @param e Entity to look up
     * @return Const pointer to the component, or nullptr if absent
     */
    template<typename T>
    const T* get(Entity e) const { return storage<T>().getComponent(e); }

    /**
     * @brief Check whether an entity holds a component
     * @tparam T Component type
     * @param e Entity to check
     * @return true if @p e has a component of type @p T
     */
    template<typename T>
    bool has(Entity e) const { return storage<T>().hasComponent(e); }

    /**
     * @brief Detach a component from an entity (no-op if absent)
     * @tparam T Component type
     * @param e Entity to detach from
     */
    template<typename T>
    void remove(Entity e) { storage<T>().removeComponent(e); }

    /**
     * @brief Direct access to the packed storage for a component type
     * @tparam T Component type
     * @return Reference to the ComponentStorage holding all T components
     */
    template<typename T>
    ComponentStorage<T, CAPACITY>& components() { return storage<T>(); }

    /**
     * @brief Direct access to the packed storage for a component type (const)
     * @tparam T Component type
     * @return Const reference to the ComponentStorage holding all T components
     */
    template<typename T>
    const ComponentStorage<T, CAPACITY>& components() const { return storage<T>(); }

    /**
     * @brief Build a query over entities holding all of @p First, @p Rest...
     * @tparam First Primary component whose packed storage drives iteration
     * @tparam Rest Additional components each matched entity must also hold
     * @return A @ref ComponentQuery iterable with range-for
     *
     * The primary storage supplies the candidate set (so only entities that have
     * @p First are ever visited), and the returned query filters those down to
     * the ones that also hold every type in @p Rest.
     */
    template<typename First, typename... Rest>
    ComponentQuery<First, Rest...> query() {
        auto& primary = storage<First>();
        World* self = this;
        std::function<bool(Entity)> filter = [self](Entity e) {
            return self->hasAll<Rest...>(e);
        };
        return ComponentQuery<First, Rest...>(primary.entityData(), primary.size(),
                                              std::move(filter));
    }
};

} // namespace enjin2
