/**
 * @file store_test.cpp
 * @brief Tests for engine.store.* persistent key-value store bindings
 *
 * Verifies:
 * - engine.store table exists and is a table
 * - save/load round-trip: number, string, boolean
 * - save/load round-trip: Lua table with mixed value types
 * - exists() returns true/false correctly
 * - delete() removes a key
 * - clear() removes all keys
 * - 16-key limit enforcement
 * - load() on non-existent key returns nil
 * - File persistence (saveToFile / loadFromFile)
 */
#include <enjin2/scripting/bindings.hpp>
#include <enjin2/scripting/lua_engine.hpp>
#include <cstdio>
#include <cstring>
#include <cstdlib>

using namespace enjin2;

static int passes = 0;
static int failures = 0;

#define ASSERT(cond, msg) \
    do { \
        if (!(cond)) { \
            fprintf(stderr, "FAIL [line %d]: %s\n", __LINE__, (msg)); \
            failures++; \
        } else { \
            passes++; \
        } \
    } while(0)

// ============================================================
// Minimal fixture: LuaEngine + LuaBindings, no canvas, no input
// ============================================================
struct StoreFixture {
    LuaEngine engine;
    LuaBindings bindings;

    StoreFixture() : bindings(&engine) {
        engine.initialize();
        bindings.registerAll();
    }

    LuaResult exec(const char* code) {
        return engine.executeString(code);
    }

    double getNum(const char* name) {
        return engine.getGlobalNumber(name);
    }
};

// ============================================================
// test_store_table_exists
// ============================================================
static void test_store_table_exists() {
    printf("--- engine.store table exists ---\n");

    StoreFixture f;
    LuaResult r = f.exec(
        "ok_store  = (engine.store ~= nil) and 1 or 0\n"
        "ok_type   = (type(engine.store) == 'table') and 1 or 0\n"
        "ok_save   = (type(engine.store.save) == 'function') and 1 or 0\n"
        "ok_load   = (type(engine.store.load) == 'function') and 1 or 0\n"
        "ok_exists = (type(engine.store.exists) == 'function') and 1 or 0\n"
        "ok_delete = (type(engine.store.delete) == 'function') and 1 or 0\n"
        "ok_clear  = (type(engine.store.clear) == 'function') and 1 or 0\n"
    );
    ASSERT(r.success, "store table access should not error");
    ASSERT(f.getNum("ok_store") == 1.0, "engine.store should be non-nil");
    ASSERT(f.getNum("ok_type") == 1.0,  "engine.store should be a table");
    ASSERT(f.getNum("ok_save") == 1.0,  "engine.store.save should be a function");
    ASSERT(f.getNum("ok_load") == 1.0,  "engine.store.load should be a function");
    ASSERT(f.getNum("ok_exists") == 1.0,"engine.store.exists should be a function");
    ASSERT(f.getNum("ok_delete") == 1.0,"engine.store.delete should be a function");
    ASSERT(f.getNum("ok_clear") == 1.0, "engine.store.clear should be a function");
}

// ============================================================
// test_store_number_roundtrip
// ============================================================
static void test_store_number_roundtrip() {
    printf("--- store number round-trip ---\n");

    StoreFixture f;
    LuaResult r = f.exec(
        "engine.store.save('highscore', 500)\n"
        "hs = engine.store.load('highscore')\n"
    );
    ASSERT(r.success, "number save/load should not error");
    ASSERT(f.getNum("hs") == 500.0, "loaded number should be 500");
}

// ============================================================
// test_store_string_roundtrip
// ============================================================
static void test_store_string_roundtrip() {
    printf("--- store string round-trip ---\n");

    StoreFixture f;
    LuaResult r = f.exec(
        "engine.store.save('name', 'Player1')\n"
        "n = engine.store.load('name')\n"
        "ok = (n == 'Player1') and 1 or 0\n"
    );
    ASSERT(r.success, "string save/load should not error");
    ASSERT(f.getNum("ok") == 1.0, "loaded string should be 'Player1'");
}

