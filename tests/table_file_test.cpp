// "Return a table" files (Tomodachi #154, #423, #279): the one sandbox every
// short-lived Lua read goes through (skin.lua, palette files, applet
// manifests). Whatever the file does, the read returns: a loop is cut off, a
// file that eats memory runs out of its own budget, and running out of memory
// anywhere (making the state, opening a validator's libraries, the file
// itself) is an error, never an abort.
#include <enjin2/graphics/table_file.hpp>

extern "C" {
#include "lua.h"
}

#include <cstdio>
#include <string>

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
    } while (0)

// The integer at t[key], or -1.
static long fieldInt(lua_State* L, int table, const char* key)
{
    lua_getfield(L, table, key);
    const long v = lua_isinteger(L, -1) ? static_cast<long>(lua_tointeger(L, -1)) : -1;
    lua_pop(L, 1);
    return v;
}

static bool readN(const std::string& source, long& n, std::string& error,
                  const TableFileOptions& options = {})
{
    n = -1;
    return readTableFile(
        source, "data file",
        [&n](lua_State* L, int table, std::string*) {
            n = fieldInt(L, table, "n");
            return true;
        },
        &error, options);
}

static void test_a_file_that_loops_forever_is_cut_off()
{
    long n;
    std::string error;
    ASSERT(!readN("while true do end", n, error), "an endless loop fails the read");
    ASSERT(error.find("instruction limit") != std::string::npos, "and says it ran into the instruction limit");
}

static void test_a_file_that_eats_memory_runs_out_of_its_budget()
{
    // Doubling a string: a few instructions, a gigabyte by the 30th.
    long n;
    std::string error;
    ASSERT(!readN("local s = 'x' for i = 1, 30 do s = s .. s end return { n = #s }", n, error),
           "a file that outgrows the budget fails the read");
    ASSERT(error.find("not enough memory") != std::string::npos, "with Lua's out of memory error");
    TableFileOptions small;
    small.memoryLimit = 64 * 1024;
    ASSERT(!readN("local s = 'x' for i = 1, 17 do s = s .. s end return { n = #s }", n, error, small),
           "128 KB of string does not fit a 64 KB budget");
    ASSERT(readN("local s = 'x' for i = 1, 10 do s = s .. s end return { n = #s }", n, error, small) && n == 1024,
           "1 KB of it does");
}

// Every budget from nothing up: making the state, loading the chunk, running
// it, each allocation in turn is the one that fails. Each read returns (an
// abort ends the test), with an error until the budget fits, then the table.
static void sweepBudgets(const char* label, const std::string& source, long expected,
                         const TableFileOptions& base = {})
{
    bool everFailed = false, cleanErrors = true, fitted = false;
    for (size_t limit = 0; limit <= 256 * 1024 && !fitted; limit += 16) {
        TableFileOptions options = base;
        options.memoryLimit = limit;
        long n;
        std::string error;
        if (readN(source, n, error, options)) {
            fitted = n == expected;
        } else {
            everFailed = true;
            if (error.empty()) cleanErrors = false;
        }
    }
    char msg[160];
    std::snprintf(msg, sizeof msg, "%s: small budgets fail the read", label);
    ASSERT(everFailed, msg);
    std::snprintf(msg, sizeof msg, "%s: each failure says why", label);
    ASSERT(cleanErrors, msg);
    std::snprintf(msg, sizeof msg, "%s: a budget that fits reads the table", label);
    ASSERT(fitted, msg);
}

static void test_running_out_of_memory_anywhere_is_an_error_never_an_abort()
{
    sweepBudgets("a data file", "local t = {} for i = 1, 200 do t[i] = 'k' .. i end return { n = #t }", 200);
}

// A validator (Tomodachi's applet manifest contract): trusted Lua that gets the
// file's text and the base, string and math libraries, runs the file in an
// empty environment itself and returns the table to read.
static const char* kValidator =
    "local source = ...\n"
    "local chunk = assert(load(source, 'data file', 't', {}))\n"
    "local t = chunk()\n"
    "assert(type(t) == 'table', 'must return a table')\n"
    "assert(math.type(t.n) == 'integer', 'n must be an integer')\n"
    "return { n = t.n + #(string.rep('x', 3)), files = (dofile or loadfile) and 1 or 0 }\n";

static void test_a_validator_reads_the_file_with_its_libraries()
{
    TableFileOptions withValidator;
    withValidator.validator = kValidator;
    long n;
    std::string error;
    ASSERT(readN("return { n = 4 }", n, error, withValidator) && n == 7,
           "the validator runs the file and returns its own table");
    long files = -1;
    readTableFile(
        "return { n = 1 }", "data file",
        [&files](lua_State* L, int table, std::string*) {
            files = fieldInt(L, table, "files");
            return true;
        },
        &error, withValidator);
    ASSERT(files == 0, "the validator has no file loaders (dofile, loadfile)");
    ASSERT(!readN("return { n = 1.5 }", n, error, withValidator) &&
               error.find("n must be an integer") != std::string::npos,
           "the validator's refusal is the error");
    ASSERT(!readN("return { n = string.rep('x', 2) }", n, error, withValidator),
           "the file itself still has no libraries");
    ASSERT(!readN("while true do end", n, error, withValidator) &&
               error.find("instruction limit") != std::string::npos,
           "a file that loops forever under a validator is cut off too");
    ASSERT(!readN("return { n = #('x'):rep(1 << 28) }", n, error, withValidator) &&
               error.find("not enough memory") != std::string::npos,
           "string methods reach the file through the string metatable; the budget still holds");
    sweepBudgets("a validated file", "return { n = 4 }", 7, withValidator);
}

int main()
{
    test_a_file_that_loops_forever_is_cut_off();
    test_a_file_that_eats_memory_runs_out_of_its_budget();
    test_running_out_of_memory_anywhere_is_an_error_never_an_abort();
    test_a_validator_reads_the_file_with_its_libraries();
    printf("\n%d passed, %d failed\n", passes, failures);
    return failures ? 1 : 0;
}
