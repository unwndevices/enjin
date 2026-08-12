#pragma once

#include "scene_json.hpp"

#include <cstdlib>
#include <cstring>
#include <string>

/**
 * @file scene_stream.hpp
 * @brief Streaming (SAX-style) scene-document loader — the ESP32/M5 path
 *
 * The DOM loader (scene_json.hpp) needs the whole document text plus the
 * whole parsed tree in memory at once; this loader reads from a pull byte
 * source through a fixed buffer and applies entities to the world one at a
 * time, so peak heap is bounded by the persistent behavior subtrees (which
 * the SceneVM interprets and must be retained anyway) plus a single entity's
 * subtree — never the full file.
 *
 * Deliberately the same document model, not a compact-struct compile: the
 * behavior sections land in the same @ref SceneDoc DOM form the ratified
 * @ref SceneVM interprets on every other track, so there is no second
 * interpreter to drift and stream-load vs DOM-load is byte-identical through
 * the canonical writer (pinned by SceneStreamLoader_gtest).
 *
 * Tolerance and rejection mirror the DOM loader: unknown root keys,
 * component names and fields are skipped (skipped subtrees are still
 * validated), malformed JSON or trailing garbage fails the load, and only
 * the first occurrence of each known root key is honored — matching
 * JsonValue::find. One extra guard for firmware: nesting beyond
 * @ref kSceneStreamMaxDepth is rejected to bound parser stack use.
 */

namespace enjin2 {

/// @brief Pull-based byte source for the streaming scene loader.
struct SceneStreamSource {
    virtual ~SceneStreamSource() = default;

    /// Fill @p buf with up to @p maxLen bytes.
    /// @return bytes produced; 0 on end of input; negative on I/O error.
    virtual int read(char* buf, size_t maxLen) = 0;
};

/// @brief Nesting limit for the streaming parser (recursion == stack on ESP32).
inline constexpr int kSceneStreamMaxDepth = 64;

namespace detail {

/// Recursive-descent parser over a buffered SceneStreamSource. Mirrors
/// JsonParser's grammar exactly; adds skip-mode parsing (validate, discard)
/// so unknown subtrees never build a DOM.
class StreamJsonParser {
public:
    explicit StreamJsonParser(SceneStreamSource& src) : src_(src) {}

    static constexpr int kEof = -1;
    static constexpr int kIoError = -2;

    int peekc() {
        if (pos_ == len_ && !fill()) return failed_ ? kIoError : kEof;
        return static_cast<unsigned char>(buf_[pos_]);
    }

    int getc() {
        const int c = peekc();
        if (c >= 0) ++pos_;
        return c;
    }

    bool failed() const { return failed_; }

    void skipWs() {
        int c;
        while ((c = peekc()) == ' ' || c == '\t' || c == '\n' || c == '\r') ++pos_;
    }

    bool literal(const char* s) {
        for (; *s; ++s)
            if (getc() != static_cast<unsigned char>(*s)) return false;
        return true;
    }

    /// Parse one value into a DOM subtree (bounded: caller controls scope).
    bool parseValue(JsonValue& out, int depth) {
        if (depth > kSceneStreamMaxDepth) return false;
        switch (peekc()) {
            case '{': {
                out.type = JsonValue::Type::Object;
                ++pos_;
                skipWs();
                if (peekc() == '}') {
                    ++pos_;
                    return true;
                }
                for (;;) {
                    std::string k;
                    skipWs();
                    if (peekc() != '"' || !parseString(k)) return false;
                    skipWs();
                    if (getc() != ':') return false;
                    skipWs();
                    JsonValue v;
                    if (!parseValue(v, depth + 1)) return false;
                    out.object.emplace_back(std::move(k), std::move(v));
                    skipWs();
                    const int c = getc();
                    if (c == ',') continue;
                    if (c == '}') return true;
                    return false;
                }
            }
            case '[': {
                out.type = JsonValue::Type::Array;
                ++pos_;
                skipWs();
                if (peekc() == ']') {
                    ++pos_;
                    return true;
                }
                for (;;) {
                    skipWs();
                    JsonValue v;
                    if (!parseValue(v, depth + 1)) return false;
                    out.array.push_back(std::move(v));
                    skipWs();
                    const int c = getc();
                    if (c == ',') continue;
                    if (c == ']') return true;
                    return false;
                }
            }
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
            default:
                out.type = JsonValue::Type::Number;
                return parseNumber(out.number);
        }
    }

