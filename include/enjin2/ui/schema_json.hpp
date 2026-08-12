// Scene-schema export (unwn #186, M3 write path).
//
// The editor's palette and inspector are driven by the same X-macro field
// lists that drive save/load (the ratified drift mitigation: one declaration,
// ADR-0005) — this header serializes that reflection surface as JSON so the
// TypeScript side never carries a second copy of the component schema.
//
// Emitted shape:
//   { "version": 1,
//     "components": [ { "name", "id", "fields": [ {"name","kind","default"} ] } ],
//     "theme":  { "name": "theme", "id": 3, "fields": [...] },
//     "themes": ["dark"], "fonts": [...], "bitmaps": [...] }
//
// `components` lists the world's reflected pack in pack order (what the
// palette can place). `theme` rides separately: it is reflected but lives at
// the document root, not on entities. `themes`/`fonts`/`bitmaps` enumerate
// the compiled-in reference sets. Field defaults come from a
// default-constructed component — exactly what addComponentByName attaches.
#pragma once

#include "asset_registry.hpp"
#include "json.hpp"
#include "reflect.hpp"
#include "scene_json.hpp"
#include "theme.hpp"

#include <string>

namespace enjin2 {

namespace detail {

/// @brief Editor-facing kind tag for one reflected field type.
template<typename T>
constexpr const char* schemaFieldKind() {
    if constexpr (std::is_same_v<T, bool>) {
        return "bool";
    } else if constexpr (std::is_same_v<T, std::string>) {
        return "string";
    } else if constexpr (std::is_same_v<T, float>) {
        return "float";
    } else if constexpr (std::is_enum_v<T>) {
        return "enum";
    } else if constexpr (std::is_integral_v<T>) {
        return "int";
    } else if constexpr (std::is_same_v<T, Pixel4>) {
        return "color";
    } else if constexpr (std::is_same_v<T, Point>) {
        return "point";
    } else if constexpr (std::is_same_v<T, Size>) {
        return "size";
    } else if constexpr (std::is_same_v<T, std::vector<std::string>>) {
        return "stringList";
    } else if constexpr (std::is_same_v<T, JsonValue>) {
        return "json";
    } else if constexpr (std::is_same_v<T, const GFXfont*>) {
        return "font";
    } else if constexpr (std::is_same_v<T, const uint8_t*>) {
        return "bitmap";
    } else {
        static_assert(sizeof(T) == 0, "unmapped reflected field type");
    }
}

/// @brief One component's schema entry, defaults read from @p instance.
template<typename C>
void writeComponentSchema(JsonWriter& w, const C& instance, const AssetRegistry& assets) {
    w.beginObject();
    w.key("name");
    w.value(ComponentTraits<C>::kName);
    w.key("id");
    w.value(static_cast<int64_t>(ComponentTraits<C>::kTypeId));
    w.key("fields");
    w.beginArray();
    ComponentTraits<C>::visitFields(instance, [&](const char* name, auto acc) {
        using V = typename decltype(acc)::value_type;
        w.beginObject();
        w.key("name");
        w.value(name);
        w.key("kind");
        w.value(schemaFieldKind<V>());
        w.key("default");
        writeFieldValue(w, acc.get(), assets);
        w.endObject();
    });
    w.endArray();
    w.endObject();
}

} // namespace detail

/**
 * @brief Serialize @p TWorld's reflected component schema (+ reference sets)
 *
 * Components appear in pack order — the same order forEachComponentName
 * enumerates and the scene writer emits.
 *
 * @p extraSections is an optional app-supplied hook, invoked with the schema
 * object still open, to append extra top-level sections (e.g. the app's
 * ParamRegistry `params`/`formatters`, unwn #201). The engine stays
 * app-agnostic — it only knows there may be an extra-sections writer.
 */
template<typename TWorld>
std::string writeSchemaJson(const AssetRegistry& assets,
                            void (*extraSections)(JsonWriter&) = nullptr) {
    JsonWriter w;
    w.beginObject();
    w.key("version");
    w.value(kSceneJsonVersion);

    w.key("components");
    w.beginArray();
    TWorld::forEachComponentType([&](auto tag) {
        using C = typename decltype(tag)::type;
        if constexpr (IsReflected<C>::value)
            detail::writeComponentSchema(w, C{}, assets);
    });
    w.endArray();

    w.key("theme");
    detail::writeComponentSchema(w, kDefaultTheme, assets);

    // Reference-only enumerations from the compiled-in set. Themes have no
    // registry: Theme::dark() is the one built-in.
    w.key("themes");
    w.beginArray();
    w.value("dark");
    w.endArray();
    w.key("fonts");
    w.beginArray();
    for (size_t i = 0; i < assets.fontCount(); ++i) w.value(assets.fontAt(i).name);
    w.endArray();
    w.key("bitmaps");
    w.beginArray();
    for (size_t i = 0; i < assets.bitmapCount(); ++i) w.value(assets.bitmapAt(i).name);
    w.endArray();

    if (extraSections) extraSections(w);

    w.endObject();
    return w.str();
}

} // namespace enjin2
