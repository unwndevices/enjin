#pragma once

// "Return a table" files (Tomodachi #154, #423): small Lua files whose one job
// is to return a table of data, like skin.lua and palette files. One sandbox
// for all of them: text only (no bytecode), an empty environment (no
// libraries), an instruction limit, a PSRAM-preferring Lua state on device,
// and everything under lua_pcall, so running out of memory fails the read
// instead of aborting.

#include <cstddef>
#include <functional>
#include <string>

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
/// @p error set to refuse it.
using TableFieldReader = std::function<bool(lua_State* L, int table, std::string* error)>;

/**
 * @brief Run @p source in the sandbox and hand the table it returns to @p read.
 *
 * @p what names the file in messages ("palette file", "skin.lua"): the chunk's
 * name, and "<what> must return a table".
 *
 * @return what @p read returns; false with the Lua error in @p error when the
 * chunk does not load, raises, runs past the instruction limit or returns no
 * table.
 */
bool readTableFile(const std::string& source, const char* what, const TableFieldReader& read,
                   std::string* error);

} // namespace enjin2