// ============================================================
// test_store_boolean_roundtrip
// ============================================================
static void test_store_boolean_roundtrip() {
    printf("--- store boolean round-trip ---\n");

    StoreFixture f;
    LuaResult r = f.exec(
        "engine.store.save('unlocked', true)\n"
        "engine.store.save('locked', false)\n"
        "u = engine.store.load('unlocked')\n"
        "l = engine.store.load('locked')\n"
        "ok_u = (u == true) and 1 or 0\n"
        "ok_l = (l == false) and 1 or 0\n"
    );
    ASSERT(r.success, "boolean save/load should not error");
    ASSERT(f.getNum("ok_u") == 1.0, "loaded true should be true");
    ASSERT(f.getNum("ok_l") == 1.0, "loaded false should be false");
}

// ============================================================
// test_store_table_roundtrip
// ============================================================
static void test_store_table_roundtrip() {
    printf("--- store table round-trip ---\n");

    StoreFixture f;
    LuaResult r = f.exec(
        "engine.store.save('progress', {level=3, score=1200, name='hero', done=true})\n"
        "p = engine.store.load('progress')\n"
        "ok_type  = (type(p) == 'table') and 1 or 0\n"
        "ok_level = (p.level == 3) and 1 or 0\n"
        "ok_score = (p.score == 1200) and 1 or 0\n"
        "ok_name  = (p.name == 'hero') and 1 or 0\n"
        "ok_done  = (p.done == true) and 1 or 0\n"
    );
    ASSERT(r.success, "table save/load should not error");
    ASSERT(f.getNum("ok_type") == 1.0,  "loaded value should be a table");
    ASSERT(f.getNum("ok_level") == 1.0, "table.level should be 3");
    ASSERT(f.getNum("ok_score") == 1.0, "table.score should be 1200");
    ASSERT(f.getNum("ok_name") == 1.0,  "table.name should be 'hero'");
    ASSERT(f.getNum("ok_done") == 1.0,  "table.done should be true");
}

// ============================================================
// test_store_exists
// ============================================================
static void test_store_exists() {
    printf("--- store exists ---\n");

    StoreFixture f;
    LuaResult r = f.exec(
        "engine.store.save('key1', 42)\n"
        "e1 = engine.store.exists('key1') and 1 or 0\n"
        "e2 = engine.store.exists('nope') and 1 or 0\n"
    );
    ASSERT(r.success, "exists() should not error");
    ASSERT(f.getNum("e1") == 1.0, "exists() should return true for saved key");
    ASSERT(f.getNum("e2") == 0.0, "exists() should return false for missing key");
}

// ============================================================
// test_store_delete
// ============================================================
static void test_store_delete() {
    printf("--- store delete ---\n");

    StoreFixture f;
    LuaResult r = f.exec(
        "engine.store.save('del_me', 99)\n"
        "d1 = engine.store.delete('del_me') and 1 or 0\n"
        "gone = (engine.store.load('del_me') == nil) and 1 or 0\n"
        "d2 = engine.store.delete('del_me') and 1 or 0\n"
    );
    ASSERT(r.success, "delete() should not error");
    ASSERT(f.getNum("d1") == 1.0, "delete() should return true for existing key");
    ASSERT(f.getNum("gone") == 1.0, "load() after delete should return nil");
    ASSERT(f.getNum("d2") == 0.0, "delete() should return false for already-deleted key");
}

// ============================================================
// test_store_clear
// ============================================================
static void test_store_clear() {
    printf("--- store clear ---\n");

    StoreFixture f;
    LuaResult r = f.exec(
        "engine.store.save('a', 1)\n"
        "engine.store.save('b', 2)\n"
        "engine.store.save('c', 3)\n"
        "engine.store.clear()\n"
        "ok_a = (engine.store.load('a') == nil) and 1 or 0\n"
        "ok_b = (engine.store.load('b') == nil) and 1 or 0\n"
        "ok_c = (engine.store.load('c') == nil) and 1 or 0\n"
    );
    ASSERT(r.success, "clear() should not error");
    ASSERT(f.getNum("ok_a") == 1.0, "load('a') after clear should return nil");
    ASSERT(f.getNum("ok_b") == 1.0, "load('b') after clear should return nil");
    ASSERT(f.getNum("ok_c") == 1.0, "load('c') after clear should return nil");
}

