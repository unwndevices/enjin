/**
 * @file tilemap_asset.hpp
 * @brief Shared tilemap cell packing + .njm map binary format (issue #41).
 *
 * One tilemap format serves both targets (device + web sim). A tileset is a
 * plain .njn sprite sheet (see sprite_asset.hpp); a map is a .njm file of
 * 16-bit cells. This header is the single source of truth for the cell bit
 * layout so C_Tilemap, the .njm loader, the aseprite2enjin tool, and the
 * web-sim codec all agree.
 *
 * Cell layout (uint16, MSB→LSB): [band:1 | vflip:1 | hflip:1 | palbank:4 | tileid:9]
 *
 *   - tileid  (bits 0-8) : 9 bits → 512 tiles. Tile id 0 is transparent.
 *   - palbank (bits 9-12): 4 bits → reserved in v1 (authors emit 0).
 *   - hflip   (bit 13)   : reserved in v1 (stored, not acted on).
 *   - vflip   (bit 14)   : reserved in v1 (stored, not acted on).
 *   - band    (bit 15)   : 0 = under (renders on L0), 1 = over (renders on L2).
 *                          v1 HONORS band via the presenter's restore filter.
 *
 * .njm file layout:
 *   bytes 0-1 : magic "NM" (0x4E, 0x4D)
 *   byte  2   : version (1)
 *   byte  3   : reserved (0)
 *   byte  4   : mapW (columns, in tiles)
 *   byte  5   : mapH (rows, in tiles)
 *   bytes 6-7 : reserved (0, 0)
 *   bytes 8+  : mapW * mapH little-endian uint16 cells, row-major (row 0 first)
 */
#pragma once

#include <cstdint>
#include <cstddef>

namespace enjin2 {

// ---------------------------------------------------------------------------
// Cell bit layout — the single source of truth (see file header).
// ---------------------------------------------------------------------------

// ---------------------------------------------------------------------------
// Tile attributes (ADR-0003 §3)
// ---------------------------------------------------------------------------

/// Cardinal direction of a directional attribute (one-way facing / conveyor).
/// The authored 2-bit value is resolved to a concrete direction at fetch by the
/// cell's flip flags (see tmResolveDir).
enum class TileDir : uint8_t { Up = 0, Right = 1, Down = 2, Left = 3 };

/**
 * @brief Per-tile attribute record, keyed by tile id (the model Tiled / LDtk /
 *        Godot TileData / pokeemerald all use — attributes live in the tileset,
 *        never in the map cell).
 *
 * A flat array of these (tile 0, 1, …) backs the collision/query API. On disk
 * it is the `.njn` v2 `ATTR` chunk (see njn2.hpp); at runtime it is the flat
 * table owned by C_Tilemap.
 *
 * Layout (2 bytes):
 *   - `flags` bit0 = SOLID, bit1 = ONEWAY, bits2-3 = DIR (cardinal), bits4-7 reserved
 *   - `kind`  = full uint8 (256 kinds — material / trigger id, polled).
 */
struct TileAttr {
    static constexpr uint8_t FLAG_SOLID  = 0x01;   ///< bit0 — the tile blocks movement
    static constexpr uint8_t FLAG_ONEWAY = 0x02;   ///< bit1 — one-way platform
    static constexpr uint8_t DIR_SHIFT   = 2;      ///< DIR lives in bits 2-3
    static constexpr uint8_t DIR_MASK    = 0x0C;   ///< 0b1100 pre-shift mask

    uint8_t flags{0};   ///< SOLID | ONEWAY | DIR:2 | reserved
    uint8_t kind{0};    ///< 0–255 material / trigger kind

