#pragma once

#include "asset_registry.hpp"
#include "json.hpp"
#include "reflect.hpp"
#include "theme.hpp"
#include "world.hpp"
#include "../core/types.hpp"
#include <algorithm>
#include <cstring>
#include <string>
#include <type_traits>
#include <vector>

/**
 * @file scene_json.hpp
 * @brief Scene ⇄ JSON serialization over the reflection layer (unwn #182, M1)
 *
 * A scene file is versioned data (ADR-0005): a `version` field, an optional
 * root `theme`, and an `entities` array whose components are **name-keyed** by
 * their @ref ComponentTraits identity. Save and load are both driven by the
 * per-component field lists — one declaration per component, no parallel
 * schema to drift.
 *
 * The reader is tolerant from day one: unknown component names, unknown
 * fields, and wrong-typed values are skipped (defaults stand); the `version`
 * field is carried for future readers, not enforced. The field-level shape is
 * v1-provisional per the locked spec — a shape change is a version bump.
 *
 * Asset pointers (`const GFXfont*`, icon bitmaps) serialize as name references
 * through the @ref AssetRegistry; an unregistered pointer writes `null` and a
 * dangling name resolves to `nullptr` (the widget's "no asset" state).
 */

namespace enjin2 {

/// @brief Scene file format version written by writeSceneJson.
inline constexpr int64_t kSceneJsonVersion = 1;

namespace detail {

// ----- Field value codecs -----------------------------------------------------
//
// One write/read pair per reflected value type. Reads return false (leaving
// the caller's default untouched) on a type mismatch — the tolerant-reader
// contract.

template<typename T>
void writeFieldValue(JsonWriter& w, const T& v, const AssetRegistry& assets) {
    if constexpr (std::is_same_v<T, bool>) {
        w.value(v);
    } else if constexpr (std::is_same_v<T, std::string>) {
        w.value(v);
    } else if constexpr (std::is_same_v<T, float>) {
        w.value(v);
    } else if constexpr (std::is_enum_v<T>) {
        w.value(static_cast<int64_t>(static_cast<std::underlying_type_t<T>>(v)));
    } else if constexpr (std::is_integral_v<T>) {
        w.value(static_cast<int64_t>(v));
    } else if constexpr (std::is_same_v<T, Pixel4>) {
        w.value(static_cast<int64_t>(v.value));
    } else if constexpr (std::is_same_v<T, Point>) {
        w.beginObject();
        w.key("x");
        w.value(static_cast<int64_t>(v.x));
        w.key("y");
        w.value(static_cast<int64_t>(v.y));
        w.endObject();
    } else if constexpr (std::is_same_v<T, Size>) {
        w.beginObject();
        w.key("width");
        w.value(static_cast<int64_t>(v.width));
        w.key("height");
        w.value(static_cast<int64_t>(v.height));
        w.endObject();
    } else if constexpr (std::is_same_v<T, std::vector<std::string>>) {
        w.beginArray();
        for (const std::string& s : v) w.value(s);
        w.endArray();
    } else if constexpr (std::is_same_v<T, const GFXfont*>) {
        const char* name = v ? assets.fontName(v) : nullptr;
        if (name) w.value(name);
        else w.null();
    } else if constexpr (std::is_same_v<T, const uint8_t*>) {
        const char* name = v ? assets.bitmapName(v) : nullptr;
        if (name) w.value(name);
        else w.null();
    } else {
        static_assert(sizeof(T) == 0, "unserializable reflected field type");
    }
}

template<typename T>
bool readFieldValue(const JsonValue& jv, T& out, const AssetRegistry& assets) {
    if constexpr (std::is_same_v<T, bool>) {
        if (jv.type != JsonValue::Type::Bool) return false;
        out = jv.boolean;
    } else if constexpr (std::is_same_v<T, std::string>) {
        if (jv.type != JsonValue::Type::String) return false;
        out = jv.str;
    } else if constexpr (std::is_same_v<T, float>) {
        if (jv.type != JsonValue::Type::Number) return false;
        out = static_cast<float>(jv.number);
    } else if constexpr (std::is_enum_v<T>) {
        if (jv.type != JsonValue::Type::Number) return false;
        out = static_cast<T>(static_cast<std::underlying_type_t<T>>(jv.number));
    } else if constexpr (std::is_integral_v<T>) {
        if (jv.type != JsonValue::Type::Number) return false;
        out = static_cast<T>(static_cast<int64_t>(jv.number));
    } else if constexpr (std::is_same_v<T, Pixel4>) {
        if (jv.type != JsonValue::Type::Number) return false;
        out = Pixel4(static_cast<uint8_t>(static_cast<int64_t>(jv.number)));
    } else if constexpr (std::is_same_v<T, Point>) {
        if (jv.type != JsonValue::Type::Object) return false;
        const JsonValue* x = jv.find("x");
        const JsonValue* y = jv.find("y");
        if (x && x->type == JsonValue::Type::Number)
            out.x = static_cast<int16_t>(static_cast<int64_t>(x->number));
        if (y && y->type == JsonValue::Type::Number)
            out.y = static_cast<int16_t>(static_cast<int64_t>(y->number));
    } else if constexpr (std::is_same_v<T, Size>) {
        if (jv.type != JsonValue::Type::Object) return false;
        const JsonValue* width = jv.find("width");
        const JsonValue* height = jv.find("height");
        if (width && width->type == JsonValue::Type::Number)
            out.width = static_cast<uint16_t>(static_cast<int64_t>(width->number));
        if (height && height->type == JsonValue::Type::Number)
            out.height = static_cast<uint16_t>(static_cast<int64_t>(height->number));
    } else if constexpr (std::is_same_v<T, std::vector<std::string>>) {
        if (jv.type != JsonValue::Type::Array) return false;
        out.clear();
        for (const JsonValue& item : jv.array)
            if (item.type == JsonValue::Type::String) out.push_back(item.str);
    } else if constexpr (std::is_same_v<T, const GFXfont*>) {
        if (jv.type == JsonValue::Type::Null) out = nullptr;
        else if (jv.type == JsonValue::Type::String) out = assets.findFont(jv.str.c_str());
        else return false;
    } else if constexpr (std::is_same_v<T, const uint8_t*>) {
        if (jv.type == JsonValue::Type::Null) out = nullptr;
        else if (jv.type == JsonValue::Type::String) out = assets.findBitmap(jv.str.c_str());
        else return false;
    } else {
        static_assert(sizeof(T) == 0, "unserializable reflected field type");
    }
    return true;
}

} // namespace detail

// ----- Component-level -------------------------------------------------------

/// @brief Write one reflected component as a JSON object (field-list order).
template<typename T>
void writeComponentJson(JsonWriter& w, const T& c, const AssetRegistry& assets) {
    w.beginObject();
    ComponentTraits<T>::visitFields(c, [&](const char* name, auto acc) {
        w.key(name);
        detail::writeFieldValue(w, acc.get(), assets);
    });
    w.endObject();
}

/**
 * @brief Populate a reflected component from a JSON object (tolerant)
 *
 * Fields are visited in field-list order, not JSON order, so dependent
 * properties (a gauge's value after its mode) always apply correctly. Missing
 * or wrong-typed fields keep the component's defaults.
 */
template<typename T>
void readComponentJson(const JsonValue& obj, T& c, const AssetRegistry& assets) {
    if (obj.type != JsonValue::Type::Object) return;
    ComponentTraits<T>::visitFields(c, [&](const char* name, auto acc) {
        const JsonValue* jv = obj.find(name);
        if (!jv) return;
        using ValueT = typename decltype(acc)::value_type;
        ValueT tmp = acc.get();
        if (detail::readFieldValue(*jv, tmp, assets)) acc.set(tmp);
    });
}

// ----- Scene-level -----------------------------------------------------------

/**
 * @brief Dump a world (and optional theme) as a versioned scene JSON document
 *
 * Entities are emitted in first-appearance order across the storages (walked
 * in the world's pack order) and each entity's components in pack order. That
 * order is reload-stable — after readSceneJson, storage insertion order *is*
 * file order — so dump → reload → dump is byte-identical. (Entity ids are not
 * usable for ordering: the EntityManager may hand them out in any order.)
 * Component types the world composes but reflection doesn't cover are skipped.
 */
template<typename TWorld>
std::string writeSceneJson(const TWorld& world, const AssetRegistry& assets,
                           const Theme* theme = nullptr) {
    // Union of every storage's entities, deduped, in first-appearance order.
    std::vector<Entity> live;
    TWorld::forEachComponentType([&](auto tag) {
        using C = typename decltype(tag)::type;
        if constexpr (IsReflected<C>::value) {
            const auto& storage = world.template components<C>();
            for (size_t i = 0; i < storage.size(); ++i) {
                const Entity e = storage.entityAt(i);
                const bool seen = std::any_of(live.begin(), live.end(),
                                              [&](Entity o) { return o.id == e.id; });
                if (!seen) live.push_back(e);
            }
        }
    });

    JsonWriter w;
    w.beginObject();
    w.key("version");
    w.value(kSceneJsonVersion);
    if (theme) {
        w.key(ComponentTraits<Theme>::kName);
        writeComponentJson(w, *theme, assets);
    }
    w.key("entities");
    w.beginArray();
    for (const Entity e : live) {
        w.beginObject();
        w.key("components");
        w.beginObject();
        TWorld::forEachComponentType([&](auto tag) {
            using C = typename decltype(tag)::type;
            if constexpr (IsReflected<C>::value) {
                if (const C* c = world.template get<C>(e)) {
                    w.key(ComponentTraits<C>::kName);
                    writeComponentJson(w, *c, assets);
                }
            }
        });
        w.endObject();
        w.endObject();
    }
    w.endArray();
    w.endObject();
    return w.str();
}

/**
 * @brief Load a scene JSON document into an (empty) world
 * @param themeOut Filled from the root `theme` object when present
 * @return false only on malformed JSON; tolerant of everything well-formed
 *
 * Entities are created in file order. Component names the world doesn't
 * compose — or reflection doesn't cover — are skipped, as are unknown fields
 * inside a component (the tolerant-reader contract from day one).
 */
template<typename TWorld>
bool readSceneJson(const std::string& text, TWorld& world, const AssetRegistry& assets,
                   Theme* themeOut = nullptr) {
    JsonValue root;
    if (!parseJson(text, root) || root.type != JsonValue::Type::Object) return false;

    if (themeOut) {
        if (const JsonValue* themeObj = root.find(ComponentTraits<Theme>::kName))
            readComponentJson(*themeObj, *themeOut, assets);
    }

    const JsonValue* entities = root.find("entities");
    if (!entities || entities->type != JsonValue::Type::Array) return true;

    for (const JsonValue& entityObj : entities->array) {
        if (entityObj.type != JsonValue::Type::Object) continue;
        const JsonValue* comps = entityObj.find("components");
        if (!comps || comps->type != JsonValue::Type::Object) continue;

        const Entity e = world.create();
        for (const auto& kv : comps->object) {
            TWorld::forEachComponentType([&](auto tag) {
                using C = typename decltype(tag)::type;
                if constexpr (IsReflected<C>::value) {
                    if (kv.first == ComponentTraits<C>::kName) {
                        if (C* c = world.template add<C>(e))
                            readComponentJson(kv.second, *c, assets);
                    }
                }
            });
        }
    }
    return true;
}

} // namespace enjin2