// ============================================================
// test_store_key_limit
// ============================================================
static void test_store_key_limit() {
    printf("--- store 16-key limit ---\n");

    StoreFixture f;

    // Fill 16 keys
    std::string code;
    for (int i = 0; i < 16; ++i) {
        char buf[64];
        snprintf(buf, sizeof(buf), "engine.store.save('k%d', %d)\n", i, i);
        code += buf;
    }
    code += "ok_16 = engine.store.save('k16', 999)\n";
    code += "overflow = (ok_16 == false) and 1 or 0\n";

    LuaResult r = f.exec(code.c_str());
    ASSERT(r.success, "16-key limit test should not error");
    ASSERT(f.getNum("overflow") == 1.0, "17th save should return false (at capacity)");
}

// ============================================================
// test_store_load_nil
// ============================================================
static void test_store_load_nil() {
    printf("--- store load nil ---\n");

    StoreFixture f;
    LuaResult r = f.exec(
        "v = engine.store.load('nonexistent')\n"
        "ok = (v == nil) and 1 or 0\n"
    );
    ASSERT(r.success, "load() on missing key should not error");
    ASSERT(f.getNum("ok") == 1.0, "load() should return nil for non-existent key");
}

// ============================================================
// test_store_overwrite
// ============================================================
static void test_store_overwrite() {
    printf("--- store overwrite ---\n");

    StoreFixture f;
    LuaResult r = f.exec(
        "engine.store.save('ow', 10)\n"
        "engine.store.save('ow', 20)\n"
        "v = engine.store.load('ow')\n"
    );
    ASSERT(r.success, "overwrite should not error");
    ASSERT(f.getNum("v") == 20.0, "overwritten value should be 20");
}

// ============================================================
// test_store_file_persistence (C++ level)
// ============================================================
static void test_store_file_persistence() {
    printf("--- store file persistence ---\n");

    const char* tmpPath = "/tmp/enjin_store_test.json";

    // Write
    LuaStore store1;
    store1.setNumber("score", 1000);
    store1.setString("player", "Alice");
    store1.setBool("complete", true);
    auto* tbl = store1.setTable("stats");
    ASSERT(tbl != nullptr, "setTable should succeed");
    if (tbl) {
        tbl->tableEntries[0] = LuaStore::TableEntry{};
        strncpy(tbl->tableEntries[0].key, "kills", LuaStore::STORE_MAX_KEY - 1);
        tbl->tableEntries[0].type = LuaStore::StoreType::Number;
        tbl->tableEntries[0].numVal = 42;
        tbl->tableCount = 1;
    }
    bool saved = store1.saveToFile(tmpPath);
    ASSERT(saved, "saveToFile should succeed");

    // Read back
    LuaStore store2;
    bool loaded = store2.loadFromFile(tmpPath);
    ASSERT(loaded, "loadFromFile should succeed");
    ASSERT(store2.count() == 4, "loaded store should have 4 entries");

    const auto* score = store2.get("score");
    ASSERT(score != nullptr, "score should exist");
    ASSERT(score && score->type == LuaStore::StoreType::Number, "score should be number");
    ASSERT(score && score->numVal == 1000.0, "score should be 1000");

    const auto* player = store2.get("player");
    ASSERT(player != nullptr, "player should exist");
    ASSERT(player && player->type == LuaStore::StoreType::String, "player should be string");
    ASSERT(player && strcmp(player->strVal, "Alice") == 0, "player should be 'Alice'");

    const auto* complete = store2.get("complete");
    ASSERT(complete != nullptr, "complete should exist");
    ASSERT(complete && complete->type == LuaStore::StoreType::Bool, "complete should be bool");
    ASSERT(complete && complete->boolVal == true, "complete should be true");

    const auto* stats = store2.get("stats");
    ASSERT(stats != nullptr, "stats table should exist");
    ASSERT(stats && stats->type == LuaStore::StoreType::Table, "stats should be table");
    ASSERT(stats && stats->tableCount == 1, "stats should have 1 entry");
    ASSERT(stats && strcmp(stats->tableEntries[0].key, "kills") == 0, "table key should be 'kills'");
    ASSERT(stats && stats->tableEntries[0].numVal == 42.0, "kills should be 42");

    // Cleanup
    remove(tmpPath);
}

// ============================================================
// test_store_flush_and_path_functions_exist (Phase 48: STORE-02)
// ============================================================
static void test_store_flush_and_path_functions_exist() {
    printf("--- engine.store.flush and path exist ---\n");

    StoreFixture f;
    LuaResult r = f.exec(
        "ok_flush = (type(engine.store.flush) == 'function') and 1 or 0\n"
        "ok_path  = (type(engine.store.path) == 'function') and 1 or 0\n"
    );
    ASSERT(r.success, "flush/path function checks should not error");
    ASSERT(f.getNum("ok_flush") == 1.0, "engine.store.flush should be a function");
    ASSERT(f.getNum("ok_path") == 1.0,  "engine.store.path should be a function");
}

