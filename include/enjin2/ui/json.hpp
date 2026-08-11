#pragma once

#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>
#include <utility>
#include <vector>

/**
 * @file json.hpp
 * @brief Minimal JSON writer + tolerant DOM reader for scene serialization
 *
 * Hand-rolled on purpose (ADR-0005: hand-rolled to_json/from_json, no JSON
 * dependency in the engine). The writer pretty-prints with two-space indents so
 * scene files stay git-diffable; the reader is a small recursive-descent parser
 * into an ordered DOM that preserves unknown keys for tolerant consumers.
 *
 * Number round-trip: floats are written with `%.9g`, which is lossless for
 * `float` through the reader's `double` representation — a dumped scene reloads
 * bit-identical values.
 *
 * This DOM allocates (std::string/std::vector), which is fine for the desktop
 * preview, the editor WASM build and tests. The ESP32 loader (M5) is planned as
 * a SAX-style streaming parse and does not go through this header.
 */

namespace enjin2 {

/// @brief Pretty-printing JSON writer over a std::string.
class JsonWriter {
public:
    /// @brief Open an object; as an element it emits its own comma/indent.
    void beginObject() {
        element();
        out_ += '{';
        push();
    }

    void endObject() {
        pop();
        out_ += '}';
    }

    /// @brief Open an array; as an element it emits its own comma/indent.
    void beginArray() {
        element();
        out_ += '[';
        push();
    }

    void endArray() {
        pop();
        out_ += ']';
    }

    /// @brief Emit an object key; the next value attaches to it.
    void key(const char* k) {
        comma();
        newlineIndent();
        writeEscaped(k);
        out_ += ": ";
        pendingKey_ = true;
    }

    void value(const char* s) {
        element();
        writeEscaped(s);
    }
    void value(const std::string& s) { value(s.c_str()); }

    void value(bool b) {
        element();
        out_ += b ? "true" : "false";
    }

    void value(int64_t n) {
        element();
        char buf[24];
        std::snprintf(buf, sizeof(buf), "%lld", static_cast<long long>(n));
        out_ += buf;
    }

    /// @brief Non-finite floats have no JSON representation and emit null.
    void value(float f) {
        if (!std::isfinite(f)) {
            null();
            return;
        }
        element();
        char buf[24];
        std::snprintf(buf, sizeof(buf), "%.9g", static_cast<double>(f));
        out_ += buf;
    }

    void null() {
        element();
        out_ += "null";
    }

    /// @brief The document so far (call after the root closes).
    const std::string& str() const { return out_; }

private:
    // An element either follows a key (no comma/indent of its own) or is an
    // array item / root (comma + fresh line at the current depth).
    void element() {
        if (pendingKey_) {
            pendingKey_ = false;
            armed_ = true; // the key's value completes an element at this depth
            return;
        }
        comma();
        if (depth_ > 0) newlineIndent();
        armed_ = true;
    }

    void comma() {
        if (armed_) out_ += ',';
    }

    void newlineIndent() {
        out_ += '\n';
        out_.append(static_cast<size_t>(depth_) * 2, ' ');
    }

    void push() {
        ++depth_;
        armed_ = false;
    }

    void pop() {
        --depth_;
        if (armed_) newlineIndent();
        armed_ = true;
    }

    void writeEscaped(const char* s) {
        out_ += '"';
        for (; *s; ++s) {
            const unsigned char c = static_cast<unsigned char>(*s);
            switch (c) {
                case '"': out_ += "\\\""; break;
                case '\\': out_ += "\\\\"; break;
                case '\n': out_ += "\\n"; break;
                case '\r': out_ += "\\r"; break;
                case '\t': out_ += "\\t"; break;
                default:
                    if (c < 0x20) {
                        char buf[8];
                        std::snprintf(buf, sizeof(buf), "\\u%04x", c);
                        out_ += buf;
                    } else {
                        out_ += static_cast<char>(c);
                    }
            }
        }
        out_ += '"';
    }

    std::string out_;
    int depth_ = 0;
    bool armed_ = false;      ///< A sibling element was already emitted at this depth.
    bool pendingKey_ = false; ///< A key was just written; the next value inlines after it.
};

/// @brief Parsed JSON value — ordered object keys, doubles for every number.
struct JsonValue {
    enum class Type { Null, Bool, Number, String, Object, Array };

    Type type = Type::Null;
    bool boolean = false;
    double number = 0.0;
    std::string str;
    std::vector<std::pair<std::string, JsonValue>> object;
    std::vector<JsonValue> array;

    /// @brief First member named @p k, or nullptr (tolerant lookup).
    const JsonValue* find(const char* k) const {
        if (type != Type::Object) return nullptr;
        for (const auto& kv : object)
            if (kv.first == k) return &kv.second;
        return nullptr;
    }
};

namespace detail {

/// @brief Recursive-descent JSON parser (strict grammar, no trailing garbage).
class JsonParser {
public:
    JsonParser(const char* text, size_t len) : p_(text), end_(text + len) {}

    bool parse(JsonValue& out) {
        skipWs();
        if (!parseValue(out)) return false;
        skipWs();
        return p_ == end_;
    }

private:
    void skipWs() {
        while (p_ != end_ && (*p_ == ' ' || *p_ == '\t' || *p_ == '\n' || *p_ == '\r')) ++p_;
    }

    bool literal(const char* s) {
        const size_t n = std::strlen(s);
        if (static_cast<size_t>(end_ - p_) < n || std::memcmp(p_, s, n) != 0) return false;
        p_ += n;
        return true;
    }

