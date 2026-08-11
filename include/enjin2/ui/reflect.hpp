#pragma once

#include <cstdint>
#include <type_traits>

/**
 * @file reflect.hpp
 * @brief Stable component identity + field reflection for the ui ECS (G1+G2)
 *
 * Every serializable component declares one field-list macro next to its struct
 * and registers it with @ref ENJIN2_REFLECT_COMPONENT. That single declaration
 * drives save, load and (later) the inspector — the schema/engine-drift
 * mitigation locked in unwn ADR-0005.
 *
 * Identity is a `constexpr` name string plus an explicit numeric type ID. The
 * JSON scene format keys components by **name**; the numeric IDs are the
 * stable identity for non-textual consumers (the packed-binary export held in
 * reserve by ADR-0005, factory tables). **Both are permanent**: never reused,
 * never renumbered, a retired type's slot never reassigned. The registry
 * covers every serializable type, ECS component or not (Theme is a plain
 * aggregate). Allocated so far (append-only):
 *
 *   1 position · 2 size · 3 theme · 4 label · 5 icon · 6 gauge ·
 *   7 overlay · 8 popup · 9 list · 10 id · 11 slot · 12 bindings
 *
 * (The runtime counter in `Component<T>::getStaticComponentID()` is link-order
 * dependent and stays a process-local detail; persistent identity lives here.)
 */

namespace enjin2 {

/// @brief Persistent component type ID, as written into scene files.
using ComponentTypeId = uint32_t;

/**
 * @brief Reflection traits for a component type
 *
 * Only reflected types specialize this (via @ref ENJIN2_REFLECT_COMPONENT); the
 * primary template stays undefined so use of an unreflected type is a compile
 * error. A specialization provides `kTypeId`, `kName` and two `visitFields`
 * overloads (mutable and const) that call the visitor once per serializable
 * property, in declaration order.
 */
template<typename T>
struct ComponentTraits;

/// @brief Detects whether @p T has been registered with ENJIN2_REFLECT_COMPONENT.
template<typename T, typename = void>
struct IsReflected : std::false_type {};

template<typename T>
struct IsReflected<T, std::void_t<decltype(ComponentTraits<T>::kName)>> : std::true_type {};

// ----- Accessors handed to visitFields visitors -------------------------------
//
// Every visited property is presented uniformly as an accessor with a
// `value_type`, a `get()`, and — on mutable visitation — a `set()`. Plain data
// members arrive as (Const)FieldRef; private state exposed through a
// getter/setter pair arrives as (Const)PropRef.

/// @brief Mutable accessor for a public data member.
template<typename ValueT>
struct FieldRef {
    using value_type = ValueT;
    ValueT* p;
    const ValueT& get() const { return *p; }
    void set(const ValueT& v) const { *p = v; }
};

/// @brief Read-only accessor for a public data member.
template<typename ValueT>
struct ConstFieldRef {
    using value_type = ValueT;
    const ValueT* p;
    const ValueT& get() const { return *p; }
};

/// @brief Mutable accessor for a getter/setter property (private state).
template<typename ValueT, typename C>
struct PropRef {
    using value_type = ValueT;
    C* c;
    ValueT (C::*getter)() const;
    void (C::*setter)(ValueT);
    ValueT get() const { return (c->*getter)(); }
    void set(ValueT v) const { (c->*setter)(v); }
};

/// @brief Read-only accessor for a getter/setter property.
template<typename ValueT, typename C>
struct ConstPropRef {
    using value_type = ValueT;
    const C* c;
    ValueT (C::*getter)() const;
    ValueT get() const { return (c->*getter)(); }
};

// ----- Field-list entry macros ------------------------------------------------
//
// A component's field list is a macro taking (FIELD, PROP) and expanding one
// entry per serializable property:
//
//   #define ENJIN2_GAUGE_COMPONENT_FIELDS(FIELD, PROP) \
//       FIELD(diameter)                                \
//       FIELD(rimColor)                                \
//       FIELD(mode)                                    \
//       PROP(value, float, value, setValue)
//
// FIELD(member) names a public data member; PROP(name, ValueT, getter, setter)
// exposes private state through its accessor pair. Entry order is visitation
// order — keep dependent properties after what they depend on (a gauge's value
// clamps against its mode; a list's selection clamps against its items).

#define ENJIN2_REFLECT_FIELD_MUT(m) \
    v(#m, ::enjin2::FieldRef<std::remove_reference_t<decltype(c.m)>>{&c.m});
#define ENJIN2_REFLECT_PROP_MUT(name, ValueT, getter, setter) \
    v(#name, ::enjin2::PropRef<ValueT, Comp>{&c, &Comp::getter, &Comp::setter});
#define ENJIN2_REFLECT_FIELD_CONST(m) \
    v(#m, ::enjin2::ConstFieldRef<std::remove_reference_t<decltype(c.m)>>{&c.m});
#define ENJIN2_REFLECT_PROP_CONST(name, ValueT, getter, setter) \
    v(#name, ::enjin2::ConstPropRef<ValueT, Comp>{&c, &Comp::getter});

/**
 * @brief Register a component type for reflection
 * @param TypeName The component type (in namespace enjin2)
 * @param Id Permanent numeric type ID (see the allocation table above)
 * @param NameStr Stable name string, as written into scene files
 * @param LIST The component's field-list macro
 *
 * Invoke at namespace scope (namespace enjin2), after the struct definition,
 * in the same header — the field list and the registration live beside the
 * struct they describe.
 */
#define ENJIN2_REFLECT_COMPONENT(TypeName, Id, NameStr, LIST)                 \
    template<>                                                                \
    struct ComponentTraits<TypeName> {                                        \
        using Comp = TypeName;                                                \
        static constexpr ComponentTypeId kTypeId = Id;                        \
        static constexpr const char* kName = NameStr;                         \
        template<typename V>                                                  \
        static void visitFields(TypeName& c, V&& v) {                         \
            LIST(ENJIN2_REFLECT_FIELD_MUT, ENJIN2_REFLECT_PROP_MUT)           \
        }                                                                     \
        template<typename V>                                                  \
        static void visitFields(const TypeName& c, V&& v) {                   \
            LIST(ENJIN2_REFLECT_FIELD_CONST, ENJIN2_REFLECT_PROP_CONST)       \
        }                                                                     \
    };

} // namespace enjin2
