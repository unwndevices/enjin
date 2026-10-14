/**
 * @file shape_json.cpp
 * @brief Tiny JSON reader for the Shape bundle (Tomodachi #330)
 */
#include "shape_json.hpp"
#include "parse_number.hpp"

#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>

namespace enjin2::shape_json {

const Value* Value::get(const char* key) const {
    if (type != Type::Object) return nullptr;
    for (const auto& m : members)
        if (m.first == key) return &m.second;
    return nullptr;
}

namespace {

class Reader {
public:
    Reader(const char* data, size_t size, unsigned maxDepth) : p_(data), end_(data + size), maxDepth_(maxDepth) {}

    bool document(Value& out, std::string& err) {
        ws();
        if (!value(out, 0)) {
            err = err_;
            return false;
        }
        ws();
        if (p_ != end_) {
            err = "trailing characters after the JSON value";
            return false;
        }
        return true;
    }

private:
    const char* p_;
    const char* end_;
    unsigned    maxDepth_;
    std::string err_;

    bool fail(const char* m) {
        if (err_.empty()) err_ = m;
        return false;
    }
    void ws() {
        while (p_ < end_ && (*p_ == ' ' || *p_ == '\t' || *p_ == '\n' || *p_ == '\r')) ++p_;
    }
    bool lit(const char* word) {
        const size_t n = std::strlen(word);
        if (static_cast<size_t>(end_ - p_) < n || std::memcmp(p_, word, n) != 0) return fail("invalid literal");
        p_ += n;
        return true;
    }

    bool value(Value& v, unsigned depth) {
        if (depth > maxDepth_) return fail("JSON nested too deeply");
        if (p_ >= end_) return fail("unexpected end of JSON");
        switch (*p_) {
            case '{': return object(v, depth);
            case '[': return array(v, depth);
            case '"': v.type = Value::Type::String; return string(v.s);
            case 't': v.type = Value::Type::Bool; v.b = true; return lit("true");
            case 'f': v.type = Value::Type::Bool; v.b = false; return lit("false");
            case 'n': v.type = Value::Type::Null; return lit("null");
            default: return number(v);
        }
    }

    bool object(Value& v, unsigned depth) {
        v.type = Value::Type::Object;
        ++p_;
        ws();
        if (p_ < end_ && *p_ == '}') {
            ++p_;
            return true;
        }
        for (;;) {
            ws();
            if (p_ >= end_ || *p_ != '"') return fail("expected a string key");
            std::string key;
            if (!string(key)) return false;
            ws();
            if (p_ >= end_ || *p_ != ':') return fail("expected ':'");
            ++p_;
            ws();
            Value child;
            if (!value(child, depth + 1)) return false;
            for (const auto& m : v.members)
                if (m.first == key) return fail("duplicate key");
            v.members.emplace_back(std::move(key), std::move(child));
            ws();
            if (p_ < end_ && *p_ == ',') {
                ++p_;
                continue;
            }
            if (p_ < end_ && *p_ == '}') {
                ++p_;
                return true;
            }
            return fail("expected ',' or '}'");
        }
    }

    bool array(Value& v, unsigned depth) {
        v.type = Value::Type::Array;
        ++p_;
        ws();
        if (p_ < end_ && *p_ == ']') {
            ++p_;
            return true;
        }
        for (;;) {
            ws();
            Value child;
            if (!value(child, depth + 1)) return false;
            v.items.push_back(std::move(child));
            ws();
            if (p_ < end_ && *p_ == ',') {
                ++p_;
                continue;
            }
            if (p_ < end_ && *p_ == ']') {
                ++p_;
                return true;
            }
            return fail("expected ',' or ']'");
        }
    }

    static void utf8(std::string& out, uint32_t c) {
        if (c < 0x80) {
            out += static_cast<char>(c);
        } else if (c < 0x800) {
            out += static_cast<char>(0xC0 | (c >> 6));
            out += static_cast<char>(0x80 | (c & 0x3F));
        } else if (c < 0x10000) {
            out += static_cast<char>(0xE0 | (c >> 12));
            out += static_cast<char>(0x80 | ((c >> 6) & 0x3F));
            out += static_cast<char>(0x80 | (c & 0x3F));
        } else {
            out += static_cast<char>(0xF0 | (c >> 18));
            out += static_cast<char>(0x80 | ((c >> 12) & 0x3F));
            out += static_cast<char>(0x80 | ((c >> 6) & 0x3F));
            out += static_cast<char>(0x80 | (c & 0x3F));
        }
    }

