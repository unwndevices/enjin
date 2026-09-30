// Lua API descriptors: signature parser, formatter and validation
// (Tomodachi #257). The grammar is documented in lua_api.hpp.
#include "../../include/enjin2/scripting/lua_api.hpp"

#include <algorithm>
#include <cctype>
#include <cstring>

namespace enjin2 {

namespace {

constexpr const char* kBuiltinTypes[] = {
    "int", "number", "string", "boolean", "table",
    "function", "userdata", "thread", "any", "nil",
};

bool identStart(char c) { return std::isalpha(static_cast<unsigned char>(c)) || c == '_'; }
bool identChar(char c) { return std::isalnum(static_cast<unsigned char>(c)) || c == '_'; }

class SignatureParser {
public:
    explicit SignatureParser(const char* text) : p_(text) {}

    bool parse(LuaApiSignature& out) {
        out.overloads.clear();
        do {
            LuaApiOverload overload;
            if (!parseOverload(overload)) return false;
            out.overloads.push_back(std::move(overload));
            skipSpaces();
            if (*p_ == '\0') return true;
            if (*p_ != '\n') return fail("expected end or a newline before another overload");
            ++p_;
        } while (true);
    }

    const std::string& error() const { return error_; }

private:
    const char* p_;
    std::string error_;

    bool fail(const char* what) {
        error_ = std::string(what) + " at \"" + std::string(p_).substr(0, 16) + "\"";
        return false;
    }

    void skipSpaces() {
        while (*p_ == ' ' || *p_ == '\t') ++p_;
    }

    bool accept(char c) {
        skipSpaces();
        if (*p_ != c) return false;
        ++p_;
        return true;
    }

    bool acceptEllipsis() {
        skipSpaces();
        if (std::strncmp(p_, "...", 3) != 0) return false;
        p_ += 3;
        return true;
    }

    bool ident(std::string& out) {
        skipSpaces();
        if (!identStart(*p_)) return fail("expected an identifier");
        const char* start = p_;
        while (identChar(*p_)) ++p_;
        out.assign(start, p_);
        return true;
    }

    // The "|alt|alt" alternatives after a type's first identifier.
    bool unionTail(std::string& out) {
        std::string part;
        while (accept('|')) {
            if (!ident(part)) return false;
            out += '|';
            out += part;
        }
        return true;
    }

    bool type(std::string& out) {
        return ident(out) && unionTail(out);
    }

    bool defaultValue(std::string& out) {
        skipSpaces();
        const char* start = p_;
        if (*p_ == '"') {
            ++p_;
            while (*p_ != '"' && *p_ != '\0' && *p_ != '\n') ++p_;
            if (*p_ != '"') return fail("unterminated string default");
            ++p_;
        } else {
            while (identChar(*p_) || *p_ == '.' || *p_ == '+' || *p_ == '-') ++p_;
            if (p_ == start) return fail("expected a default value");
        }
        out.assign(start, p_);
        return true;
    }

    bool param(LuaApiParam& out) {
        if (acceptEllipsis()) {
            out.name = "...";
            return !accept(':') || type(out.type);
        }
        if (!ident(out.name)) return false;
        if (!accept(':')) return fail("expected ':' and a type");
        if (!type(out.type)) return false;
        if (accept('?')) {
            out.optional = true;
            if (accept('=') && !defaultValue(out.defaultValue)) return false;
        } else if (accept('=')) {
            return fail("a default needs '?' (an optional parameter)");
        }
        return true;
    }

    bool result(LuaApiResult& out) {
        if (acceptEllipsis()) {
            out.name = "...";
            return !accept(':') || type(out.type);
        }
        std::string first;
        if (!ident(first)) return false;
        if (accept(':')) {
            out.name = first;
            if (!type(out.type)) return false;
        } else {
            out.type = first;
            if (!unionTail(out.type)) return false;
        }
        out.optional = accept('?');
        return true;
    }