    bool parseValue(JsonValue& out) {
        if (p_ == end_) return false;
        switch (*p_) {
            case '{': return parseObject(out);
            case '[': return parseArray(out);
            case '"':
                out.type = JsonValue::Type::String;
                return parseString(out.str);
            case 't':
                out.type = JsonValue::Type::Bool;
                out.boolean = true;
                return literal("true");
            case 'f':
                out.type = JsonValue::Type::Bool;
                out.boolean = false;
                return literal("false");
            case 'n':
                out.type = JsonValue::Type::Null;
                return literal("null");
            default: return parseNumber(out);
        }
    }

    bool parseObject(JsonValue& out) {
        out.type = JsonValue::Type::Object;
        ++p_; // '{'
        skipWs();
        if (p_ != end_ && *p_ == '}') {
            ++p_;
            return true;
        }
        while (p_ != end_) {
            std::string k;
            skipWs();
            if (p_ == end_ || *p_ != '"' || !parseString(k)) return false;
            skipWs();
            if (p_ == end_ || *p_ != ':') return false;
            ++p_;
            skipWs();
            JsonValue v;
            if (!parseValue(v)) return false;
            out.object.emplace_back(std::move(k), std::move(v));
            skipWs();
            if (p_ == end_) return false;
            if (*p_ == ',') {
                ++p_;
                continue;
            }
            if (*p_ == '}') {
                ++p_;
                return true;
            }
            return false;
        }
        return false;
    }

    bool parseArray(JsonValue& out) {
        out.type = JsonValue::Type::Array;
        ++p_; // '['
        skipWs();
        if (p_ != end_ && *p_ == ']') {
            ++p_;
            return true;
        }
        while (p_ != end_) {
            JsonValue v;
            skipWs();
            if (!parseValue(v)) return false;
            out.array.push_back(std::move(v));
            skipWs();
            if (p_ == end_) return false;
            if (*p_ == ',') {
                ++p_;
                continue;
            }
            if (*p_ == ']') {
                ++p_;
                return true;
            }
            return false;
        }
        return false;
    }

    bool parseString(std::string& out) {
        ++p_; // '"'
        while (p_ != end_) {
            const unsigned char c = static_cast<unsigned char>(*p_);
            if (c == '"') {
                ++p_;
                return true;
            }
            if (c == '\\') {
                ++p_;
                if (p_ == end_) return false;
                switch (*p_) {
                    case '"': out += '"'; break;
                    case '\\': out += '\\'; break;
                    case '/': out += '/'; break;
                    case 'b': out += '\b'; break;
                    case 'f': out += '\f'; break;
                    case 'n': out += '\n'; break;
                    case 'r': out += '\r'; break;
                    case 't': out += '\t'; break;
                    case 'u': {
                        unsigned cp = 0;
                        if (!parseHex4(cp)) return false;
                        appendUtf8(out, cp);
                        break;
                    }
                    default: return false;
                }
                ++p_;
            } else {
                out += static_cast<char>(c);
                ++p_;
            }
        }
        return false;
    }

    bool parseHex4(unsigned& cp) {
        cp = 0;
        for (int i = 0; i < 4; ++i) {
            ++p_;
            if (p_ == end_) return false;
            const char c = *p_;
            unsigned d;
            if (c >= '0' && c <= '9') d = static_cast<unsigned>(c - '0');
            else if (c >= 'a' && c <= 'f') d = static_cast<unsigned>(c - 'a' + 10);
            else if (c >= 'A' && c <= 'F') d = static_cast<unsigned>(c - 'A' + 10);
            else return false;
            cp = (cp << 4) | d;
        }
        return true;
    }

    // Surrogate pairs are not recombined (the writer never emits them); a lone
    // surrogate encodes as-is, which round-trips our own output faithfully.
    static void appendUtf8(std::string& out, unsigned cp) {
        if (cp < 0x80) {
            out += static_cast<char>(cp);
        } else if (cp < 0x800) {
            out += static_cast<char>(0xC0 | (cp >> 6));
            out += static_cast<char>(0x80 | (cp & 0x3F));
        } else {
            out += static_cast<char>(0xE0 | (cp >> 12));
            out += static_cast<char>(0x80 | ((cp >> 6) & 0x3F));
            out += static_cast<char>(0x80 | (cp & 0x3F));
        }
    }

    bool parseNumber(JsonValue& out) {
        const char* start = p_;
        if (p_ != end_ && *p_ == '+') return false; // JSON forbids a leading '+'
        if (p_ != end_ && *p_ == '-') ++p_;
        bool any = false;
        while (p_ != end_ && ((*p_ >= '0' && *p_ <= '9') || *p_ == '.' || *p_ == 'e' ||
                              *p_ == 'E' || *p_ == '-' || *p_ == '+')) {
            ++p_;
            any = true;
        }
        if (!any) return false;
        // strtod needs a terminated buffer; numbers are short.
        char buf[48];
        const size_t n = static_cast<size_t>(p_ - start);
        if (n >= sizeof(buf)) return false;
        std::memcpy(buf, start, n);
        buf[n] = '\0';
        char* parsed = nullptr;
        out.number = std::strtod(buf, &parsed);
        if (parsed != buf + n) return false;
        out.type = JsonValue::Type::Number;
        return true;
    }

    const char* p_;
    const char* end_;
};

} // namespace detail

/// @brief Parse @p len bytes of JSON into @p out; false on malformed input.
inline bool parseJson(const char* text, size_t len, JsonValue& out) {
    return detail::JsonParser(text, len).parse(out);
}

inline bool parseJson(const std::string& text, JsonValue& out) {
    return parseJson(text.data(), text.size(), out);
}

} // namespace enjin2
