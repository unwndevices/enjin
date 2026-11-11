// Lua API descriptors: signature parser, formatter and validation
// (Tomodachi #257). The grammar is documented in lua_api.hpp.
#include "../../include/enjin2/scripting/lua_api.hpp"

#include <algorithm>
#include <functional>
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

// ADR-0030 case: no '_' in a name; UPPER_SNAKE is a constant's.
bool hasUnderscore(const std::string& s) { return s.find('_') != std::string::npos; }
bool isUpperSnake(const std::string& s) {
    if (s.empty() || !std::isupper(static_cast<unsigned char>(s[0]))) return false;
    for (char c : s)
        if (!(std::isupper(static_cast<unsigned char>(c)) || std::isdigit(static_cast<unsigned char>(c)) || c == '_'))
            return false;
    return true;
}

// ADR-0030's two failure forms, held as a whitelist. Each clause of a
// descriptor's text (split at "; ", ". " and newlines) that names a failure
// condition (an unknown name, an invalid or bad argument, a full pool, a
// missing file, out of range, a printed reason, ignored, keeps the current
// one, ...) must say which of the two forms it is: it raises, or (on a
// returnsErr() entry) it returns `nil, "…"` / `nil, err` (or is that entry's
// `err: "…"` list). Anything else
// ("nil for an unknown id", "false when", "-1 on failure", "an unknown name is
// forward", "is ignored", "logs") is not a failure report and fails the census.
// Quoted text is a value the function hands back ("failed", "unknown", an err
// message), not a condition. A reading is not a failure: "the clock is
// unknown", a finished or stale id, an omitted argument. Returns the offending
// clause, or "" when every failure clause is one of the two forms.
std::string failureOutsideTheTwoForms(const char* text, bool nilErr) {
    if (!text) return {};
    static const char* const kConditions[] = {
        "unknown", "invalid", "bad", "wrong", "fail", "fails", "failed", "failure",
        "is full", "are full", "when all", "is busy", "are busy", "is in use", "are in use",
        "missing", "not found", "no such", "out of range", "too long", "too many", "printed",
        "logs and", "and logs", "ignored", "keeps the current", "unsupported", "not a",
    };
    static const char* const kNotAFailure[] = {"nothing fails", "never fails", "cannot fail",
                                               "omitted", "is unknown"};
    // Whole words: "invalid" is not in "invalidate", "bad" not in "badge".
    auto has = [](const std::string& c, const char* w) {
        const size_t len = std::strlen(w);
        for (size_t at = c.find(w); at != std::string::npos; at = c.find(w, at + 1)) {
            const bool startOk = at == 0 || !std::isalnum(static_cast<unsigned char>(c[at - 1]));
            const bool endOk = at + len >= c.size() || !std::isalnum(static_cast<unsigned char>(c[at + len]));
            if (startOk && endOk) return true;
        }
        return false;
    };
    auto judge = [&](const std::string& raw) -> bool {
        std::string c, bare;  // lower-cased; and without its quoted values
        bool quoted = false;
        for (char ch : raw) {
            const char lower = static_cast<char>(std::tolower(static_cast<unsigned char>(ch)));
            c += lower;
            if (ch == '"') { quoted = !quoted; bare += ' '; continue; }
            bare += quoted ? ' ' : lower;
        }
        for (const char* w : kNotAFailure)
            if (bare.find(w) != std::string::npos) return true;
        bool condition = false;
        for (const char* w : kConditions)
            if (has(bare, w)) { condition = true; break; }
        if (!condition) return true;
        if (bare.find("raise") != std::string::npos) return true;
        // A returnsErr() entry's own err list ("err: \"not found\" (a missing
        // folder), ...") documents the second form.
        const size_t lead = bare.find_first_not_of(' ');
        if (nilErr && lead != std::string::npos && bare.compare(lead, 4, "err:") == 0) return true;
        return nilErr && (c.find("nil, \"") != std::string::npos || c.find("nil, err") != std::string::npos);
    };
    std::string clause;
    for (const char* q = text;; ++q) {
        const bool end = *q == '\0';
        // ". " and "; " end a clause ("1.5", "file.lua", "0..7" don't).
        const bool stop = end || *q == '\n' ||
                          ((*q == '.' || *q == ';') && (q[1] == ' ' || q[1] == '\0' || q[1] == '\n'));
        if (!stop) { clause += *q; continue; }
        if (!judge(clause)) {
            const size_t first = clause.find_first_not_of(' ');
            return first == std::string::npos ? clause : clause.substr(first);
        }
        clause.clear();
        if (end) break;
    }
    return {};
}