    bool hex4(uint32_t& c) {
        if (end_ - p_ < 4) return fail("truncated \\u escape");
        c = 0;
        for (int i = 0; i < 4; ++i) {
            const char h = *p_++;
            c <<= 4;
            if (h >= '0' && h <= '9') c |= static_cast<uint32_t>(h - '0');
            else if (h >= 'a' && h <= 'f') c |= static_cast<uint32_t>(h - 'a' + 10);
            else if (h >= 'A' && h <= 'F') c |= static_cast<uint32_t>(h - 'A' + 10);
            else return fail("bad \\u escape");
        }
        return true;
    }

    bool string(std::string& out) {
        ++p_;  // opening quote
        out.clear();
        while (p_ < end_) {
            const char c = *p_++;
            if (c == '"') return true;
            if (static_cast<unsigned char>(c) < 0x20) return fail("control character in a string");
            if (c != '\\') {
                out += c;
                continue;
            }
            if (p_ >= end_) break;
            const char e = *p_++;
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
                    uint32_t cp = 0;
                    if (!hex4(cp)) return false;
                    if (cp >= 0xD800 && cp <= 0xDBFF) {
                        uint32_t lo = 0;
                        if (end_ - p_ < 2 || p_[0] != '\\' || p_[1] != 'u') return fail("lone surrogate");
                        p_ += 2;
                        if (!hex4(lo)) return false;
                        if (lo < 0xDC00 || lo > 0xDFFF) return fail("lone surrogate");
                        cp = 0x10000 + ((cp - 0xD800) << 10) + (lo - 0xDC00);
                    } else if (cp >= 0xDC00 && cp <= 0xDFFF) {
                        return fail("lone surrogate");
                    }
                    utf8(out, cp);
                    break;
                }
                default: return fail("bad escape");
            }
        }
        return fail("unterminated string");
    }

    bool number(Value& v) {
        const char* s = p_;
        if (p_ < end_ && *p_ == '-') ++p_;
        if (p_ >= end_ || *p_ < '0' || *p_ > '9') return fail("invalid value");
        if (*p_ == '0') ++p_;
        else while (p_ < end_ && *p_ >= '0' && *p_ <= '9') ++p_;
        if (p_ < end_ && *p_ == '.') {
            ++p_;
            if (p_ >= end_ || *p_ < '0' || *p_ > '9') return fail("invalid number");
            while (p_ < end_ && *p_ >= '0' && *p_ <= '9') ++p_;
        }
        if (p_ < end_ && (*p_ == 'e' || *p_ == 'E')) {
            ++p_;
            if (p_ < end_ && (*p_ == '+' || *p_ == '-')) ++p_;
            if (p_ >= end_ || *p_ < '0' || *p_ > '9') return fail("invalid number");
            while (p_ < end_ && *p_ >= '0' && *p_ <= '9') ++p_;
        }
        if (p_ - s > 64) return fail("number too long");
        v.type = Value::Type::Number;
        if (!import_detail::parseDouble(s, p_, v.n)) return fail("number out of range");
        return true;
    }
};

}  // namespace

bool parse(const char* data, size_t size, Value& out, std::string& err, unsigned maxDepth) {
    out = Value{};
    if (!data && size) {
        err = "no data";
        return false;
    }
    Reader r(data, size, maxDepth);
    return r.document(out, err);
}

std::string quote(const std::string& s) {
    std::string out = "\"";
    for (const char c : s) {
        switch (c) {
            case '"': out += "\\\""; break;
            case '\\': out += "\\\\"; break;
            case '\n': out += "\\n"; break;
            case '\r': out += "\\r"; break;
            case '\t': out += "\\t"; break;
            default:
                if (static_cast<unsigned char>(c) < 0x20) {
                    char b[8];
                    std::snprintf(b, sizeof b, "\\u%04x", static_cast<unsigned>(static_cast<unsigned char>(c)));
                    out += b;
                } else {
                    out += c;
                }
        }
    }
    return out + "\"";
}

}  // namespace enjin2::shape_json
