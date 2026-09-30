/**
 * @file lua_api_stdlib.hpp
 * @brief The Lua standard library a script can reach, one line per name
 *
 * ADR-0013 (Tomodachi): the stdlib is not registered from descriptors, so its
 * reference is this short hand-kept list: every standard name a script sees
 * after LuaPlatform opens the libraries and applies the sandbox
 * (LuaPlatformConfig::SANDBOXED_GLOBALS), the same on every host. Lua 5.4 is
 * built without LUA_COMPAT_5_3, so math.pow, math.atan2 and friends are absent.
 *
 * `print` is not listed: the bindings replace it and describe their own.
 * Tomodachi's _G census (libtomo test_lua_api_census.cpp) fails on a standard
 * name missing here and on a name listed here that a script cannot reach.
 */
#pragma once

#include <cstddef>
#include <string>

namespace enjin2 {

struct LuaStdlibEntry {
    const char* name;     ///< As reached from _G: "pairs", "string", "string.format"
    const char* summary;  ///< One line
    const char* anchor;   ///< Manual anchor; nullptr = "pdf-<name>"
};

inline constexpr LuaStdlibEntry kLuaStdlib[] = {
    // Basic functions (manual §6.1)
    {"_G", "The global environment table.", nullptr},
    {"_VERSION", "The running Lua version, \"Lua 5.4\".", nullptr},
    {"assert", "Raises an error if its first argument is false or nil; else returns all arguments.", nullptr},
    {"collectgarbage", "Controls the garbage collector (\"collect\", \"count\", \"step\", ...).", nullptr},
    {"error", "Raises an error with a message or value, optionally at a stack level.", nullptr},
    {"getmetatable", "Returns an object's metatable (or its __metatable field).", nullptr},
    {"ipairs", "Iterates t[1], t[2], ... up to the first nil.", nullptr},
    {"load", "Compiles a chunk from a string or function; returns it as a function.", nullptr},
    {"next", "Returns the next key and value of a table after a given key.", nullptr},
    {"pairs", "Iterates every key and value of a table (honours __pairs).", nullptr},
    {"pcall", "Calls a function in protected mode; returns a status and its results or error.", nullptr},
    {"rawequal", "Compares two values without calling __eq.", nullptr},
    {"rawget", "Reads t[k] without calling __index.", nullptr},
    {"rawlen", "Length of a table or string without calling __len.", nullptr},
    {"rawset", "Writes t[k] = v without calling __newindex.", nullptr},
    {"select", "Returns the arguments after index n, or their count with \"#\".", nullptr},
    {"setmetatable", "Sets a table's metatable and returns the table.", nullptr},
    {"tonumber", "Converts a value to a number, optionally in a given base; nil if it cannot.", nullptr},
    {"tostring", "Converts a value to a string (honours __tostring and __name).", nullptr},
    {"type", "The type name of a value: \"nil\", \"number\", \"string\", \"table\", ...", nullptr},
    {"warn", "Emits a warning built from its string arguments.", nullptr},
    {"xpcall", "Like pcall, with a message handler called on error.", nullptr},

    // Coroutines (manual §6.2)
    {"coroutine", "Coroutine manipulation.", "6.2"},
    {"coroutine.close", "Closes a suspended or dead coroutine.", nullptr},
    {"coroutine.create", "Creates a coroutine from a function.", nullptr},
    {"coroutine.isyieldable", "True when the running coroutine can yield.", nullptr},
    {"coroutine.resume", "Starts or continues a coroutine; returns a status and its values.", nullptr},
    {"coroutine.running", "The running coroutine and whether it is the main one.", nullptr},
    {"coroutine.status", "\"running\", \"suspended\", \"normal\" or \"dead\".", nullptr},
    {"coroutine.wrap", "Creates a coroutine as a function that resumes it on each call.", nullptr},
    {"coroutine.yield", "Suspends the running coroutine, passing values to resume.", nullptr},

    // Strings (manual §6.4)
    {"string", "String manipulation; also the methods of every string (s:upper()).", "6.4"},
    {"string.byte", "The byte values of s[i..j].", nullptr},
    {"string.char", "A string built from byte values.", nullptr},
    {"string.dump", "The binary chunk of a Lua function.", nullptr},
    {"string.find", "Finds a pattern (or plain text) in a string; returns start, end, captures.", nullptr},
    {"string.format", "Formats values printf-style (%d, %5.2f, %s, %q, ...).", nullptr},
    {"string.gmatch", "Iterates the matches of a pattern.", nullptr},
    {"string.gsub", "Replaces pattern matches by a string, table or function; returns the count too.", nullptr},
    {"string.len", "Length of a string in bytes.", nullptr},
    {"string.lower", "A copy with letters lowercased.", nullptr},
    {"string.match", "The captures (or whole match) of a pattern.", nullptr},
    {"string.pack", "Packs values into a binary string by format.", nullptr},
    {"string.packsize", "Size of a string.pack result for a format.", nullptr},
    {"string.rep", "A string repeated n times, with an optional separator.", nullptr},
    {"string.reverse", "A string reversed.", nullptr},
    {"string.sub", "The substring s[i..j]; negative indices count from the end.", nullptr},
    {"string.unpack", "Unpacks values from a binary string by format.", nullptr},
    {"string.upper", "A copy with letters uppercased.", nullptr},

    // UTF-8 (manual §6.5)
    {"utf8", "UTF-8 encoding support.", "6.5"},
    {"utf8.char", "A UTF-8 string from code points.", nullptr},
    {"utf8.charpattern", "The pattern matching one UTF-8 byte sequence.", nullptr},
    {"utf8.codepoint", "The code points of s[i..j].", nullptr},
    {"utf8.codes", "Iterates the byte positions and code points of a string.", nullptr},
    {"utf8.len", "Number of UTF-8 characters in s[i..j], or nil and the first bad position.", nullptr},
    {"utf8.offset", "Byte position of the n-th character.", nullptr},

    // Tables (manual §6.6)
    {"table", "Table manipulation.", "6.6"},
    {"table.concat", "Joins list elements into a string with a separator.", nullptr},
    {"table.insert", "Inserts a value at the end of a list, or at a position.", nullptr},
    {"table.move", "Copies a range of elements, possibly to another table.", nullptr},
    {"table.pack", "A table of its arguments, with n = their count.", nullptr},
    {"table.remove", "Removes and returns a list element (the last by default).", nullptr},
    {"table.sort", "Sorts a list in place, with an optional comparator.", nullptr},
    {"table.unpack", "Returns the elements of a list as values.", nullptr},

    // Mathematics (manual §6.7)
    {"math", "Mathematical functions.", "6.7"},
    {"math.abs", "Absolute value.", nullptr},
    {"math.acos", "Arc cosine, in radians.", nullptr},
    {"math.asin", "Arc sine, in radians.", nullptr},
    {"math.atan", "Arc tangent of y/x, in radians, using both signs (x defaults to 1).", nullptr},
    {"math.ceil", "Smallest integer >= x.", nullptr},
    {"math.cos", "Cosine of an angle in radians.", nullptr},
    {"math.deg", "Radians to degrees.", nullptr},
    {"math.exp", "e raised to x.", nullptr},
    {"math.floor", "Largest integer <= x.", nullptr},
    {"math.fmod", "Remainder of x / y, rounded towards zero.", nullptr},
    {"math.huge", "A value larger than any other number (infinity).", nullptr},
    {"math.log", "Logarithm of x, natural or in a given base.", nullptr},
    {"math.max", "The largest of its arguments.", nullptr},
    {"math.maxinteger", "The largest integer.", nullptr},
    {"math.min", "The smallest of its arguments.", nullptr},
    {"math.mininteger", "The smallest integer.", nullptr},
    {"math.modf", "Integral and fractional parts of x.", nullptr},
    {"math.pi", "The value of pi.", nullptr},
    {"math.rad", "Degrees to radians.", nullptr},
    {"math.random", "A float in [0,1), or an integer in [1,m] or [m,n].", nullptr},
    {"math.randomseed", "Seeds the pseudo-random generator.", nullptr},
    {"math.sin", "Sine of an angle in radians.", nullptr},
    {"math.sqrt", "Square root.", nullptr},
    {"math.tan", "Tangent of an angle in radians.", nullptr},
    {"math.tointeger", "x as an integer if it has an exact one, else nil.", nullptr},
    {"math.type", "\"integer\", \"float\", or nil for a non-number.", nullptr},
    {"math.ult", "True if integer m < n when compared as unsigned.", nullptr},
};

inline constexpr size_t kLuaStdlibCount = sizeof(kLuaStdlib) / sizeof(kLuaStdlib[0]);

/// The Lua 5.4 reference manual entry for a stdlib name.
inline std::string luaStdlibManualUrl(const LuaStdlibEntry& entry) {
    std::string url = "https://www.lua.org/manual/5.4/manual.html#";
    if (entry.anchor) {
        url += entry.anchor;
    } else {
        url += "pdf-";
        url += entry.name;
    }
    return url;
}

} // namespace enjin2