    /// Validate and discard one value — the skip path for unknown keys.
    bool skipValue(int depth) {
        if (depth > kSceneStreamMaxDepth) return false;
        switch (peekc()) {
            case '{': {
                ++pos_;
                skipWs();
                if (peekc() == '}') {
                    ++pos_;
                    return true;
                }
                for (;;) {
                    skipWs();
                    if (peekc() != '"' || !skipString()) return false;
                    skipWs();
                    if (getc() != ':') return false;
                    skipWs();
                    if (!skipValue(depth + 1)) return false;
                    skipWs();
                    const int c = getc();
                    if (c == ',') continue;
                    if (c == '}') return true;
                    return false;
                }
            }
            case '[': {
                ++pos_;
                skipWs();
                if (peekc() == ']') {
                    ++pos_;
                    return true;
                }
                for (;;) {
                    skipWs();
                    if (!skipValue(depth + 1)) return false;
                    skipWs();
                    const int c = getc();
                    if (c == ',') continue;
                    if (c == ']') return true;
                    return false;
                }
            }
            case '"':
                return skipString();
            case 't':
                return literal("true");
            case 'f':
                return literal("false");
            case 'n':
                return literal("null");
            default: {
                double dummy;
                return parseNumber(dummy);
            }
        }
    }

    bool parseString(std::string& out) {
        ++pos_; // opening quote (caller peeked '"')
        for (;;) {
            const int c = getc();
            if (c < 0) return false;
            if (c == '"') return true;
            if (c == '\\') {
                const int e = getc();
                if (e < 0) return false;
                switch (e) {
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
            } else {
                out += static_cast<char>(c);
            }
        }
    }

private:
    bool fill() {
        if (eof_ || failed_) return false;
        const int n = src_.read(buf_, sizeof(buf_));
        if (n < 0) {
            failed_ = true;
            return false;
        }
        if (n == 0) {
            eof_ = true;
            return false;
        }
        pos_ = 0;
        len_ = static_cast<size_t>(n);
        return true;
    }

    bool skipString() {
        ++pos_; // opening quote
        for (;;) {
            const int c = getc();
            if (c < 0) return false;
            if (c == '"') return true;
            if (c == '\\') {
                const int e = getc();
                if (e < 0) return false;
                switch (e) {
                    case '"': case '\\': case '/': case 'b': case 'f':
                    case 'n': case 'r': case 't':
                        break;
                    case 'u': {
                        unsigned cp = 0;
                        if (!parseHex4(cp)) return false;
                        break;
                    }
                    default: return false;
                }
            }
        }
    }

    bool parseHex4(unsigned& cp) {
        cp = 0;
        for (int i = 0; i < 4; ++i) {
            const int c = getc();
            unsigned d;
            if (c >= '0' && c <= '9') d = static_cast<unsigned>(c - '0');
            else if (c >= 'a' && c <= 'f') d = static_cast<unsigned>(c - 'a' + 10);
            else if (c >= 'A' && c <= 'F') d = static_cast<unsigned>(c - 'A' + 10);
            else return false;
            cp = (cp << 4) | d;
        }
        return true;
    }

    // Surrogate pairs are not recombined — same policy as the DOM parser.
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

    bool parseNumber(double& out) {
        char num[48];
        size_t n = 0;
        if (peekc() == '+') return false; // JSON forbids a leading '+'
        if (peekc() == '-') {
            num[n++] = '-';
            ++pos_;
        }
        bool any = false;
        for (;;) {
            const int c = peekc();
            if (!((c >= '0' && c <= '9') || c == '.' || c == 'e' || c == 'E' ||
                  c == '-' || c == '+'))
                break;
            if (n >= sizeof(num) - 1) return false;
            num[n++] = static_cast<char>(c);
            ++pos_;
            any = true;
        }
        if (!any) return false;
        num[n] = '\0';
        char* parsed = nullptr;
        out = std::strtod(num, &parsed);
        return parsed == num + n;
    }

    SceneStreamSource& src_;
    char buf_[256];
    size_t pos_ = 0;
    size_t len_ = 0;
    bool eof_ = false;
    bool failed_ = false;
};

} // namespace detail

/**
 * @brief Stream-load a full scene document into @p doc + an (empty) world
 *
 * The streaming twin of @ref readSceneDocJson: same tolerance, same section
 * semantics, same first-occurrence-wins key handling — but entities are
 * created as their array elements finish parsing and only the behavior
 * sections are retained as DOM.
 *
 * @return false on malformed JSON, source I/O error, non-object root,
 *         trailing garbage, or nesting beyond kSceneStreamMaxDepth.
 */
template<typename TWorld>
bool readSceneDocStream(SceneStreamSource& src, SceneDoc& doc, TWorld& world,
                        const AssetRegistry& assets, Theme* themeOut = nullptr,
                        bool* themePresentOut = nullptr) {
    detail::StreamJsonParser p(src);

    bool themePresent = false;
    if (themePresentOut) *themePresentOut = false;

    p.skipWs();
    if (p.getc() != '{') return false;
    p.skipWs();

    bool seenVersion = false, seenScene = false, seenState = false, seenTimers = false,
         seenAnimations = false, seenOn = false, seenTheme = false, seenEntities = false;

    bool rootOpen = p.peekc() != '}';
    if (!rootOpen) p.getc(); // empty root object: straight to the trailing check
    while (rootOpen) {
        p.skipWs();
        std::string key;
        if (p.peekc() != '"' || !p.parseString(key)) return false;
        p.skipWs();
        if (p.getc() != ':') return false;
        p.skipWs();

        // First occurrence of each known key wins (JsonValue::find semantics);
        // duplicates and unknown keys are validated and discarded.
        auto claim = [](bool& seen) {
            const bool firstHit = !seen;
            seen = true;
            return firstHit;
        };

        if (key == "version" && claim(seenVersion)) {
            JsonValue v;
            if (!p.parseValue(v, 0)) return false;
            if (v.type == JsonValue::Type::Number) doc.version = static_cast<int64_t>(v.number);
            // v2 clean break (unwn #202): reject a pre-v2 document as soon as its
            // version is seen — before more of the stream is applied to the world.
            if (doc.version < kSceneMinReadVersion) return false;
        } else if (key == "scene" && claim(seenScene)) {
            JsonValue v;
            if (!p.parseValue(v, 0)) return false;
            if (v.type == JsonValue::Type::String) doc.scene = v.str;
        } else if (key == "state" && claim(seenState)) {
            if (!p.parseValue(doc.state, 0)) return false;
        } else if (key == "timers" && claim(seenTimers)) {
            if (!p.parseValue(doc.timers, 0)) return false;
        } else if (key == "animations" && claim(seenAnimations)) {
            if (!p.parseValue(doc.animations, 0)) return false;
        } else if (key == "on" && claim(seenOn)) {
            if (!p.parseValue(doc.on, 0)) return false;
        } else if (key == ComponentTraits<Theme>::kName && claim(seenTheme)) {
            themePresent = true;
            JsonValue v;
            if (!p.parseValue(v, 0)) return false;
            if (themeOut) readComponentJson(v, *themeOut, assets);
        } else if (key == "entities" && claim(seenEntities) && p.peekc() == '[') {
            // The streaming heart: one entity subtree in memory at a time.
            p.getc();
            p.skipWs();
            if (p.peekc() == ']') {
                p.getc();
            } else {
                for (;;) {
                    p.skipWs();
                    JsonValue entity;
                    if (!p.parseValue(entity, 0)) return false;
                    readEntityJson(entity, world, assets);
                    p.skipWs();
                    const int c = p.getc();
                    if (c == ',') continue;
                    if (c == ']') break;
                    return false;
                }
            }
        } else {
            // Unknown key, duplicate of a known key, or a non-array
            // `entities` value (which the DOM loader also ignores).
            if (!p.skipValue(0)) return false;
        }

        p.skipWs();
        const int c = p.getc();
        if (c == ',') continue;
        if (c == '}') break;
        return false;
    }

    // Strict grammar: nothing but whitespace may follow the root.
    p.skipWs();
    if (p.peekc() != detail::StreamJsonParser::kEof || p.failed()) return false;

    if (themePresentOut) *themePresentOut = themePresent;
    return true;
}

} // namespace enjin2