    bool parseOverload(LuaApiOverload& out) {
        if (!accept('(')) return fail("expected '('");
        if (!accept(')')) {
            bool sawOptional = false;
            do {
                LuaApiParam prm;
                if (!param(prm)) return false;
                if (!out.params.empty() && out.params.back().name == "...")
                    return fail("'...' must be the last parameter");
                const bool variadic = prm.name == "...";
                if (!variadic && !prm.optional && sawOptional)
                    return fail("a required parameter follows an optional one");
                sawOptional = sawOptional || prm.optional;
                for (const auto& other : out.params)
                    if (other.name == prm.name) return fail("duplicate parameter name");
                out.params.push_back(std::move(prm));
            } while (accept(','));
            if (!accept(')')) return fail("expected ',' or ')'");
        }
        skipSpaces();
        if (std::strncmp(p_, "->", 2) != 0) return fail("expected '->'");
        p_ += 2;
        do {
            LuaApiResult res;
            if (!result(res)) return false;
            if (!out.results.empty() && out.results.back().name == "...")
                return fail("'...' must be the last result");
            out.results.push_back(std::move(res));
        } while (accept(','));
        // "nil" alone means no values; inside a list it is a mistake.
        for (const auto& res : out.results) {
            if (res.type != "nil") continue;
            if (out.results.size() > 1 || !res.name.empty() || res.optional)
                return fail("'nil' must stand alone as the result list");
        }
        if (out.results.size() == 1 && out.results[0].type == "nil") out.results.clear();
        return true;
    }
};

// Whether `word` occurs in `text` as a whole identifier.
bool mentionsIdentifier(const char* text, const char* word) {
    if (!text) return false;
    const size_t len = std::strlen(word);
    for (const char* hit = std::strstr(text, word); hit; hit = std::strstr(hit + 1, word)) {
        const bool startOk = hit == text || !identChar(hit[-1]);
        if (startOk && !identChar(hit[len])) return true;
    }
    return false;
}

bool isBuiltinType(const std::string& t) {
    for (const char* b : kBuiltinTypes)
        if (t == b) return true;
    return false;
}

// Split "a|b|c" into its alternatives.
std::vector<std::string> typeParts(const std::string& type) {
    std::vector<std::string> parts;
    size_t start = 0;
    while (start <= type.size()) {
        const size_t bar = type.find('|', start);
        parts.push_back(type.substr(start, bar == std::string::npos ? std::string::npos : bar - start));
        if (bar == std::string::npos) break;
        start = bar + 1;
    }
    return parts;
}

void validateEntries(const LuaApiModule& module, std::string& errors);

void validateEntry(const LuaApiModule& module, const LuaApiEntry& e, std::string& errors) {
    const std::string where = std::string(module.path) + (*module.path ? "." : "") +
                              (e.name ? e.name : "");
    auto problem = [&](const std::string& what) { errors += where + ": " + what + "\n"; };

    if (!e.name || !*e.name) problem("empty name");
    if (!e.summary || !*e.summary) problem("missing summary");

    switch (e.kind) {
        case LuaApiKind::Constant:
            return;
        case LuaApiKind::Table:
            if (!e.table) {
                problem("table entry without a module");
            } else {
                validateEntries(*e.table, errors);
            }
            return;
        case LuaApiKind::Function:
            if (!e.func) problem("function entry without a C function");
            break;
        case LuaApiKind::Lifecycle:
            break;
    }

    LuaApiSignature sig;
    std::string err;
    if (!e.signature || !parseLuaApiSignature(e.signature, sig, &err)) {
        problem("signature does not parse: " + (e.signature ? err : std::string("missing")));
        return;
    }

    // Argument lines name exactly the distinct parameters, in first-appearance order.
    std::vector<std::string> names;
    for (const auto& o : sig.overloads)
        for (const auto& prm : o.params)
            if (std::find(names.begin(), names.end(), prm.name) == names.end())
                names.push_back(prm.name);
    std::vector<std::string> lineNames;
    for (const auto& line : luaApiArgLines(e)) {
        lineNames.push_back(line.first);
        if (line.second.empty()) problem("argument line '" + line.first + "' has no text");
    }
    if (lineNames != names) {
        std::string want, got;
        for (const auto& n : names) want += (want.empty() ? "" : ", ") + n;
        for (const auto& n : lineNames) got += (got.empty() ? "" : ", ") + n;
        problem("argument lines [" + got + "] do not match parameters [" + want + "]");
    }

    // Lowercase types are builtins; capitalised ones are userdata or enum names.
    auto checkType = [&](const std::string& type) {
        for (const auto& part : typeParts(type)) {
            if (part.empty()) continue;
            if (std::islower(static_cast<unsigned char>(part[0])) && !isBuiltinType(part))
                problem("unknown type '" + part + "'");
        }
    };
    for (const auto& o : sig.overloads) {
        for (const auto& prm : o.params) checkType(prm.type);
        for (const auto& res : o.results) checkType(res.type);
    }

    // Each declared enum is non-empty and referenced by the signature or an argument line.
    for (size_t i = 0; i < e.enumCount; ++i) {
        const LuaApiEnum& en = e.enums[i];
        if (!en.name || !*en.name || !en.values || en.count == 0) {
            problem("empty enum");
            continue;
        }
        const bool used = mentionsIdentifier(e.signature, en.name) ||
                          mentionsIdentifier(e.args, en.name);
        if (!used) problem(std::string("enum '") + en.name + "' is never referenced");
    }
}

void validateEntries(const LuaApiModule& module, std::string& errors) {
    if (!module.path) {
        errors += "(module): missing path\n";
        return;
    }
    for (size_t i = 0; i < module.count; ++i) validateEntry(module, module.entries[i], errors);
    for (size_t i = 0; i < module.count; ++i)
        for (size_t j = i + 1; j < module.count; ++j)
            if (module.entries[i].name && module.entries[j].name &&
                std::strcmp(module.entries[i].name, module.entries[j].name) == 0)
                errors += std::string(module.path) + "." + module.entries[i].name +
                          ": described twice\n";
}

} // namespace

bool parseLuaApiSignature(const char* text, LuaApiSignature& out, std::string* error) {
    SignatureParser parser(text ? text : "");
    const bool ok = parser.parse(out);
    if (!ok && error) *error = parser.error();
    return ok;
}

std::string formatLuaApiSignature(const LuaApiSignature& sig) {
    std::string s;
    for (size_t i = 0; i < sig.overloads.size(); ++i) {
        const LuaApiOverload& o = sig.overloads[i];
        if (i > 0) s += '\n';
        s += '(';
        for (size_t j = 0; j < o.params.size(); ++j) {
            const LuaApiParam& prm = o.params[j];
            if (j > 0) s += ", ";
            s += prm.name;
            if (!prm.type.empty()) s += ':' + prm.type;
            if (prm.optional) s += '?';
            if (!prm.defaultValue.empty()) s += '=' + prm.defaultValue;
        }
        s += ") -> ";
        if (o.results.empty()) s += "nil";
        for (size_t j = 0; j < o.results.size(); ++j) {
            const LuaApiResult& res = o.results[j];
            if (j > 0) s += ", ";
            if (!res.name.empty()) s += res.name + (res.type.empty() ? "" : ":");
            s += res.type;
            if (res.optional) s += '?';
        }
    }
    return s;
}

std::vector<std::pair<std::string, std::string>> luaApiArgLines(const LuaApiEntry& entry) {
    std::vector<std::pair<std::string, std::string>> lines;
    if (!entry.args) return lines;
    const char* p = entry.args;
    while (*p) {
        const char* end = std::strchr(p, '\n');
        const std::string line(p, end ? end : p + std::strlen(p));
        const size_t colon = line.find(':');
        std::string name = line.substr(0, colon);
        std::string text = colon == std::string::npos ? std::string() : line.substr(colon + 1);
        const size_t lead = text.find_first_not_of(' ');
        text = lead == std::string::npos ? std::string() : text.substr(lead);
        lines.emplace_back(std::move(name), std::move(text));
        if (!end) break;
        p = end + 1;
    }
    return lines;
}

bool validateLuaApiModule(const LuaApiModule& module, std::string* errors) {
    std::string found;
    validateEntries(module, found);
    if (errors) *errors += found;
    return found.empty();
}

} // namespace enjin2
