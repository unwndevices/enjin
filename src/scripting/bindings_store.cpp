#include "../../include/enjin2/scripting/bindings.hpp"
#include "../../include/enjin2/scripting/bind_helpers.hpp"
#include <cstdio>
#include <cstring>

#if !defined(ESP32) && !defined(__EMSCRIPTEN__)
    #include <fstream>
    #include <sstream>
#endif

namespace enjin2 {

//==============================================================================
// LuaStore Implementation
//==============================================================================

LuaStore::LuaStore() : m_count(0) {}

void LuaStore::clear() {
    for (int i = 0; i < m_count; ++i) {
        m_entries[i] = StoreSlot{};
    }
    m_count = 0;
}

bool LuaStore::exists(const char* key) const {
    return findIndex(key) >= 0;
}

bool LuaStore::remove(const char* key) {
    int idx = findIndex(key);
    if (idx < 0) return false;
    // Shift remaining entries down
    for (int i = idx; i < m_count - 1; ++i) {
        m_entries[i] = m_entries[i + 1];
    }
    m_entries[m_count - 1] = StoreSlot{};
    --m_count;
    return true;
}

bool LuaStore::setNumber(const char* key, double value) {
    StoreSlot* slot = findOrCreate(key);
    if (!slot) return false;
    slot->type = StoreType::Number;
    slot->numVal = value;
    slot->strVal[0] = '\0';
    slot->tableCount = 0;
    return true;
}

bool LuaStore::setString(const char* key, const char* value) {
    StoreSlot* slot = findOrCreate(key);
    if (!slot) return false;
    slot->type = StoreType::String;
    strncpy(slot->strVal, value, STORE_MAX_STRING - 1);
    slot->strVal[STORE_MAX_STRING - 1] = '\0';
    slot->tableCount = 0;
    return true;
}

bool LuaStore::setBool(const char* key, bool value) {
    StoreSlot* slot = findOrCreate(key);
    if (!slot) return false;
    slot->type = StoreType::Bool;
    slot->boolVal = value;
    slot->strVal[0] = '\0';
    slot->tableCount = 0;
    return true;
}

LuaStore::StoreSlot* LuaStore::setTable(const char* key) {
    StoreSlot* slot = findOrCreate(key);
    if (!slot) return nullptr;
    slot->type = StoreType::Table;
    slot->tableCount = 0;
    return slot;
}

const LuaStore::StoreSlot* LuaStore::get(const char* key) const {
    int idx = findIndex(key);
    if (idx < 0) return nullptr;
    return &m_entries[idx];
}

int LuaStore::findIndex(const char* key) const {
    for (int i = 0; i < m_count; ++i) {
        if (strcmp(m_entries[i].key, key) == 0) {
            return i;
        }
    }
    return -1;
}

LuaStore::StoreSlot* LuaStore::findOrCreate(const char* key) {
    int idx = findIndex(key);
    if (idx >= 0) return &m_entries[idx];
    if (m_count >= STORE_MAX_KEYS) return nullptr;
    StoreSlot* slot = &m_entries[m_count++];
    *slot = StoreSlot{};
    strncpy(slot->key, key, STORE_MAX_KEY - 1);
    slot->key[STORE_MAX_KEY - 1] = '\0';
    return slot;
}

//==============================================================================
// Buffer-based JSON serialization (shared — all platforms)
//==============================================================================

static size_t bufWriteChar(char* out, size_t cap, size_t pos, char c) {
    if (pos + 1 < cap) out[pos] = c;
    return pos + 1;
}

static size_t bufWriteStr(char* out, size_t cap, size_t pos, const char* s) {
    while (*s) {
        if (pos + 1 < cap) out[pos] = *s;
        ++pos; ++s;
    }
    return pos;
}

static size_t bufWriteJsonEscaped(char* out, size_t cap, size_t pos, const char* s) {
    pos = bufWriteChar(out, cap, pos, '"');
    for (const char* p = s; *p; ++p) {
        switch (*p) {
            case '"':  pos = bufWriteStr(out, cap, pos, "\\\""); break;
            case '\\': pos = bufWriteStr(out, cap, pos, "\\\\"); break;
            case '\n': pos = bufWriteStr(out, cap, pos, "\\n");  break;
            case '\r': pos = bufWriteStr(out, cap, pos, "\\r");  break;
            case '\t': pos = bufWriteStr(out, cap, pos, "\\t");  break;
            default:   pos = bufWriteChar(out, cap, pos, *p);    break;
        }
    }
    return bufWriteChar(out, cap, pos, '"');
}

static size_t bufWriteSlotValue(char* out, size_t cap, size_t pos,
                                const LuaStore::StoreSlot& slot) {
    switch (slot.type) {
        case LuaStore::StoreType::Number: {
            char numBuf[64];
            int n = snprintf(numBuf, sizeof(numBuf), "%g", slot.numVal);
            for (int j = 0; j < n; ++j)
                pos = bufWriteChar(out, cap, pos, numBuf[j]);
            break;
        }
        case LuaStore::StoreType::String:
            pos = bufWriteJsonEscaped(out, cap, pos, slot.strVal);
            break;
        case LuaStore::StoreType::Bool:
            pos = bufWriteStr(out, cap, pos, slot.boolVal ? "true" : "false");
            break;
        case LuaStore::StoreType::Table: {
            pos = bufWriteChar(out, cap, pos, '{');
            for (int i = 0; i < slot.tableCount; ++i) {
                if (i > 0) pos = bufWriteChar(out, cap, pos, ',');
                const auto& te = slot.tableEntries[i];
                pos = bufWriteJsonEscaped(out, cap, pos, te.key);
                pos = bufWriteChar(out, cap, pos, ':');
                // Build a temporary StoreSlot to reuse bufWriteSlotValue recursively
                LuaStore::StoreSlot tmp{};
                tmp.type = te.type;
                tmp.numVal = te.numVal;
                tmp.boolVal = te.boolVal;
                strncpy(tmp.strVal, te.strVal, LuaStore::STORE_MAX_STRING - 1);
                tmp.strVal[LuaStore::STORE_MAX_STRING - 1] = '\0';
                tmp.tableCount = 0;
                pos = bufWriteSlotValue(out, cap, pos, tmp);
            }
            pos = bufWriteChar(out, cap, pos, '}');
            break;
        }
        default:
            pos = bufWriteStr(out, cap, pos, "null");
            break;
    }
    return pos;
}

bool LuaStore::writeStoreToBuffer(char* out, size_t cap) const {
    size_t pos = 0;
    pos = bufWriteChar(out, cap, pos, '{');
    for (int i = 0; i < m_count; ++i) {
        if (i > 0) pos = bufWriteChar(out, cap, pos, ',');
        pos = bufWriteJsonEscaped(out, cap, pos, m_entries[i].key);
        pos = bufWriteChar(out, cap, pos, ':');
        pos = bufWriteSlotValue(out, cap, pos, m_entries[i]);
    }
    pos = bufWriteChar(out, cap, pos, '}');
    if (pos < cap) {
        out[pos] = '\0';
        return true;
    }
    if (cap > 0) out[cap - 1] = '\0';
    return false;
}

//==============================================================================
// JSON File I/O (Desktop only — excluded on ESP32 and WASM)
//==============================================================================

#if !defined(ESP32) && !defined(__EMSCRIPTEN__)

// Minimal JSON writer — only needs to handle our restricted types
static void writeJsonEscaped(std::ofstream& out, const char* s) {
    out << '"';
    for (const char* p = s; *p; ++p) {
        switch (*p) {
            case '"':  out << "\\\""; break;
            case '\\': out << "\\\\"; break;
            case '\n': out << "\\n"; break;
            case '\r': out << "\\r"; break;
            case '\t': out << "\\t"; break;
            default:   out << *p; break;
        }
    }
    out << '"';
}

static void writeSlotValue(std::ofstream& out, const LuaStore::StoreSlot& slot) {
    switch (slot.type) {
        case LuaStore::StoreType::Number:
            out << slot.numVal;
            break;
        case LuaStore::StoreType::String:
            writeJsonEscaped(out, slot.strVal);
            break;
        case LuaStore::StoreType::Bool:
            out << (slot.boolVal ? "true" : "false");
            break;
        case LuaStore::StoreType::Table: {
            out << "{";
            for (int i = 0; i < slot.tableCount; ++i) {
                if (i > 0) out << ",";
                const auto& te = slot.tableEntries[i];
                writeJsonEscaped(out, te.key);
                out << ":";
                // Table entries are flat — write inline
                LuaStore::StoreSlot tmp{};
                tmp.type = te.type;
                tmp.numVal = te.numVal;
                tmp.boolVal = te.boolVal;
                strncpy(tmp.strVal, te.strVal, LuaStore::STORE_MAX_STRING - 1);
                tmp.strVal[LuaStore::STORE_MAX_STRING - 1] = '\0';
                tmp.tableCount = 0;
                writeSlotValue(out, tmp);
            }
            out << "}";
            break;
        }
        default:
            out << "null";
            break;
    }
}

bool LuaStore::saveToFile(const char* path) const {
    std::ofstream out(path);
    if (!out.is_open()) return false;

    out << "{";
    bool first = true;
    for (int i = 0; i < m_count; ++i) {
        if (!first) out << ",";
        first = false;
        out << "\n  ";
        writeJsonEscaped(out, m_entries[i].key);
        out << ": ";
        writeSlotValue(out, m_entries[i]);
    }
    out << "\n}\n";
    return out.good();
}

// Minimal JSON reader — whitespace-tolerant, handles our restricted value types
static void skipWhitespace(const char*& p) {
    while (*p == ' ' || *p == '\t' || *p == '\n' || *p == '\r') ++p;
}

static bool readJsonString(const char*& p, char* out, int maxLen) {
    if (*p != '"') return false;
    ++p;
    int i = 0;
    while (*p && *p != '"') {
        if (*p == '\\') {
            ++p;
            switch (*p) {
                case '"': if (i < maxLen - 1) out[i++] = '"'; break;
                case '\\': if (i < maxLen - 1) out[i++] = '\\'; break;
                case 'n': if (i < maxLen - 1) out[i++] = '\n'; break;
                case 'r': if (i < maxLen - 1) out[i++] = '\r'; break;
                case 't': if (i < maxLen - 1) out[i++] = '\t'; break;
                default: if (i < maxLen - 1) out[i++] = *p; break;
            }
        } else {
            if (i < maxLen - 1) out[i++] = *p;
        }
        ++p;
    }
    out[i] = '\0';
    if (*p == '"') ++p;
    return true;
}

static bool readJsonValue(const char*& p, LuaStore::StoreSlot& slot);

static bool readJsonObject(const char*& p, LuaStore::StoreSlot& slot) {
    if (*p != '{') return false;
    ++p;
    slot.type = LuaStore::StoreType::Table;
    slot.tableCount = 0;
    skipWhitespace(p);
    if (*p == '}') { ++p; return true; }

    while (*p) {
        skipWhitespace(p);
        if (slot.tableCount >= LuaStore::STORE_MAX_TABLE_ENTRIES) return false;
        auto& te = slot.tableEntries[slot.tableCount];
        if (!readJsonString(p, te.key, LuaStore::STORE_MAX_KEY)) return false;
        skipWhitespace(p);
        if (*p != ':') return false;
        ++p;
        skipWhitespace(p);
        // Read value into a temp slot to extract type/value
        LuaStore::StoreSlot tmp{};
        if (!readJsonValue(p, tmp)) return false;
        te.type = tmp.type;
        te.numVal = tmp.numVal;
        te.boolVal = tmp.boolVal;
        strncpy(te.strVal, tmp.strVal, LuaStore::STORE_MAX_STRING - 1);
        te.strVal[LuaStore::STORE_MAX_STRING - 1] = '\0';
        slot.tableCount++;
        skipWhitespace(p);
        if (*p == ',') { ++p; continue; }
        if (*p == '}') { ++p; return true; }
        return false;
    }
    return false;
}

static bool readJsonValue(const char*& p, LuaStore::StoreSlot& slot) {
    skipWhitespace(p);
    if (*p == '"') {
        slot.type = LuaStore::StoreType::String;
        return readJsonString(p, slot.strVal, LuaStore::STORE_MAX_STRING);
    } else if (*p == '{') {
        return readJsonObject(p, slot);
    } else if (*p == 't' && strncmp(p, "true", 4) == 0) {
        slot.type = LuaStore::StoreType::Bool;
        slot.boolVal = true;
        p += 4;
        return true;
    } else if (*p == 'f' && strncmp(p, "false", 5) == 0) {
        slot.type = LuaStore::StoreType::Bool;
        slot.boolVal = false;
        p += 5;
        return true;
    } else if (*p == 'n' && strncmp(p, "null", 4) == 0) {
        p += 4;
        return true;
    } else if (*p == '-' || (*p >= '0' && *p <= '9')) {
        slot.type = LuaStore::StoreType::Number;
        char* end = nullptr;
        slot.numVal = strtod(p, &end);
        if (end == p) return false;
        p = end;
        return true;
    }
    return false;
}

bool LuaStore::loadFromFile(const char* path) {
    std::ifstream in(path);
    if (!in.is_open()) return false;

    std::string content((std::istreambuf_iterator<char>(in)),
                         std::istreambuf_iterator<char>());
    in.close();

    clear();
    const char* p = content.c_str();
    skipWhitespace(p);
    if (*p != '{') return false;
    ++p;
    skipWhitespace(p);
    if (*p == '}') return true;

    while (*p) {
        skipWhitespace(p);
        if (m_count >= STORE_MAX_KEYS) return false;
        StoreSlot& slot = m_entries[m_count];
        slot = StoreSlot{};
        if (!readJsonString(p, slot.key, STORE_MAX_KEY)) return false;
        skipWhitespace(p);
        if (*p != ':') return false;
        ++p;
        skipWhitespace(p);
        if (!readJsonValue(p, slot)) return false;
        m_count++;
        skipWhitespace(p);
        if (*p == ',') { ++p; continue; }
        if (*p == '}') break;
        return false;
    }
    return true;
}

#else
// ESP32 stub (STORE-03) / WASM stub (STORE-04) — deferred
bool LuaStore::saveToFile(const char*) const { return false; }
bool LuaStore::loadFromFile(const char*) { return false; }
#endif

//==============================================================================
// Lua Binding Functions
//==============================================================================

// Helper: push a StoreSlot value onto the Lua stack
static void pushStoreValue(lua_State* L, const LuaStore::StoreSlot& slot) {
    switch (slot.type) {
        case LuaStore::StoreType::Number:
            lua_pushnumber(L, static_cast<lua_Number>(slot.numVal));
            break;
        case LuaStore::StoreType::String:
            lua_pushstring(L, slot.strVal);
            break;
        case LuaStore::StoreType::Bool:
            lua_pushboolean(L, slot.boolVal ? 1 : 0);
            break;
        case LuaStore::StoreType::Table:
            lua_newtable(L);
            for (int i = 0; i < slot.tableCount; ++i) {
                const auto& te = slot.tableEntries[i];
                LuaStore::StoreSlot tmp{};
                tmp.type = te.type;
                tmp.numVal = te.numVal;
                tmp.boolVal = te.boolVal;
                strncpy(tmp.strVal, te.strVal, LuaStore::STORE_MAX_STRING - 1);
                tmp.strVal[LuaStore::STORE_MAX_STRING - 1] = '\0';
                tmp.tableCount = 0;
                pushStoreValue(L, tmp);
                lua_setfield(L, -2, te.key);
            }
            break;
        default:
            lua_pushnil(L);
            break;
    }
}

// Helper: read Lua value at stack index into a StoreSlot (returns false on unsupported type)
static bool popStoreTableEntry(lua_State* L, int idx, LuaStore::StoreSlot& slot);

static bool readLuaTable(lua_State* L, int idx, LuaStore::StoreSlot& slot) {
    slot.type = LuaStore::StoreType::Table;
    slot.tableCount = 0;

    lua_pushnil(L);  // first key
    while (lua_next(L, idx) != 0) {
        // key at -2, value at -1
        if (!lua_isstring(L, -2)) {
            lua_pop(L, 2);  // pop key + value
            return false;  // only string keys supported
        }
        if (slot.tableCount >= LuaStore::STORE_MAX_TABLE_ENTRIES) {
            lua_pop(L, 2);
            return false;
        }
        auto& te = slot.tableEntries[slot.tableCount];
        const char* k = lua_tostring(L, -2);
        strncpy(te.key, k, LuaStore::STORE_MAX_KEY - 1);
        te.key[LuaStore::STORE_MAX_KEY - 1] = '\0';

        int vtype = lua_type(L, -1);
        if (vtype == LUA_TNUMBER) {
            te.type = LuaStore::StoreType::Number;
            te.numVal = lua_tonumber(L, -1);
        } else if (vtype == LUA_TSTRING) {
            te.type = LuaStore::StoreType::String;
            const char* s = lua_tostring(L, -1);
            strncpy(te.strVal, s, LuaStore::STORE_MAX_STRING - 1);
            te.strVal[LuaStore::STORE_MAX_STRING - 1] = '\0';
        } else if (vtype == LUA_TBOOLEAN) {
            te.type = LuaStore::StoreType::Bool;
            te.boolVal = lua_toboolean(L, -1) != 0;
        } else {
            lua_pop(L, 2);  // skip unsupported types
            continue;
        }
        slot.tableCount++;
        lua_pop(L, 1);  // pop value, keep key for next lua_next
    }
    return true;
}

// --- engine.store.save(key, value) ---
// Returns true on success, false if at 16-key capacity or invalid args
int LuaBindings::lua_engine_store_save(lua_State* L) {
    LuaBindings* b = getBindings(L);
    if (!b) { lua_pushboolean(L, 0); return 1; }

    const char* key = luaL_checkstring(L, 1);
    int vtype = lua_type(L, 2);
    bool ok = false;

    switch (vtype) {
        case LUA_TNUMBER:
            ok = b->m_store.setNumber(key, lua_tonumber(L, 2));
            break;
        case LUA_TSTRING:
            ok = b->m_store.setString(key, lua_tostring(L, 2));
            break;
        case LUA_TBOOLEAN:
            ok = b->m_store.setBool(key, lua_toboolean(L, 2) != 0);
            break;
        case LUA_TTABLE: {
            LuaStore::StoreSlot* slot = b->m_store.setTable(key);
            if (slot) {
                ok = readLuaTable(L, 2, *slot);
            }
            break;
        }
        default:
            luaL_error(L, "engine.store.save: unsupported value type '%s'",
                       lua_typename(L, vtype));
            return 0;
    }

    if (!ok) {
        printf("[engine.store] warning: save('%s') failed (capacity: %d/%d)\n",
               key, b->m_store.count(), LuaStore::STORE_MAX_KEYS);
    }

    // Auto-persist to file if store path is set
    if (ok && b->m_storePath[0] != '\0') {
        b->m_store.saveToFile(b->m_storePath);
    }

    lua_pushboolean(L, ok ? 1 : 0);
    return 1;
}

// --- engine.store.load(key) ---
// Returns the stored value, or nil if not found
int LuaBindings::lua_engine_store_load(lua_State* L) {
    LuaBindings* b = getBindings(L);
    if (!b) { lua_pushnil(L); return 1; }

    const char* key = luaL_checkstring(L, 1);
    const LuaStore::StoreSlot* slot = b->m_store.get(key);
    if (!slot) {
        lua_pushnil(L);
        return 1;
    }
    pushStoreValue(L, *slot);
    return 1;
}

// --- engine.store.exists(key) ---
// Returns boolean
int LuaBindings::lua_engine_store_exists(lua_State* L) {
    LuaBindings* b = getBindings(L);
    if (!b) { lua_pushboolean(L, 0); return 1; }

    const char* key = luaL_checkstring(L, 1);
    lua_pushboolean(L, b->m_store.exists(key) ? 1 : 0);
    return 1;
}

// --- engine.store.delete(key) ---
// Returns boolean (true if key existed and was removed)
int LuaBindings::lua_engine_store_delete(lua_State* L) {
    LuaBindings* b = getBindings(L);
    if (!b) { lua_pushboolean(L, 0); return 1; }

    const char* key = luaL_checkstring(L, 1);
    bool removed = b->m_store.remove(key);

    // Auto-persist
    if (removed && b->m_storePath[0] != '\0') {
        b->m_store.saveToFile(b->m_storePath);
    }

    lua_pushboolean(L, removed ? 1 : 0);
    return 1;
}

// --- engine.store.clear() ---
// Removes all stored data
int LuaBindings::lua_engine_store_clear(lua_State* L) {
    LuaBindings* b = getBindings(L);
    if (!b) return 0;

    b->m_store.clear();

    // Auto-persist (writes empty store)
    if (b->m_storePath[0] != '\0') {
        b->m_store.saveToFile(b->m_storePath);
    }

    return 0;
}

// --- engine.store.flush() --- (Phase 48: STORE-02)
// Explicitly writes the current store to disk. Returns true on success, false if no path set.
int LuaBindings::lua_engine_store_flush(lua_State* L) {
    LuaBindings* b = getBindings(L);
    if (!b) { lua_pushboolean(L, 0); return 1; }
    if (b->m_storePath[0] == '\0') { lua_pushboolean(L, 0); return 1; }  // no path set
    bool ok = b->m_store.saveToFile(b->m_storePath);
    lua_pushboolean(L, ok ? 1 : 0);
    return 1;
}

// --- engine.store.path(filepath) --- (Phase 48: STORE-02)
// Sets the save file path at runtime and loads any existing data from the new path.
int LuaBindings::lua_engine_store_path(lua_State* L) {
    LuaBindings* b = getBindings(L);
    if (!b) return 0;
    const char* path = luaL_checkstring(L, 1);
    strncpy(b->m_storePath, path, sizeof(b->m_storePath) - 1);
    b->m_storePath[sizeof(b->m_storePath) - 1] = '\0';
    b->m_store.loadFromFile(b->m_storePath);  // load existing data; silent no-op if file missing
    return 0;
}

} // namespace enjin2
