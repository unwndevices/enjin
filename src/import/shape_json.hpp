/**
 * @file shape_json.hpp
 * @brief A tiny JSON value + reader for the shape sidecar and the id ledger (Tomodachi #330)
 *
 * Internal to enjin2_import.  RFC 8259 subset: objects keep key order, numbers
 * are doubles, strings are UTF-8 (`\uXXXX` encoded, surrogate pairs joined).
 * The reader is depth- and size-capped and never throws.
 */
#pragma once

#include <cstddef>
#include <string>
#include <utility>
#include <vector>

namespace enjin2::shape_json {

struct Value {
    enum class Type { Null, Bool, Number, String, Array, Object } type = Type::Null;
    bool        b = false;
    double      n = 0;
    std::string s;
    std::vector<Value>                         items;    ///< Array.
    std::vector<std::pair<std::string, Value>> members;  ///< Object, in source order.

    bool isObject() const { return type == Type::Object; }
    bool isArray() const { return type == Type::Array; }
    bool isString() const { return type == Type::String; }
    bool isNumber() const { return type == Type::Number; }
    /// Member @p key of an object, or nullptr.
    const Value* get(const char* key) const;
};

/// Parse @p size bytes.  false (and a message in @p err) on any error.
bool parse(const char* data, size_t size, Value& out, std::string& err, unsigned maxDepth = 32);

/// @p s as a JSON string literal, quotes included.
std::string quote(const std::string& s);

}  // namespace enjin2::shape_json
