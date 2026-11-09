#pragma once

// "Return a table" files (Tomodachi #154, #423, #279): small Lua files whose
// one job is to return a table of data, like skin.lua, palette files and
// applet manifests. One sandbox for every short-lived Lua state that reads
// one: text only (no bytecode), an empty environment (no libraries; a
// validator, when there is one, gets base, string and math), an instruction
// limit, a memory budget, a PSRAM-preferring Lua state on device, and making
// the state, opening the libraries, loading and running all under lua_pcall,
// so running out of memory fails the read instead of aborting.

#include <cstddef>
#include <functional>
#include <string>
#include <string_view>

struct lua_State;

namespace enjin2 {

/**
 * @brief Read a small text file whole: a regular, non-empty file of at most
 * @p limit bytes with no NUL byte.
 *
 * @return true and @p out filled; on failure false, @p out empty and a
 * one-line reason in @p error ("not a file", "empty or larger than 4 KB",
 * "cannot open", "cannot read", "has a NUL byte").
 */
bool readTableFileText(const std::string& path, size_t limit, std::string& out,
                       std::string* error);

/// Reads the returned table, which sits at stack index @p table. false with
/// @p error set to refuse it. It runs after the protected part (a Lua error's
/// longjmp would skip a C++ reader's destructors), with the memory budget
/// lifted for its few allocations, so it should only read: lua_getfield,
/// lua_next, lua_rawgeti, lua_tolstring on strings.
using TableFieldReader = std::function<bool(lua_State* L, int table, std::string* error)>;

/// What a read may spend. The defaults fit every file Tomodachi reads.
struct TableFileOptions {
    /// Trusted Lua that reads the file instead of running it: it gets the
    /// file's text as its one argument and the base (without dofile and
    /// loadfile), string and math libraries, runs the file itself (in an
    /// empty environment: `load(source, name, 't', {})`), checks it, and
    /// returns the table to read. Tomodachi's applet manifest contract is
    /// one. nullptr: the file runs itself, in an empty environment.
    const char* validator = nullptr;
    /// Bytes the Lua state may hold while it is made and the file loads and
    /// runs; an allocation past it fails, and the read with "not enough
    /// memory". It bounds a file that eats memory (a string doubled in a
    /// loop) before it can take the heap, PSRAM or internal, from the device.
    size_t memoryLimit = 1024 * 1024;
};

/**
 * @brief Run @p source in the sandbox and hand the table it returns to @p read.
 *
 * @p what names the file in messages ("palette file", "skin.lua"): the chunk's
 * name, and "<what> must return a table".
 *
 * @return what @p read returns; false with the Lua error in @p error when the
 * Lua state cannot be made, the chunk (or the validator) does not load,
 * raises, runs past the instruction limit (100 000 VM instructions, the
 * same for every file), outgrows the memory budget ("not
 * enough memory") or returns no table.
 */
bool readTableFile(std::string_view source, const char* what, const TableFieldReader& read,
                   std::string* error, const TableFileOptions& options = {});

} // namespace enjin2
