/**
 * @file gc_assert_test.cpp
 * @brief Unit tests for GC control bindings and component dependency assertion
 *
 * Tests cover:
 *   GC-01: engine.lua.collect() is callable from Lua without error
 *   GC-02: engine.lua.memory() returns a non-negative number (Lua heap bytes)
 *   DEP-01: assertRequires<T>() is a no-op when required component T is present
 *   DEP-03: assertRequires<T>() disables component when T is missing (release builds only)
 *
 * Note: DEP-02 (debug assert() fires) is verified by code inspection only.
 * Invoking assert(false) in a debug build would abort the test process.
 * DEP-03 live path is gated with #ifdef NDEBUG — excluded from debug builds.
 */
#include <enjin2/scripting/bindings.hpp>
#include <enjin2/scripting/lua_engine.hpp>
#include <enjin2/core/object.hpp>
#include <enjin2/core/component.hpp>
#include <enjin2/components/drawable.hpp>
#include <cstdio>

using namespace enjin2;

static int passes = 0;
static int failures = 0;

#define ASSERT(cond, msg) \
    do { \
        if (!(cond)) { \
            fprintf(stderr, "FAIL: %s\n", msg); \
            failures++; \
        } else { \
            passes++; \
        } \
    } while(0)

// ============================================================
// Minimal test components for DEP-01 / DEP-03
// ============================================================

/** A simple component that other components can declare as a dependency. */
class C_TestDep : public Component {
public:
    explicit C_TestDep(Object* owner) : Component(owner) {}
};

/** A component that asserts it requires C_TestDep via assertRequires<C_TestDep>(). */
class C_RequiresTestDep : public Component {
public:
    explicit C_RequiresTestDep(Object* owner) : Component(owner) {}
    void awake() override {
        this->template assertRequires<C_TestDep>();
    }
};

// ============================================================
// GCFixture: minimal LuaEngine + LuaBindings with no canvas
// ============================================================
struct GCFixture {
    LuaEngine engine;
    LuaBindings bindings;

    GCFixture() : bindings(&engine) {
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
// GC-01: engine.lua.collect() callable without error
// ============================================================
static void test_gc01_collect_no_error() {
    printf("--- GC-01: engine.lua.collect() callable ---\n");
    GCFixture f;

    // Verify engine.lua.collect is a function
    LuaResult r2 = f.exec("is_func = (type(engine.lua.collect) == 'function') and 1 or 0");
    ASSERT(r2.success, "GC-01: is_func exec should succeed");
    double val = f.getNum("is_func");
    ASSERT(val == 1.0, "GC-01: engine.lua.collect must be a function");

    // Call collect() — must not error
    LuaResult r3 = f.exec("engine.lua.collect()");
    ASSERT(r3.success, "GC-01: engine.lua.collect() must execute without error");
}

// ============================================================
// GC-02: engine.lua.memory() returns non-negative number
// ============================================================
static void test_gc02_memory_returns_number() {
    printf("--- GC-02: engine.lua.memory() returns bytes ---\n");
    GCFixture f;

    LuaResult r = f.exec("mem = engine.lua.memory()");
    ASSERT(r.success, "GC-02: engine.lua.memory() exec should succeed");
    double mem = f.getNum("mem");
    ASSERT(mem >= 0.0, "GC-02: engine.lua.memory() must return non-negative value");
}

// ============================================================
// DEP-01: assertRequires<T>() happy path — no-op when dep present
// ============================================================
static void test_dep01_happy_path() {
    printf("--- DEP-01: assertRequires happy path (component present) ---\n");

    Object obj;
    obj.addComponent<C_TestDep>();
    C_RequiresTestDep* c = obj.addComponent<C_RequiresTestDep>();
    ASSERT(c != nullptr, "DEP-01: addComponent<C_RequiresTestDep> should succeed");

    // obj.awake() triggers C_RequiresTestDep::awake() — should not abort
    // C_TestDep is present, so assertRequires<C_TestDep>() is a no-op
    obj.awake();

    ASSERT(c->isEnabled(), "DEP-01: component should remain enabled when dep is present");
    passes++;  // reaching here means no abort occurred
    (void)c;
}

// ============================================================
// DEP-03: release path — missing dep disables component
// Only compiled and run in release builds (NDEBUG defined).
// In debug builds, assertRequires<T>() for a missing dep calls assert(false),
// which would abort the test process — deliberately excluded here.
// ============================================================
#ifdef NDEBUG
static void test_dep03_release_missing_disables() {
    printf("--- DEP-03: release path disables on missing dep ---\n");

    Object obj;
    // Add C_RequiresTestDep but NOT C_TestDep
    C_RequiresTestDep* c = obj.addComponent<C_RequiresTestDep>();
    ASSERT(c != nullptr, "DEP-03: addComponent should succeed");
    ASSERT(c->isEnabled(), "DEP-03: component enabled before awake");

    obj.awake();  // triggers assertRequires — missing dep, release path: disables component

    ASSERT(!c->isEnabled(), "DEP-03: component must be disabled after missing dep in release");
}
#endif

// ============================================================
// main
// ============================================================
int main()
{
    printf("gc_assert_test\n");
    printf("==============\n");

    test_gc01_collect_no_error();
    test_gc02_memory_returns_number();
    test_dep01_happy_path();

#ifdef NDEBUG
    test_dep03_release_missing_disables();
#endif

    printf("\nResults: %d passed, %d failed\n", passes, failures);
    return failures == 0 ? 0 : 1;
}
