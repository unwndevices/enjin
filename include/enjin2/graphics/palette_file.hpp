#pragma once

#include "palette.hpp"

#include <string>

namespace enjin2 {

/**
 * @brief The role names a palette file uses, in index order (ADR-0019).
 *
 * `kPaletteRoleNames[ROLE_BG]` is "bg", ... `kPaletteRoleNames[ROLE_ACCENT4]`
 * is "accent4".
 */
extern const char* const kPaletteRoleNames[ROLE_COUNT];

/**
 * @brief A palette preset read from a palette file (Tomodachi #423).
 *
 * A palette file is a Lua chunk that returns a table:
 * @code
 * return {
 *   name = "Nyx8 Light", author = "Javier Guerrero",
 *   source = "https://lospec.com/palette-list/nyx8",
 *   bg = "#f6d6bd", surface = "#c3a38a", muted = "#997577", text = "#08141e",
 *   accent = "#0f2a3f", accent2 = "#20394f", accent3 = "#4e495f", accent4 = "#816271",
 * }
 * @endcode
 * `name` and the 8 roles are required, `author` and `source` are optional
 * credits. Nothing checks how the colours look together (no contrast check).
 */
struct PaletteFile {
    std::string name;            ///< Shown in Settings; unique key of a preset
    std::string author;          ///< Credit, may be empty
    std::string source;          ///< Where the palette comes from (URL), may be empty
    RGB colors[ROLE_COUNT];      ///< The 8 roles, indexed by ROLE_*
};

/**
 * @brief Read a palette file's source text.
 *
 * The chunk runs text-only in an empty environment under an instruction
 * limit, like the applet and skin manifests. Fails, with a one-line reason in
 * `error`, when the chunk does not return a table, `name` is missing or empty,
 * or a role is missing or not a `"#rrggbb"` string.
 *
 * @return true and `out` filled on success; on failure `out` is unspecified.
 */
bool parsePaletteFile(const std::string& source, PaletteFile& out, std::string* error);

/**
 * @brief Read a palette file from disk (a regular file of at most 4 KB),
 * then @ref parsePaletteFile.
 */
bool loadPaletteFile(const std::string& path, PaletteFile& out, std::string* error);

} // namespace enjin2
