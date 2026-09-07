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

static constexpr uint16_t TM_TILEID_BITS   = 9;
static constexpr uint16_t TM_TILEID_MASK   = 0x01FF;              ///< bits 0-8
static constexpr uint16_t TM_PALBANK_SHIFT = 9;
static constexpr uint16_t TM_PALBANK_MASK  = 0x000F;             ///< 4 bits, pre-shift
static constexpr uint16_t TM_HFLIP_BIT     = 1u << 13;
static constexpr uint16_t TM_VFLIP_BIT     = 1u << 14;
static constexpr uint16_t TM_BAND_BIT      = 1u << 15;

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
    return static_cast<uint8_t>((cell & TM_BAND_BIT) ? 1 : 0);
}

/// @brief Pack a cell from its fields. Fields are masked to their widths.
static constexpr uint16_t tmPackCell(uint16_t tileId, uint8_t band, uint8_t palbank,
                                     bool hflip, bool vflip) {
    return static_cast<uint16_t>(
        (tileId & TM_TILEID_MASK) |
        (static_cast<uint16_t>(palbank & TM_PALBANK_MASK) << TM_PALBANK_SHIFT) |
        (hflip ? TM_HFLIP_BIT : 0) |
        (vflip ? TM_VFLIP_BIT : 0) |
        (band  ? TM_BAND_BIT  : 0));
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
    if (!data || size < sizeof(NjmHeader)) return false;
    out.magic[0]  = data[0];
    out.magic[1]  = data[1];
    out.version   = data[2];
    out.reserved0 = data[3];
    out.mapW      = data[4];
    out.mapH      = data[5];
    out.reserved1 = data[6];
    out.reserved2 = data[7];
    if (out.magic[0] != NJM_MAGIC_0 || out.magic[1] != NJM_MAGIC_1) return false;
    if (out.version != NJM_VERSION) return false;
    if (out.mapW == 0 || out.mapH == 0) return false;
    // 2 bytes per cell.
    if (size < sizeof(NjmHeader) + njmCellCount(out) * 2u) return false;
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
    if (tx >= h.mapW || ty >= h.mapH) return 0;
    const uint32_t idx = static_cast<uint32_t>(ty) * h.mapW + tx;
    const uint8_t* p = data + sizeof(NjmHeader) + idx * 2u;
    return static_cast<uint16_t>(p[0] | (static_cast<uint16_t>(p[1]) << 8));
}

} // namespace enjin2