// ============================================================
// test_store_flush_no_path (Phase 48: STORE-02)
// ============================================================
static void test_store_flush_no_path() {
    printf("--- engine.store.flush() with no path returns false ---\n");

    StoreFixture f;
    LuaResult r = f.exec(
        "result = engine.store.flush()\n"
        "ok = (result == false) and 1 or 0\n"
    );
    ASSERT(r.success, "flush() with no path should not error");
    ASSERT(f.getNum("ok") == 1.0, "flush() with no path should return false");
}

// ============================================================
// test_store_flush_with_path (Phase 48: STORE-02)
// ============================================================
static void test_store_flush_with_path() {
    printf("--- engine.store.flush() with path saves and returns true ---\n");

    const char* tmpPath = "/tmp/enjin_test_flush.json";

    // First fixture: set path, save data, flush
    {
        StoreFixture f;
        LuaResult r = f.exec(
            "engine.store.path('/tmp/enjin_test_flush.json')\n"
            "engine.store.save('level', 42)\n"
            "flush_result = engine.store.flush()\n"
            "ok_flush = (flush_result == true) and 1 or 0\n"
        );
        ASSERT(r.success, "flush() with path should not error");
        ASSERT(f.getNum("ok_flush") == 1.0, "flush() should return true after path is set");
    }

    // Second fixture: verify the file was written by loading from the same path
    {
        StoreFixture f2;
        LuaResult r2 = f2.exec(
            "engine.store.path('/tmp/enjin_test_flush.json')\n"
            "v = engine.store.load('level')\n"
            "ok = (v == 42) and 1 or 0\n"
        );
        ASSERT(r2.success, "loading from flushed path should not error");
        ASSERT(f2.getNum("ok") == 1.0, "loaded level should be 42 from flushed file");
    }

    remove(tmpPath);
}

// ============================================================
// test_store_path_loads_existing (Phase 48: STORE-02)
// ============================================================
static void test_store_path_loads_existing() {
    printf("--- engine.store.path() loads existing data automatically ---\n");

    const char* tmpPath = "/tmp/enjin_test_path_load.json";

    // First fixture: save a file via C++ API directly
    {
        LuaStore store;
        store.setNumber("checkpoint", 7);
        store.setString("hero", "Enjin");
        bool saved = store.saveToFile(tmpPath);
        ASSERT(saved, "C++ saveToFile should succeed for path test setup");
    }

    // Second fixture: call engine.store.path() and verify data is loaded
    {
        StoreFixture f;
        LuaResult r = f.exec(
            "engine.store.path('/tmp/enjin_test_path_load.json')\n"
            "v_cp = engine.store.load('checkpoint')\n"
            "v_h  = engine.store.load('hero')\n"
            "ok_cp = (v_cp == 7) and 1 or 0\n"
            "ok_h  = (v_h == 'Enjin') and 1 or 0\n"
        );
        ASSERT(r.success, "path() with existing file should not error");
        ASSERT(f.getNum("ok_cp") == 1.0, "checkpoint should be 7 after path()");
        ASSERT(f.getNum("ok_h") == 1.0,  "hero should be 'Enjin' after path()");
    }

    remove(tmpPath);
}

// ============================================================
// main
// ============================================================
int main() {
    printf("=== store_test ===\n");

    test_store_table_exists();
    test_store_number_roundtrip();
    test_store_string_roundtrip();
    test_store_boolean_roundtrip();
    test_store_table_roundtrip();
    test_store_exists();
    test_store_delete();
    test_store_clear();
    test_store_key_limit();
    test_store_load_nil();
    test_store_overwrite();
    test_store_file_persistence();

    // Phase 48: STORE-02 — flush() and path() bindings
    test_store_flush_and_path_functions_exist();
    test_store_flush_no_path();
    test_store_flush_with_path();
    test_store_path_loads_existing();

    printf("\n=== Results: %d passed, %d failed ===\n", passes, failures);
    return (failures == 0) ? 0 : 1;
}
