/**
 * @file sprite_asset.hpp
 * @brief .njn binary sprite asset format: header, parsing, and file loading
 *
 * The .njn format stores 4-bit indexed-color sprite sheets as a compact
 * binary file suitable for both desktop and embedded (LittleFS/SD) loading.
 *
 * Layout:
 *   bytes 0-1  : magic "NJ" (0x4E, 0x4A)
 *   byte  2    : version (1 or 2)
 *   byte  3    : cellW   (cell width in pixels)
 *   byte  4    : cellH   (cell height in pixels)
 *   byte  5    : cols    (grid columns)
 *   byte  6    : rows    (grid rows)
 *   byte  7    : reserved (0)
 *   bytes 8+   : pixel data
 *
 * Version 1 pixel data: one byte per pixel, lower nibble = palette index.
 *   Total bytes = cellW * cellH * cols * rows.
 *
 * Version 2 pixel data (unwn #204): 4-bit nibble-packed, two pixels per byte,
 *   matching Canvas4's convention (the even/low-index pixel occupies the low
 *   nibble, the odd pixel the high nibble). An odd pixel count leaves the final
 *   high nibble as zero padding. Palette index 15 is reserved as transparent —
 *   the engine's blit-skip convention — leaving 15 paintable grays (0..14).
 *   Total bytes = ceil(cellW * cellH * cols * rows / 2).
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
    NjnHeader header;               ///< Parsed file header (magic, version, dimensions)
    const uint8_t* pixelData;   ///< Pointer into asset buffer (not owned by this struct)
    uint32_t pixelDataSize;     ///< Size in bytes of the pixel data
};

/// Magic bytes for .njn files
static constexpr uint8_t NJN_MAGIC_0 = 'N';
static constexpr uint8_t NJN_MAGIC_1 = 'J';
static constexpr uint8_t NJN_VERSION    = 1;  ///< Legacy byte-per-pixel format
static constexpr uint8_t NJN_VERSION_V2 = 2;  ///< 4-bit nibble-packed (unwn #204)

/// Palette index reserved as transparent in .njn v2 — the engine's blit-skip
/// convention, unifying the icon `matte` sentinel, the sprite widget, and .njn.
static constexpr uint8_t NJN_TRANSPARENT_INDEX = 15;

/// @brief Whether a palette index is the reserved transparent marker.
inline bool njnIsTransparent(uint8_t index) { return index == NJN_TRANSPARENT_INDEX; }

/// @brief Logical pixel count of a sheet (cellW * cellH * cols * rows).
inline uint32_t njnPixelCount(const NjnHeader& h) {
    return static_cast<uint32_t>(h.cellW) * h.cellH * h.cols * h.rows;
}

/// @brief Packed byte count for a v2 pixel plane: ceil(pixelCount / 2).
inline uint32_t njnPackedByteSize(uint32_t pixelCount) {
    return (pixelCount + 1u) / 2u;
}

/**
 * @brief Pack byte-per-pixel indices into 4-bit nibbles (Canvas4 order).
 *
 * The even (low-index) pixel goes to the low nibble, the odd pixel to the high
 * nibble. Only the low nibble of each input index is used. An odd @p count
 * leaves the final byte's high nibble zero-padded.
 *
 * @param indices  Source indices, one per byte (only bits 0-3 read)
 * @param count    Number of pixels
 * @param out      Destination buffer, must hold njnPackedByteSize(count) bytes
 * @return Number of packed bytes written
 */
inline size_t njnPackNibbles(const uint8_t* indices, size_t count, uint8_t* out) {
    if (!indices || !out) return 0;
    size_t bytes = 0;
    for (size_t i = 0; i < count; i += 2) {
        uint8_t low = indices[i] & 0x0F;
        uint8_t high = (i + 1 < count) ? (indices[i + 1] & 0x0F) : 0;
        out[bytes++] = static_cast<uint8_t>((high << 4) | low);
    }
    return bytes;
}

/**
 * @brief Unpack 4-bit nibbles back into byte-per-pixel indices.
 *
 * Inverse of njnPackNibbles; the trailing padding nibble of an odd @p count is
 * ignored. Output values are palette indices 0-15 (15 = transparent).
 *
 * @param packed  Source packed bytes (>= njnPackedByteSize(count))
 * @param count   Number of pixels to recover
 * @param out     Destination, one byte per pixel (>= count bytes)
 */
inline void njnUnpackNibbles(const uint8_t* packed, size_t count, uint8_t* out) {
    if (!packed || !out) return;
    for (size_t i = 0; i < count; ++i) {
        uint8_t byte = packed[i / 2];
        out[i] = (i & 1) ? static_cast<uint8_t>(byte >> 4)
                         : static_cast<uint8_t>(byte & 0x0F);
    }
}

/**
 * @brief Encode a complete .njn v2 file (8-byte header + packed pixels).
 *
 * @param cellW,cellH,cols,rows  Header geometry (a static bitmap is cols=rows=1)
 * @param indices     Source indices, one per byte, length cellW*cellH*cols*rows
 * @param pixelCount  Length of @p indices (must equal the geometry's plane size)
 * @param out         Destination buffer
 * @param outCap      Capacity of @p out
 * @return Total bytes written, or 0 on a size mismatch / insufficient capacity
 */
inline size_t njnEncodeV2(uint8_t cellW, uint8_t cellH, uint8_t cols, uint8_t rows,
                          const uint8_t* indices, size_t pixelCount,
                          uint8_t* out, size_t outCap) {
    if (!indices || !out) return 0;
    if (cellW == 0 || cellH == 0 || cols == 0 || rows == 0) return 0;
    uint32_t plane = static_cast<uint32_t>(cellW) * cellH * cols * rows;
    if (pixelCount != plane) return 0;
    size_t total = sizeof(NjnHeader) + njnPackedByteSize(plane);
    if (outCap < total) return 0;

    out[0] = NJN_MAGIC_0;
    out[1] = NJN_MAGIC_1;
    out[2] = NJN_VERSION_V2;
    out[3] = cellW;
    out[4] = cellH;
    out[5] = cols;
    out[6] = rows;
    out[7] = 0;
    njnPackNibbles(indices, pixelCount, out + sizeof(NjnHeader));
    return total;
}

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

    // Validate version (v1 byte-per-pixel, v2 4-bit packed)
    if (out.version != NJN_VERSION && out.version != NJN_VERSION_V2) return false;

    // Validate dimensions (nonzero)
    if (out.cellW == 0 || out.cellH == 0 || out.cols == 0 || out.rows == 0) return false;

    // Validate that the buffer holds enough pixel data — v1 is one byte per
    // pixel, v2 is nibble-packed (two pixels per byte).
    uint32_t pixelCount = njnPixelCount(out);
    uint32_t dataBytes = (out.version == NJN_VERSION_V2) ? njnPackedByteSize(pixelCount)
                                                         : pixelCount;
    if (size < sizeof(NjnHeader) + dataBytes) return false;

    return true;
}

/**
 * @brief Compute total pixel data size from header fields
 * @param h Parsed NjnHeader
 * @return Total pixel byte count (cellW * cellH * cols * rows)
 */
inline uint32_t njnPixelDataSize(const NjnHeader& h) {
    return static_cast<uint32_t>(h.cellW) * h.cellH * h.cols * h.rows;
}

} // namespace enjin2
