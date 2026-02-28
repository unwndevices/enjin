/**
 * @file sprite_asset.hpp
 * @brief .njn binary sprite asset format: header, parsing, and file loading
 *
 * The .njn format stores 4-bit indexed-color sprite sheets as a compact
 * binary file suitable for both desktop and embedded (LittleFS/SD) loading.
 *
 * Layout:
 *   bytes 0-1  : magic "NJ" (0x4E, 0x4A)
 *   byte  2    : version (1)
 *   byte  3    : cellW   (cell width in pixels)
 *   byte  4    : cellH   (cell height in pixels)
 *   byte  5    : cols    (grid columns)
 *   byte  6    : rows    (grid rows)
 *   byte  7    : reserved (0)
 *   bytes 8+   : raw pixel data — 1 byte per pixel, lower nibble = palette index
 *                Total pixel bytes = cellW * cellH * cols * rows
 */
#pragma once

#include <cstdint>
#include <cstddef>

namespace enjin2 {

/// .njn file header (8 bytes)
struct NjnHeader {
    uint8_t magic[2];   ///< Must be {'N','J'}
    uint8_t version;    ///< Format version (currently 1)
    uint8_t cellW;      ///< Cell width in pixels
    uint8_t cellH;      ///< Cell height in pixels
    uint8_t cols;       ///< Sprite sheet grid columns
    uint8_t rows;       ///< Sprite sheet grid rows
    uint8_t reserved;   ///< Reserved, must be 0
};

static_assert(sizeof(NjnHeader) == 8, "NjnHeader must be exactly 8 bytes");

/// Result of loading a .njn file
struct SpriteAsset {
    NjnHeader header;
    const uint8_t* pixelData;   ///< Pointer into asset buffer (not owned by this struct)
    uint32_t pixelDataSize;     ///< Size in bytes of the pixel data
};

/// Magic bytes for .njn files
static constexpr uint8_t NJN_MAGIC_0 = 'N';
static constexpr uint8_t NJN_MAGIC_1 = 'J';
static constexpr uint8_t NJN_VERSION  = 1;

/**
 * @brief Parse and validate a .njn header from raw bytes
 * @param data   Pointer to at least 8 bytes of data
 * @param size   Total size of the data buffer
 * @param[out] out  Parsed header on success
 * @return true if header is valid (magic, version, sane dims, size sufficient for pixels)
 */
inline bool parseNjnHeader(const uint8_t* data, size_t size, NjnHeader& out) {
    if (!data || size < sizeof(NjnHeader)) return false;

    // Read header
    out.magic[0] = data[0];
    out.magic[1] = data[1];
    out.version  = data[2];
    out.cellW    = data[3];
    out.cellH    = data[4];
    out.cols     = data[5];
    out.rows     = data[6];
    out.reserved = data[7];

    // Validate magic
    if (out.magic[0] != NJN_MAGIC_0 || out.magic[1] != NJN_MAGIC_1) return false;

    // Validate version
    if (out.version != NJN_VERSION) return false;

    // Validate dimensions (nonzero)
    if (out.cellW == 0 || out.cellH == 0 || out.cols == 0 || out.rows == 0) return false;

    // Validate that buffer contains enough pixel data
    uint32_t pixelCount = static_cast<uint32_t>(out.cellW) * out.cellH * out.cols * out.rows;
    if (size < sizeof(NjnHeader) + pixelCount) return false;

    return true;
}

/**
 * @brief Compute total pixel data size from header fields
 */
inline uint32_t njnPixelDataSize(const NjnHeader& h) {
    return static_cast<uint32_t>(h.cellW) * h.cellH * h.cols * h.rows;
}

} // namespace enjin2