void validateConvention(const LuaApiModule& module, const LuaApiEntry& e,
                        const LuaApiSignature* sig, const std::function<void(const std::string&)>& problem) {
    const std::string name = e.name ? e.name : "";
    const bool constantLike = e.kind == LuaApiKind::Constant || e.kind == LuaApiKind::Table;
    if (module.scope == LuaApiScope::Metatable) {
        if (name.rfind("__", 0) != 0) problem("a metatable field is a __metamethod (ADR-0030)");
    } else if (hasUnderscore(name) && !(constantLike && isUpperSnake(name))) {
        problem("snake_case name; Lua names are camelCase (ADR-0030)");
    }
    if (!sig) return;
    for (const auto& o : sig->overloads) {
        for (const auto& prm : o.params)
            if (prm.name != "_" && hasUnderscore(prm.name))
                problem("snake_case parameter '" + prm.name + "' (ADR-0030)");
        for (const auto& r : o.results)
            if (hasUnderscore(r.name)) problem("snake_case result '" + r.name + "' (ADR-0030)");
    }
    if (e.kind != LuaApiKind::Function) return;
    if (e.failure == LuaApiFailure::NilErr) {
        for (const auto& o : sig->overloads) {
            const auto& r = o.results;
            if (r.size() < 2 || r.back().name != "err" || r.back().type != "string" ||
                !r.back().optional || !r.front().optional)
                problem("returns nil, err: its results end in 'x?, err:string?' (ADR-0030)");
        }
    } else {
        for (const auto& o : sig->overloads)
            for (const auto& r : o.results)
                if (r.name == "err")
                    problem("returns an err but is not marked returnsErr() (ADR-0030)");
    }
    for (const char* text : {e.summary, e.hostNote, e.args}) {
        const std::string clause = failureOutsideTheTwoForms(text, e.failure == LuaApiFailure::NilErr);
        if (!clause.empty())
            problem("documents a failure ('" + clause +
                    "') that is neither nil, err (returnsErr()) nor a raise (ADR-0030)");
    }
}

void validateEntry(const LuaApiModule& module, const LuaApiEntry& e, std::string& errors) {
    const std::string where = std::string(module.path) + (*module.path ? "." : "") +
                              (e.name ? e.name : "");
    auto problem = [&](const std::string& what) { errors += where + ": " + what + "\n"; };

    if (!e.name || !*e.name) problem("empty name");
    if (!e.summary || !*e.summary) problem("missing summary");

    switch (e.kind) {
        case LuaApiKind::Constant:
            validateConvention(module, e, nullptr, problem);
            return;
        case LuaApiKind::Table:
            validateConvention(module, e, nullptr, problem);
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
    validateConvention(module, e, &sig, problem);

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
    // The path's parts: camelCase, or UPPER_SNAKE for a table of constants
    // (gfx.COLOR); a Methods / Metatable path names a userdata type.
    if (module.scope == LuaApiScope::Table) {
        std::string path = module.path;
        size_t start = 0;
        while (start <= path.size()) {
            size_t dot = path.find('.', start);
            if (dot == std::string::npos) dot = path.size();
            const std::string part = path.substr(start, dot - start);
            if (hasUnderscore(part) && !isUpperSnake(part))
                errors += path + ": snake_case table name '" + part +
                          "'; Lua names are camelCase (ADR-0030)\n";
            start = dot + 1;
        }
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