    constexpr bool solid() const { return (flags & FLAG_SOLID) != 0; }
    constexpr bool oneway() const { return (flags & FLAG_ONEWAY) != 0; }
    /// @brief Authored/resolved 2-bit direction (see TileDir), unshifted (0-3).
    constexpr uint8_t dir() const { return static_cast<uint8_t>((flags >> DIR_SHIFT) & 0x03u); }
};

/**
 * @brief Resolve an authored DIR by the cell's flip flags (the Godot model).
 *
 * Horizontally flipping a directional attribute swaps East↔West (1↔3);
 * vertically flipping swaps North↔South (0↔2). Tiles that carry a direction
 * must therefore keep it transform-resolved at query time, not baked at author
 * time.
 *
 * @param dir   Authored 2-bit direction (0-3).
 * @param hflip Cell horizontal flip flag.
 * @param vflip Cell vertical flip flag.
 * @return The resolved direction (0-3).
 */
constexpr uint8_t tmResolveDir(uint8_t dir, bool hflip, bool vflip) {
    if (hflip && ((dir == static_cast<uint8_t>(TileDir::Right)) ||
                  (dir == static_cast<uint8_t>(TileDir::Left)))) {
        dir ^= 0x02u;   // Right(1) <-> Left(3)
    }
    if (vflip && ((dir == static_cast<uint8_t>(TileDir::Up)) ||
                  (dir == static_cast<uint8_t>(TileDir::Down)))) {
        dir ^= 0x02u;   // Up(0) <-> Down(2)
    }
    return dir & 0x03u;
}

static constexpr uint16_t TM_TILEID_BITS   = 9;
static constexpr uint16_t TM_TILEID_MASK   = 0x01FF;              ///< bits 0-8
static constexpr uint16_t TM_MAX_TILES     = 1 << TM_TILEID_BITS; ///< 512 tile ids
static constexpr uint16_t TM_PALBANK_SHIFT = 9;
static constexpr uint16_t TM_PALBANK_MASK  = 0x000F;             ///< 4 bits, pre-shift
static constexpr uint16_t TM_PALBANK_COUNT = 1 << 4;             ///< 16 palette banks
static constexpr uint16_t TM_HFLIP_BIT     = 1U << 13;
static constexpr uint16_t TM_VFLIP_BIT     = 1U << 14;
static constexpr uint16_t TM_BAND_BIT      = 1U << 15;

/// @brief Tile id (0-511). Id 0 = transparent (skipped in draw).
static constexpr uint16_t tmCellTileId(uint16_t cell) { return cell & TM_TILEID_MASK; }
/// @brief Palette bank (0-15). Reserved in v1.
static constexpr uint8_t  tmCellPalbank(uint16_t cell) {
    return static_cast<uint8_t>((cell >> TM_PALBANK_SHIFT) & TM_PALBANK_MASK);
}
/// @brief Horizontal-flip flag. Reserved in v1 (stored, not acted on).
static constexpr bool tmCellHFlip(uint16_t cell) { return (cell & TM_HFLIP_BIT) != 0; }
/// @brief Vertical-flip flag. Reserved in v1 (stored, not acted on).
static constexpr bool tmCellVFlip(uint16_t cell) { return (cell & TM_VFLIP_BIT) != 0; }
/// @brief Band bit: 0 = under (L0), 1 = over (L2). Honored in v1.
static constexpr uint8_t tmCellBand(uint16_t cell) {
    return static_cast<uint8_t>((cell & TM_BAND_BIT) != 0 ? 1 : 0);
}

/// @brief Pack a cell from its fields. Fields are masked to their widths.
static constexpr uint16_t tmPackCell(uint16_t tileId, uint8_t band, uint8_t palbank,
                                     bool hflip, bool vflip) {
    return static_cast<uint16_t>(
        (tileId & TM_TILEID_MASK) |
        (static_cast<uint16_t>(palbank & TM_PALBANK_MASK) << TM_PALBANK_SHIFT) |
        (hflip ? TM_HFLIP_BIT : 0) |
        (vflip ? TM_VFLIP_BIT : 0) |
        (band != 0 ? TM_BAND_BIT : 0));
}

// ---------------------------------------------------------------------------
// .njm map header (8 bytes)
// ---------------------------------------------------------------------------

struct NjmHeader {
    uint8_t magic[2];   ///< Must be {'N','M'}
    uint8_t version;    ///< Format version (currently 1)
    uint8_t reserved0;  ///< Reserved, must be 0
    uint8_t mapW;       ///< Map width in tiles (columns)
    uint8_t mapH;       ///< Map height in tiles (rows)
    uint8_t reserved1;  ///< Reserved, must be 0
    uint8_t reserved2;  ///< Reserved, must be 0
};

static_assert(sizeof(NjmHeader) == 8, "NjmHeader must be exactly 8 bytes");

static constexpr uint8_t NJM_MAGIC_0 = 'N';
static constexpr uint8_t NJM_MAGIC_1 = 'M';
static constexpr uint8_t NJM_VERSION = 1;

/// @brief Number of cells described by a parsed header.
static constexpr uint32_t njmCellCount(const NjmHeader& h) {
    return static_cast<uint32_t>(h.mapW) * h.mapH;
}

/**
 * @brief Parse and validate a .njm header from raw bytes.
 * @param data Pointer to at least 8 bytes.
 * @param size Total buffer size.
 * @param[out] out Parsed header on success.
 * @return true if magic/version/dims are valid and the buffer holds all cells.
 */
inline bool parseNjmHeader(const uint8_t* data, size_t size, NjmHeader& out) {
    if (data == nullptr || size < sizeof(NjmHeader)) {
        return false;
    }
    out.magic[0]  = data[0];
    out.magic[1]  = data[1];
    out.version   = data[2];
    out.reserved0 = data[3];
    out.mapW      = data[4];
    out.mapH      = data[5];
    out.reserved1 = data[6];
    out.reserved2 = data[7];
    if (out.magic[0] != NJM_MAGIC_0 || out.magic[1] != NJM_MAGIC_1) {
        return false;
    }
    if (out.version != NJM_VERSION) {
        return false;
    }
    if (out.mapW == 0 || out.mapH == 0) {
        return false;
    }
    // 2 bytes per cell.
    if (size < sizeof(NjmHeader) + (njmCellCount(out) * 2U)) {
        return false;
    }
    return true;
}

/**
 * @brief Read cell (tx,ty) from a validated .njm buffer (little-endian).
 * @param data  Pointer to the .njm buffer (header + cells).
 * @param h     Parsed header.
 * @param tx    Column.
 * @param ty    Row.
 * @return The 16-bit cell, or 0 if out of bounds.
 */
inline uint16_t njmCellAt(const uint8_t* data, const NjmHeader& h,
                          uint16_t tx, uint16_t ty) {
    if (tx >= h.mapW || ty >= h.mapH) {
        return 0;
    }
    const uint32_t idx = (static_cast<uint32_t>(ty) * h.mapW) + tx;
    const uint8_t* p = data + sizeof(NjmHeader) + (idx * 2U);
    return static_cast<uint16_t>(p[0] | (static_cast<uint16_t>(p[1]) << 8));
}

} // namespace enjin2
