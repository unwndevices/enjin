#pragma once

#include "component.hpp"
#include "json.hpp"
#include "reflect.hpp"
#include <string>
#include <utility>

/**
 * @file slot.hpp
 * @brief `cpp:` slot + bindings components for scene-file entities (unwn #183, M2)
 *
 * The ratified schema (ADR-0005) keeps everything Turing-complete in C++
 * behind two seams: named host effects and **`cpp:` slots** — compiled-in
 * visualizations registered by string ID, configured from the scene file
 * through an opaque property bag. The engine round-trips the bag verbatim and
 * lets behavior data write into it; *drawing* a slot is the host's job (the
 * v1 slot registry is scoped to M3).
 *
 * BindingsComponent carries the entity's declarative property bindings
 * (`"visible": "showMiniPreview"`): a JSON object mapping a property name on
 * this entity to a state expression, re-evaluated by the SceneVM after every
 * dispatch/tick.
 */

namespace enjin2 {

/**
 * @brief A named `cpp:` visualization slot with its property bag
 *
 * `slot` is the registry ID (file form `cpp:<id>`, e.g. `cpp:OrbitPlayhead`
 * is stored as id `OrbitPlayhead`). `props` is an opaque JSON object the host
 * renderer interprets; the SceneVM resolves `entity.prop` paths on a slot
 * entity into this bag, so bindings and `set` actions reach slot properties
 * exactly like reflected widget fields.
 */
struct SlotComponent : public Component<SlotComponent> {
    std::string slot; ///< Registry ID the host resolves to a compiled renderer
    JsonValue props;  ///< Opaque property bag (JSON object), host-interpreted

    SlotComponent(std::string slot_ = {}) : slot(std::move(slot_)) {
        props.type = JsonValue::Type::Object;
    }
};

/// @brief Serializable properties of @ref SlotComponent (see reflect.hpp).
#define ENJIN2_SLOT_COMPONENT_FIELDS(FIELD, PROP) \
    FIELD(slot)                                   \
    FIELD(props)

ENJIN2_REFLECT_COMPONENT(SlotComponent, 11, "slot", ENJIN2_SLOT_COMPONENT_FIELDS)

/**
 * @brief Declarative property bindings for one entity
 *
 * A JSON object mapping property name → expression. `visible` binds through
 * the guard grammar (a condition); every other property binds as a value
 * lookup (state var or `entity.prop`). Bindings are scene data, not runtime
 * state — they serialize with the entity and are applied by the SceneVM.
 */
struct BindingsComponent : public Component<BindingsComponent> {
    JsonValue bindings; ///< JSON object: property name → expression string

    BindingsComponent() { bindings.type = JsonValue::Type::Object; }
};

/// @brief Serializable properties of @ref BindingsComponent (see reflect.hpp).
#define ENJIN2_BINDINGS_COMPONENT_FIELDS(FIELD, PROP) \
    FIELD(bindings)

ENJIN2_REFLECT_COMPONENT(BindingsComponent, 12, "bindings", ENJIN2_BINDINGS_COMPONENT_FIELDS)

} // namespace enjin2
