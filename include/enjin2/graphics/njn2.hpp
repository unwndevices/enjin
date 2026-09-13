/**
 * @file njn2.hpp
 * @brief .njn v2 typed-chunk container — reader and writer (issue #76)
 *
 * The .njn v2 format extends the v1 8-byte flat header into a RIFF/glTF-style
 * typed-chunk container.  A directory at the front of the file lets readers
 * skip unknown chunks without error, so new chunk types are addable without
 * breaking existing readers.
 *
 * ## Wire layout
 *
 * ```
 * [File header — 12 bytes]
 *   byte  0-1 : magic 'N','J'  (0x4E, 0x4A)
 *   byte  2   : version = 2
 *   byte  3   : reserved (0)
 *   u32   4-7 : numChunks — number of directory entries (little-endian)
 *   u32   8-11: fileSize  — total byte length of the file (little-endian)
 *
 * [Directory — numChunks × 12 bytes each]
 *   byte  0-3 : chunkId (4-byte ASCII tag, e.g. 'META', 'PIXL')
 *   u32   4-7 : offset  — byte offset from the start of the file to chunk data
 *   u32   8-11: size    — byte length of the chunk data
 *
 * [Chunk data blocks — packed immediately after the directory]
 *   Each block contains raw chunk bytes; no padding between blocks (sizes are
 *   exact).  Readers locate chunks via the directory, so insertion order is
 *   irrelevant and extra chunks are safely skipped.
 * ```
 *
 * ## Defined chunk IDs
 *
 * | ID     | Content                                                        |
 * |--------|----------------------------------------------------------------|
 * | `META` | Sheet geometry (cellW, cellH, cols, rows) — 4 bytes            |
 * | `PIXL` | Raw pixel data (cellW × cellH × cols × rows bytes, 4bpp idx)  |
 * | `CLIP` | Animation clip table (variable length, see NjnClip)            |
 * | `ATTR` | Per-tile attribute table (numTiles × 2 bytes, see TileAttr)    |
 * | `PALB` | Palette-bank table (numBanks × 16 × 2 bytes RGB565)            |
 *
 * The flat-sheet chunks (`META`/`PIXL`/`ATTR`/`PALB`) and `CLIP` keep their
 * layouts above.  A *layered* sprite asset adds the chunks below and carries
 * no flattened `META`/`PIXL` fallback payload; its `CLIP` frame indices
 * reference **animation frames** (0..numFrames-1), not sheet cells.
 *
 * | ID     | Content                                                        |
 * |--------|----------------------------------------------------------------|
 * | `LHDR` | Layered header: schema version, canvas extent, counts          |
 * | `LIMG` | Immutable pool of cropped part images (4bpp, 1 byte/px)        |
 * | `LPRT` | Named sprite parts, bottom-to-top painter order                |
 * | `LREF` | One frame-part reference per part per frame                    |
 * | `LDUR` | Authored per-frame durations (ms)                              |
 *
 * Chunk IDs not in the tables above are *unknown* and must be skipped silently.
 *
 * ## Endianness
 *
 * All multi-byte fields are little-endian.  The file header, directory, and
 * all chunk-internal integers follow this rule.
 *
 * ## Usage (writer)
 *
 * ```cpp
 * std::vector<uint8_t> buf;
 * NjnV2Writer w;
 * // --- META chunk ---
 * w.beginChunk(NJN2_CHUNK_META);
 * w.writeU8(cellW); w.writeU8(cellH); w.writeU8(cols); w.writeU8(rows);
 * w.endChunk();
 * // --- PIXL chunk ---
 * w.beginChunk(NJN2_CHUNK_PIXL);
 * w.writeBytes(pixelData, pixelDataSize);
 * w.endChunk();
 * w.finalise(buf);   // serialises header + directory + all chunk data
 * ```
 *
 * ## Usage (reader)
 *
 * ```cpp
 * NjnV2Reader r;
 * if (!r.open(buf.data(), buf.size())) { return; } // bad magic/version
 * if (auto* c = r.find(NJN2_CHUNK_META)) {
 *     uint8_t cellW = c->data[0];
 *     // ...
 * }
 * // Unknown chunks: r.find() returns nullptr — reader simply moves on.
 * ```
 */
#pragma once

#include <cstdint>
#include <cstddef>
#include <cstring>
#include <vector>
#include <array>

#include "tilemap_asset.hpp"

namespace enjin2 {

// ---------------------------------------------------------------------------
// Constants
// ---------------------------------------------------------------------------

static constexpr uint8_t NJN2_MAGIC_0 = 'N';
static constexpr uint8_t NJN2_MAGIC_1 = 'J';
static constexpr uint8_t NJN2_VERSION = 2;

/// Byte length of the file-level header.
static constexpr uint32_t NJN2_FILE_HEADER_SIZE = 12;

/// Byte length of one directory entry.
static constexpr uint32_t NJN2_DIR_ENTRY_SIZE = 12;

// ---------------------------------------------------------------------------
// Chunk IDs — 4-byte ASCII tags
// ---------------------------------------------------------------------------

/// A chunk id is four ASCII bytes packed into a uint32 (no null terminator).
using NjnChunkTag = std::array<uint8_t, 4>;

/// Helper: make a NjnChunkTag from a string literal (no null included).
constexpr NjnChunkTag njnTag(char a, char b, char c, char d) {
    return {{ static_cast<uint8_t>(a), static_cast<uint8_t>(b),
              static_cast<uint8_t>(c), static_cast<uint8_t>(d) }};
}

/// Sheet geometry chunk: cellW(u8) cellH(u8) cols(u8) rows(u8) — 4 bytes.
static constexpr NjnChunkTag NJN2_CHUNK_META = {{ 'M','E','T','A' }};
/// Raw pixel data chunk: cellW × cellH × cols × rows bytes, 4bpp palette index.
static constexpr NjnChunkTag NJN2_CHUNK_PIXL = {{ 'P','I','X','L' }};
/// Animation clips chunk: variable (see NjnLoopMode / NjnFrameEntry below).
static constexpr NjnChunkTag NJN2_CHUNK_CLIP = {{ 'C','L','I','P' }};
/// Per-tile attributes chunk: numTiles × 2 bytes (flags byte + kind byte).
static constexpr NjnChunkTag NJN2_CHUNK_ATTR = {{ 'A','T','T','R' }};
/// Palette-bank table chunk: numBanks × 16 entries × 2 bytes RGB565 each.
static constexpr NjnChunkTag NJN2_CHUNK_PALB = {{ 'P','A','L','B' }};

// --- Layered sprite chunks (schema version 1, issue #93) ------------------

/// Layered header chunk: schema version + authored canvas extent + counts.
static constexpr NjnChunkTag NJN2_CHUNK_LHDR = {{ 'L','H','D','R' }};
/// Layered part-image pool chunk: immutable cropped part images.
static constexpr NjnChunkTag NJN2_CHUNK_LIMG = {{ 'L','I','M','G' }};
/// Layered sprite-parts chunk: named parts in bottom-to-top painter order.
static constexpr NjnChunkTag NJN2_CHUNK_LPRT = {{ 'L','P','R','T' }};
/// Layered frame-part references chunk: one reference per part per frame.
static constexpr NjnChunkTag NJN2_CHUNK_LREF = {{ 'L','R','E','F' }};
/// Layered per-frame durations chunk: authored hold time per animation frame.
static constexpr NjnChunkTag NJN2_CHUNK_LDUR = {{ 'L','D','U','R' }};

/// The only layered schema version the codec understands.  A reader that sees
/// a different value rejects the asset as an unsupported layered schema.
static constexpr uint8_t NJN2_LAYERED_SCHEMA_VERSION = 1;

/// Sentinel `imageIndex` meaning "this part is invisible this frame" (no image,
/// no offset).  A valid image index is always in 0..numImages-1, so 0xFFFF is
/// unambiguous given numImages < 0xFFFF.
static constexpr uint16_t NJN2_LAYERED_INVISIBLE = 0xFFFF;

// ---------------------------------------------------------------------------
// CLIP chunk internal layout
//
// CLIP chunk byte layout:
//   u8  numClips
//   for each clip:
//     u8[16]  name (null-padded)
//     u8      loopMode  (0=Once, 1=Loop, 2=PingPong)
//     u8      numFrames
//     for each frame:
//       u16  frameIndex   (sheet cell index, little-endian)
//       u16  durationMs   (little-endian)
//       u8   eventId      (0 = none)
//
// ATTR chunk byte layout:
//   u16 numTiles (little-endian)
//   for each tile (numTiles entries):
//     u8 flags    bit0=SOLID bit1=ONEWAY bits2-3=DIR:2 bits4-7=reserved
//     u8 kind     full uint8, 256 kinds
//
// PALB chunk byte layout:
//   u8 numBanks
//   for each bank (numBanks entries):
//     16 × u16 (RGB565, little-endian)  — one full 16-entry palette remap
// ---------------------------------------------------------------------------

/// Loop mode for a CLIP animation clip.
enum class NjnLoopMode : uint8_t {
    Once     = 0,
    Loop     = 1,
    PingPong = 2,
};

/// One frame entry inside a CLIP chunk.
struct NjnFrameEntry {
    uint16_t frameIndex;   ///< Sheet cell index.
    uint16_t durationMs;   ///< Frame hold time in milliseconds.
    uint8_t  eventId;      ///< Per-frame event id (0 = none).
};

/// One decoded animation clip (name, loopMode, frames).
struct NjnClip {
    char              name[16];    ///< Null-padded clip name (max 15 chars + '\0').
    NjnLoopMode       loopMode;    ///< Once / Loop / PingPong.
    std::vector<NjnFrameEntry> frames; ///< Per-frame entries.
};

/// Per-tile attribute record (matches §3 TileAttr layout from ADR-0003).
using NjnTileAttr = TileAttr;

// ---------------------------------------------------------------------------
// Layered sprite records (schema version 1)
//
// LHDR chunk byte layout:
//   u8   schemaVersion   (must equal NJN2_LAYERED_SCHEMA_VERSION)
//   u16  canvasW         (authored canvas width, px)
//   u16  canvasH         (authored canvas height, px)
//   u16  numFrames       (animation frame count)
//   u16  numParts        (sprite part count)
//   u16  numImages       (part-image pool size)
//
// LIMG chunk byte layout (numImages records, in image order):
//   for each image:
//     u16  w             (≥1)
//     u16  h             (≥1)
//     w*h bytes          4bpp palette index in low nibble, row-major
//
// LPRT chunk byte layout (numParts records, in bottom-to-top painter order):
//   for each part:
//     u8[16]  name       (null-padded, ≤15 printable chars)
//
// LREF chunk byte layout (numFrames × numParts records, frame-major):
//   for each frame, for each part:
//     u16  imageIndex    (0..numImages-1, or 0xFFFF = invisible)
//     s16  offsetX       (authored offset from canvas origin, px)
//     s16  offsetY
//
// LDUR chunk byte layout (numFrames records):
//   for each frame:
//     u16  durationMs
//
// CLIP chunk: unchanged layout; frame indices reference animation frames.
//
// Part-image identity is (w, h, canonical palette indices) only.  Offsets and
// invisibility live on frame-part references, so linked pixels may move.
// ---------------------------------------------------------------------------

/// One cropped, immutable part image: dimensions + canonical 4bpp pixels.
struct NjnPartImage {
    uint16_t w;                       ///< Cropped width in pixels (≥1).
    uint16_t h;                       ///< Cropped height in pixels (≥1).
    std::vector<uint8_t> pixels;      ///< w*h bytes, low nibble = palette index.
    /// Non-owning view of the same w*h pixels inside the source container.
    /// `njn2DecodeLayered()` fills this instead of `pixels` when decoding for a
    /// direct copy into a destination (e.g. the PSRAM asset arena), so a large
    /// asset never needs a second heap-resident pixel copy. Null when pixels
    /// were materialized (the default) or the image was not decoded.
    const uint8_t* rawPixels = nullptr;
};

/// One named sprite part.  The name is diagnostics metadata, never image identity.
struct NjnPart {
    char name[16];                    ///< Null-padded part name (≤15 chars + '\0').
};

/// One frame-part reference: an image at an authored offset, or invisible.
struct NjnFramePartRef {
    uint16_t imageIndex;              ///< 0..numImages-1, or NJN2_LAYERED_INVISIBLE.
    int16_t  offsetX;                 ///< Offset from the canvas origin (px).
    int16_t  offsetY;
};

/// A decoded layered sprite asset (the union of the layered chunks).
struct NjnLayered {
    uint8_t  schemaVersion = NJN2_LAYERED_SCHEMA_VERSION;
    uint16_t canvasW = 0;             ///< Authored canvas extent (stable every frame).
    uint16_t canvasH = 0;
    std::vector<NjnPartImage> images; ///< Immutable image pool.
    std::vector<NjnPart>      parts;  ///< Bottom-to-top painter order.
    std::vector<NjnFramePartRef> refs;///< Frame-major: numFrames × numParts.
    std::vector<uint16_t>     durations; ///< One per animation frame (ms).
    std::vector<NjnClip>      clips;  ///< Optional; frameIndex → animation frame.
};

// ---------------------------------------------------------------------------
// Reader
// ---------------------------------------------------------------------------

/// A resolved directory entry pointing into the raw file buffer.
struct NjnV2Chunk {
    NjnChunkTag     id;     ///< 4-byte tag.
    const uint8_t*  data;   ///< Pointer into the file buffer (not owned).
    uint32_t        size;   ///< Byte length of the chunk data.
};

/**
 * @brief Read-only view over a .njn v2 byte buffer.
 *
 * Call open() with a flat byte buffer; it validates magic/version and builds
 * an in-memory directory of chunk descriptors.  find() locates a chunk by tag;
 * unknown tags simply return nullptr so callers can skip them.
 */
class NjnV2Reader {
public:
    NjnV2Reader() = default;

    /**
     * @brief Parse and validate a .njn v2 file from a raw byte buffer.
     * @param data  Pointer to the start of the file bytes.
     * @param size  Total byte count of the buffer.
     * @return true on success; false if the data is malformed.
     */
    bool open(const uint8_t* data, size_t size) {
        m_chunks.clear();

        // --- File header validation ---
        if (!data || size < NJN2_FILE_HEADER_SIZE) return false;

        if (data[0] != NJN2_MAGIC_0 || data[1] != NJN2_MAGIC_1) return false;
        if (data[2] != NJN2_VERSION) return false;
        // byte 3: reserved (ignored)

        const uint32_t numChunks = readU32LE(data + 4);
        const uint32_t fileSize  = readU32LE(data + 8);

        if (fileSize != static_cast<uint32_t>(size)) return false;

        // --- Directory ---
        // Guard against uint32 wrap in numChunks * NJN2_DIR_ENTRY_SIZE.
        if (numChunks > (UINT32_MAX / NJN2_DIR_ENTRY_SIZE)) return false;
        const uint32_t dirBytes = numChunks * NJN2_DIR_ENTRY_SIZE;
        if (size < NJN2_FILE_HEADER_SIZE + dirBytes) return false;

        for (uint32_t i = 0; i < numChunks; ++i) {
            const uint8_t* entry = data + NJN2_FILE_HEADER_SIZE + i * NJN2_DIR_ENTRY_SIZE;
            NjnV2Chunk c;
            c.id   = {{ entry[0], entry[1], entry[2], entry[3] }};
            c.data = nullptr;
            const uint32_t offset = readU32LE(entry + 4);
            const uint32_t csz    = readU32LE(entry + 8);
            c.size = csz;

            // Validate chunk bounds
            if (offset < NJN2_FILE_HEADER_SIZE) return false;
            if (static_cast<uint64_t>(offset) + csz > fileSize) return false;

            c.data = data + offset;
            m_chunks.push_back(c);
        }

        return true;
    }

    /**
     * @brief Find a chunk by its 4-byte tag.
     * @param tag  The chunk ID to search for.
     * @return Pointer to the NjnV2Chunk descriptor, or nullptr if not present.
     *
     * Unknown tags return nullptr — callers must handle absence gracefully.
     */
    const NjnV2Chunk* find(const NjnChunkTag& tag) const {
        for (const auto& c : m_chunks) {
            if (c.id == tag) return &c;
        }
        return nullptr;
    }

    /// Number of chunks in the directory.
    uint32_t chunkCount() const { return static_cast<uint32_t>(m_chunks.size()); }

    /**
     * @brief Count the chunks with a given 4-byte tag (duplicate detection).
     * @param tag  The chunk ID to count.
     * @return The number of directory entries with that tag.
     */
    uint32_t count(const NjnChunkTag& tag) const {
        uint32_t n = 0;
        for (const auto& c : m_chunks) {
            if (c.id == tag) ++n;
        }
        return n;
    }

private:
    std::vector<NjnV2Chunk> m_chunks;

    static uint32_t readU32LE(const uint8_t* p) {
        return static_cast<uint32_t>(p[0])
             | (static_cast<uint32_t>(p[1]) << 8)
             | (static_cast<uint32_t>(p[2]) << 16)
             | (static_cast<uint32_t>(p[3]) << 24);
    }
};

// ---------------------------------------------------------------------------
// Writer
// ---------------------------------------------------------------------------

/**
 * @brief Accumulative .njn v2 file builder.
 *
 * Build a file by calling beginChunk() / writeU8() / writeU16LE() /
 * writeBytes() / endChunk() for each chunk, then call finalise() to
 * serialise the complete file (header + directory + data) into a buffer.
 *
 * Chunks may be written in any order.  The directory is emitted by finalise().
 *
 * Example:
 * ```cpp
 * NjnV2Writer w;
 * w.beginChunk(NJN2_CHUNK_META);
 * w.writeU8(cellW); w.writeU8(cellH); w.writeU8(cols); w.writeU8(rows);
 * w.endChunk();
 * std::vector<uint8_t> out;
 * w.finalise(out);
 * ```
 */
class NjnV2Writer {
public:
    NjnV2Writer() = default;

    /// Start a new chunk.  Must be paired with endChunk().
    void beginChunk(const NjnChunkTag& tag) {
        m_pending   = tag;
        m_chunkData.clear();
        m_inChunk   = true;
    }

    /// Finish the current chunk and commit it to the pending list.
    void endChunk() {
        if (!m_inChunk) return;
        PendingChunk pc;
        pc.id   = m_pending;
        pc.data = m_chunkData;
        m_pending_chunks.push_back(std::move(pc));
        m_chunkData.clear();
        m_inChunk = false;
    }

    /// Append a single byte to the current chunk.
    void writeU8(uint8_t v) { m_chunkData.push_back(v); }

    /// Append a 16-bit value (little-endian) to the current chunk.
    void writeU16LE(uint16_t v) {
        m_chunkData.push_back(static_cast<uint8_t>(v & 0xFF));
        m_chunkData.push_back(static_cast<uint8_t>((v >> 8) & 0xFF));
    }

    /// Append a 32-bit value (little-endian) to the current chunk.
    void writeU32LE(uint32_t v) {
        m_chunkData.push_back(static_cast<uint8_t>(v & 0xFF));
        m_chunkData.push_back(static_cast<uint8_t>((v >> 8) & 0xFF));
        m_chunkData.push_back(static_cast<uint8_t>((v >> 16) & 0xFF));
        m_chunkData.push_back(static_cast<uint8_t>((v >> 24) & 0xFF));
    }

    /// Append arbitrary bytes to the current chunk.
    void writeBytes(const uint8_t* src, size_t len) {
        m_chunkData.insert(m_chunkData.end(), src, src + len);
    }

    /**
     * @brief Serialise the complete .njn v2 file into @p out.
     *
     * Layout produced:
     *   [12-byte file header]
     *   [numChunks × 12-byte directory]
     *   [chunk data blocks, packed]
     *
     * @param out  Destination buffer; contents are overwritten.
     */
    void finalise(std::vector<uint8_t>& out) const {
        const uint32_t numChunks = static_cast<uint32_t>(m_pending_chunks.size());
        const uint32_t dirOffset = NJN2_FILE_HEADER_SIZE;
        const uint32_t dirBytes  = numChunks * NJN2_DIR_ENTRY_SIZE;
        uint32_t dataOffset      = dirOffset + dirBytes;

        // Compute total file size
        uint32_t fileSize = dataOffset;
        for (const auto& pc : m_pending_chunks) {
            fileSize += static_cast<uint32_t>(pc.data.size());
        }

        out.resize(fileSize);
        uint8_t* p = out.data();

        // --- File header ---
        p[0] = NJN2_MAGIC_0;
        p[1] = NJN2_MAGIC_1;
        p[2] = NJN2_VERSION;
        p[3] = 0; // reserved
        writeU32LE(p + 4, numChunks);
        writeU32LE(p + 8, fileSize);
        p += NJN2_FILE_HEADER_SIZE;

        // --- Directory ---
        uint32_t curOffset = dataOffset;
        for (const auto& pc : m_pending_chunks) {
            p[0] = pc.id[0];
            p[1] = pc.id[1];
            p[2] = pc.id[2];
            p[3] = pc.id[3];
            writeU32LE(p + 4, curOffset);
            writeU32LE(p + 8, static_cast<uint32_t>(pc.data.size()));
            p += NJN2_DIR_ENTRY_SIZE;
            curOffset += static_cast<uint32_t>(pc.data.size());
        }

        // --- Chunk data ---
        for (const auto& pc : m_pending_chunks) {
            if (!pc.data.empty()) {
                std::memcpy(p, pc.data.data(), pc.data.size());
                p += pc.data.size();
            }
        }
    }

    /// Number of completed chunks accumulated so far.
    uint32_t chunkCount() const { return static_cast<uint32_t>(m_pending_chunks.size()); }

private:
    struct PendingChunk {
        NjnChunkTag           id;
        std::vector<uint8_t>  data;
    };

    NjnChunkTag           m_pending  = {};
    bool                  m_inChunk  = false;
    std::vector<uint8_t>  m_chunkData;
    std::vector<PendingChunk> m_pending_chunks;

    static void writeU32LE(uint8_t* p, uint32_t v) {
        p[0] = static_cast<uint8_t>(v & 0xFF);
        p[1] = static_cast<uint8_t>((v >> 8) & 0xFF);
        p[2] = static_cast<uint8_t>((v >> 16) & 0xFF);
        p[3] = static_cast<uint8_t>((v >> 24) & 0xFF);
    }
};

// ---------------------------------------------------------------------------
// Convenience helpers for typed chunk content
// ---------------------------------------------------------------------------

/**
 * @brief Decode a META chunk into its four geometry fields.
 * @param c    A chunk with id == NJN2_CHUNK_META.
 * @param cellW  Output: cell width in pixels.
 * @param cellH  Output: cell height in pixels.
 * @param cols   Output: grid columns.
 * @param rows   Output: grid rows.
 * @return true if the chunk is present and has ≥4 bytes.
 */
inline bool njn2DecodeMeta(const NjnV2Chunk* c,
                           uint8_t& cellW, uint8_t& cellH,
                           uint8_t& cols,  uint8_t& rows) {
    if (!c || c->size < 4) return false;
    cellW = c->data[0];
    cellH = c->data[1];
    cols  = c->data[2];
    rows  = c->data[3];
    return true;
}

/**
 * @brief Write a META chunk (cellW, cellH, cols, rows) to a writer.
 */
inline void njn2WriteMeta(NjnV2Writer& w,
                          uint8_t cellW, uint8_t cellH,
                          uint8_t cols,  uint8_t rows) {
    w.beginChunk(NJN2_CHUNK_META);
    w.writeU8(cellW);
    w.writeU8(cellH);
    w.writeU8(cols);
    w.writeU8(rows);
    w.endChunk();
}

/**
 * @brief Write a PIXL chunk with raw pixel bytes.
 */
inline void njn2WritePixl(NjnV2Writer& w, const uint8_t* pixels, uint32_t size) {
    w.beginChunk(NJN2_CHUNK_PIXL);
    w.writeBytes(pixels, size);
    w.endChunk();
}

/**
 * @brief Write an ATTR chunk from a flat array of NjnTileAttr records.
 * @param w       Writer target.
 * @param attrs   Array of per-tile attribute records (tile 0, 1, …).
 * @param numTiles Number of entries in @p attrs.
 */
inline void njn2WriteAttr(NjnV2Writer& w,
                          const NjnTileAttr* attrs, uint16_t numTiles) {
    w.beginChunk(NJN2_CHUNK_ATTR);
    w.writeU16LE(numTiles);
    for (uint16_t i = 0; i < numTiles; ++i) {
        w.writeU8(attrs[i].flags);
        w.writeU8(attrs[i].kind);
    }
    w.endChunk();
}

/**
 * @brief Decode an ATTR chunk into a caller-owned vector of NjnTileAttr.
 * @param c       Chunk with id == NJN2_CHUNK_ATTR (may be nullptr).
 * @param out     Output vector; replaced on success.
 * @return true if the chunk is valid and was decoded.
 */
inline bool njn2DecodeAttr(const NjnV2Chunk* c, std::vector<NjnTileAttr>& out) {
    if (!c || c->size < 2) return false;
    const uint16_t numTiles = static_cast<uint16_t>(c->data[0])
                            | (static_cast<uint16_t>(c->data[1]) << 8);
    if (c->size < static_cast<uint32_t>(2 + numTiles * 2)) return false;
    out.resize(numTiles);
    for (uint16_t i = 0; i < numTiles; ++i) {
        out[i].flags = c->data[2 + i * 2];
        out[i].kind  = c->data[2 + i * 2 + 1];
    }
    return true;
}

/**
 * @brief Write a PALB chunk from a flat array of RGB565 palette banks.
 * @param w        Writer target.
 * @param banks    Pointer to numBanks × 16 uint16 RGB565 entries.
 * @param numBanks Number of banks (≤255).
 */
inline void njn2WritePalb(NjnV2Writer& w, const uint16_t* banks, uint8_t numBanks) {
    w.beginChunk(NJN2_CHUNK_PALB);
    w.writeU8(numBanks);
    for (uint32_t i = 0; i < static_cast<uint32_t>(numBanks) * 16; ++i) {
        w.writeU16LE(banks[i]);
    }
    w.endChunk();
}

/**
 * @brief Decode a PALB chunk.
 * @param c        Chunk with id == NJN2_CHUNK_PALB (may be nullptr).
 * @param out      Output vector of RGB565 values (numBanks × 16 entries).
 * @param numBanks Output: number of banks read.
 * @return true if valid and decoded.
 */
inline bool njn2DecodePalb(const NjnV2Chunk* c,
                            std::vector<uint16_t>& out,
                            uint8_t& numBanks) {
    if (!c || c->size < 1) return false;
    numBanks = c->data[0];
    const uint32_t expected = 1u + static_cast<uint32_t>(numBanks) * 16u * 2u;
    if (c->size < expected) return false;
    out.resize(static_cast<size_t>(numBanks) * 16);
    for (size_t i = 0; i < out.size(); ++i) {
        out[i] = static_cast<uint16_t>(c->data[1 + i * 2])
               | (static_cast<uint16_t>(c->data[1 + i * 2 + 1]) << 8);
    }
    return true;
}

/**
 * @brief Write a CLIP chunk containing one or more named animation clips.
 * @param w     Writer target.
 * @param clips Pointer to an array of NjnClip records.
 * @param count Number of clips.
 *
 * Wire layout written:
 *   u8        numClips
 *   for each clip:
 *     u8[16]  name (null-padded, up to 15 printable chars)
 *     u8      loopMode
 *     u8      numFrames
 *     for each frame:
 *       u16   frameIndex  (LE)
 *       u16   durationMs  (LE)
 *       u8    eventId
 */
inline void njn2WriteClip(NjnV2Writer& w,
                           const NjnClip* clips, uint8_t count) {
    w.beginChunk(NJN2_CHUNK_CLIP);
    w.writeU8(count);
    for (uint8_t ci = 0; ci < count; ++ci) {
        const NjnClip& c = clips[ci];
        // 16-byte null-padded name
        for (int k = 0; k < 16; ++k) {
            w.writeU8(k < 15 ? static_cast<uint8_t>(c.name[k]) : 0u);
        }
        w.writeU8(static_cast<uint8_t>(c.loopMode));
        w.writeU8(static_cast<uint8_t>(c.frames.size()));
        for (const auto& f : c.frames) {
            w.writeU16LE(f.frameIndex);
            w.writeU16LE(f.durationMs);
            w.writeU8(f.eventId);
        }
    }
    w.endChunk();
}

/**
 * @brief Decode a CLIP chunk into a caller-owned vector of NjnClip.
 * @param c   Chunk with id == NJN2_CHUNK_CLIP (may be nullptr).
 * @param out Output vector; replaced on success.
 * @return true if the chunk is present and well-formed.
 */
inline bool njn2DecodeClip(const NjnV2Chunk* c, std::vector<NjnClip>& out) {
    if (!c || c->size < 1) return false;
    const uint8_t numClips = c->data[0];
    uint32_t pos = 1;
    out.clear();
    for (uint8_t ci = 0; ci < numClips; ++ci) {
        // name(16) + loopMode(1) + numFrames(1) = 18 bytes minimum per clip
        if (pos + 18u > c->size) return false;
        NjnClip clip;
        for (int k = 0; k < 16; ++k) clip.name[k] = static_cast<char>(c->data[pos + k]);
        clip.loopMode = static_cast<NjnLoopMode>(c->data[pos + 16]);
        const uint8_t numFrames = c->data[pos + 17];
        pos += 18;
        // each frame = u16 + u16 + u8 = 5 bytes
        if (pos + static_cast<uint32_t>(numFrames) * 5u > c->size) return false;
        clip.frames.resize(numFrames);
        for (uint8_t fi = 0; fi < numFrames; ++fi) {
            clip.frames[fi].frameIndex = static_cast<uint16_t>(c->data[pos])
                                       | (static_cast<uint16_t>(c->data[pos + 1]) << 8);
            clip.frames[fi].durationMs = static_cast<uint16_t>(c->data[pos + 2])
                                       | (static_cast<uint16_t>(c->data[pos + 3]) << 8);
            clip.frames[fi].eventId    = c->data[pos + 4];
            pos += 5;
        }
        out.push_back(std::move(clip));
    }
    return true;
}

// ---------------------------------------------------------------------------
// Layered sprite codec (schema version 1, issue #93)
// ---------------------------------------------------------------------------

/**
 * @brief Write a complete layered sprite asset (LHDR + LIMG + LPRT + LREF +
 *        LDUR + optional CLIP) to a writer.
 * @param w  Writer target.
 * @param asset  The layered asset to serialise.
 *
 * Counts are derived from the vectors: numFrames = durations.size(),
 * numParts = parts.size(), numImages = images.size().  The caller must keep
 * refs.size() == numFrames * numParts, images[i].pixels.size() == w*h, and
 * every count ≤ 0xFFFF (the Python mirror raises on these; the C++ writer
 * trusts the caller like the other njn2Write* helpers).  The reader is the
 * authority: njn2DecodeLayered rejects anything this writer could over-truncate.
 *
 * The CLIP chunk is reused unchanged; its frame indices reference animation
 * frames (0..numFrames-1), not sheet cells.  No META/PIXL fallback is written.
 */
inline void njn2WriteLayered(NjnV2Writer& w, const NjnLayered& asset) {
    const uint16_t numFrames = static_cast<uint16_t>(asset.durations.size());
    const uint16_t numParts  = static_cast<uint16_t>(asset.parts.size());
    const uint16_t numImages = static_cast<uint16_t>(asset.images.size());

    w.beginChunk(NJN2_CHUNK_LHDR);
    w.writeU8(asset.schemaVersion);
    w.writeU16LE(asset.canvasW);
    w.writeU16LE(asset.canvasH);
    w.writeU16LE(numFrames);
    w.writeU16LE(numParts);
    w.writeU16LE(numImages);
    w.endChunk();

    w.beginChunk(NJN2_CHUNK_LIMG);
    for (const auto& img : asset.images) {
        w.writeU16LE(img.w);
        w.writeU16LE(img.h);
        w.writeBytes(img.pixels.data(), img.pixels.size());
    }
    w.endChunk();

    w.beginChunk(NJN2_CHUNK_LPRT);
    for (const auto& part : asset.parts) {
        for (int k = 0; k < 16; ++k) {
            w.writeU8(k < 15 ? static_cast<uint8_t>(part.name[k]) : 0u);
        }
    }
    w.endChunk();

    w.beginChunk(NJN2_CHUNK_LREF);
    for (const auto& ref : asset.refs) {
        w.writeU16LE(ref.imageIndex);
        w.writeU16LE(static_cast<uint16_t>(ref.offsetX));
        w.writeU16LE(static_cast<uint16_t>(ref.offsetY));
    }
    w.endChunk();

    w.beginChunk(NJN2_CHUNK_LDUR);
    for (uint16_t d : asset.durations) {
        w.writeU16LE(d);
    }
    w.endChunk();

    if (!asset.clips.empty()) {
        njn2WriteClip(w, asset.clips.data(), static_cast<uint8_t>(asset.clips.size()));
    }
}

/**
 * @brief Decode a layered sprite asset from a .njn v2 reader, validating the
 *        schema version, required-chunk presence, exact record sizes, and
 *        reference bounds.
 * @param r       A reader that already `open()`ed the file successfully.
 * @param out     Decoded asset; only valid when the function returns true.
 * @param errMsg  Optional out-param receiving a static failure reason string.
 * @param materializePixels
 *                When true (default) each image owns a heap copy of its pixels.
 *                When false, `NjnPartImage::rawPixels` points into @p r's buffer
 *                instead, so a loader can copy straight into its destination
 *                without a second full pixel copy (device PSRAM path, #100).
 *                The reader buffer must outlive @p out in that case.
 * @return true on success; false on any malformed/unsupported input.
 *
 * Rejects: missing or duplicate required chunks, an unsupported layered schema
 * version, zero/overflowing counts, truncated records, images with zero
 * dimensions, out-of-range part-image references, and CLIP frame indices that
 * fall outside 0..numFrames-1.  Unknown chunks are ignored.
 */
inline bool njn2DecodeLayered(const NjnV2Reader& r, NjnLayered& out,
                              const char** errMsg = nullptr,
                              bool materializePixels = true) {
    auto fail = [&](const char* m) -> bool {
        if (errMsg) *errMsg = m;
        return false;
    };
    auto rdU16 = [](const uint8_t* p) -> uint16_t {
        return static_cast<uint16_t>(p[0]) | (static_cast<uint16_t>(p[1]) << 8);
    };
    auto rdS16 = [&](const uint8_t* p) -> int16_t {
        return static_cast<int16_t>(rdU16(p));
    };

    // --- Required chunks: present exactly once each ---
    const NjnChunkTag required[] = {
        NJN2_CHUNK_LHDR, NJN2_CHUNK_LIMG, NJN2_CHUNK_LPRT,
        NJN2_CHUNK_LREF, NJN2_CHUNK_LDUR,
    };
    const NjnV2Chunk* req[5] = {nullptr, nullptr, nullptr, nullptr, nullptr};
    for (size_t i = 0; i < 5; ++i) {
        const uint32_t n = r.count(required[i]);
        if (n == 0) return fail("missing required layered chunk");
        if (n > 1)  return fail("duplicate required layered chunk");
        req[i] = r.find(required[i]);
    }

    // --- LHDR ---
    const NjnV2Chunk* hdr = req[0];
    if (hdr->size < 11) return fail("truncated LHDR");
    const uint8_t schema = hdr->data[0];
    if (schema != NJN2_LAYERED_SCHEMA_VERSION) return fail("unsupported layered schema version");
    const uint16_t canvasW   = rdU16(hdr->data + 1);
    const uint16_t canvasH   = rdU16(hdr->data + 3);
    const uint16_t numFrames = rdU16(hdr->data + 5);
    const uint16_t numParts  = rdU16(hdr->data + 7);
    const uint16_t numImages = rdU16(hdr->data + 9);
    if (canvasW == 0 || canvasH == 0) return fail("zero canvas extent");
    if (numFrames == 0 || numParts == 0) return fail("zero frame/part count");
    if (numImages == 0) return fail("empty part-image pool");

    // --- LIMG: walk numImages records, exact size, non-zero dimensions ---
    const NjnV2Chunk* imgChunk = req[1];
    out.images.clear();
    {
        uint32_t pos = 0;
        for (uint16_t i = 0; i < numImages; ++i) {
            if (static_cast<uint64_t>(pos) + 4u > imgChunk->size) return fail("truncated LIMG image header");
            const uint16_t w = rdU16(imgChunk->data + pos);
            const uint16_t h = rdU16(imgChunk->data + pos + 2);
            pos += 4;
            if (w == 0 || h == 0) return fail("zero-size part image");
            const uint64_t pixelBytes = static_cast<uint64_t>(w) * h;
            if (static_cast<uint64_t>(pos) + pixelBytes > imgChunk->size) return fail("truncated LIMG pixels");
            NjnPartImage img;
            img.w = w;
            img.h = h;
            if (materializePixels) {
                img.pixels.assign(imgChunk->data + pos, imgChunk->data + pos + pixelBytes);
            } else {
                img.rawPixels = imgChunk->data + pos;
            }
            pos += static_cast<uint32_t>(pixelBytes);
            out.images.push_back(std::move(img));
        }
        if (pos != imgChunk->size) return fail("LIMG trailing bytes");
    }

    // --- LPRT: exact size ---
    const NjnV2Chunk* partChunk = req[2];
    if (partChunk->size != static_cast<uint64_t>(numParts) * 16u) return fail("LPRT size mismatch");
    out.parts.clear();
    out.parts.resize(numParts);
    for (uint16_t i = 0; i < numParts; ++i) {
        for (int k = 0; k < 16; ++k) {
            out.parts[i].name[k] = static_cast<char>(partChunk->data[i * 16u + k]);
        }
    }

    // --- LREF: exact size, reference bounds ---
    const NjnV2Chunk* refChunk = req[3];
    const uint64_t refBytes = static_cast<uint64_t>(numFrames) * numParts * 6u;
    if (refChunk->size != refBytes) return fail("LREF size mismatch");
    out.refs.clear();
    out.refs.resize(static_cast<size_t>(numFrames) * numParts);
    for (size_t i = 0; i < out.refs.size(); ++i) {
        const uint8_t* p = refChunk->data + i * 6u;
        const uint16_t idx = rdU16(p);
        if (idx != NJN2_LAYERED_INVISIBLE && idx >= numImages) return fail("invalid part-image reference");
        out.refs[i].imageIndex = idx;
        out.refs[i].offsetX    = rdS16(p + 2);
        out.refs[i].offsetY    = rdS16(p + 4);
    }

    // --- LDUR: exact size ---
    const NjnV2Chunk* durChunk = req[4];
    if (durChunk->size != static_cast<uint64_t>(numFrames) * 2u) return fail("LDUR size mismatch");
    out.durations.clear();
    out.durations.resize(numFrames);
    for (uint16_t i = 0; i < numFrames; ++i) {
        out.durations[i] = rdU16(durChunk->data + i * 2u);
    }

    // --- CLIP: optional, at most once; frame indices reference animation frames ---
    out.clips.clear();
    if (r.count(NJN2_CHUNK_CLIP) > 1) return fail("duplicate CLIP chunk");
    const NjnV2Chunk* clipChunk = r.find(NJN2_CHUNK_CLIP);
    if (clipChunk != nullptr) {
        if (!njn2DecodeClip(clipChunk, out.clips)) return fail("malformed CLIP chunk");
        for (const auto& clip : out.clips) {
            for (const auto& f : clip.frames) {
                if (f.frameIndex >= numFrames) return fail("CLIP frame index out of range");
            }
        }
    }

    out.schemaVersion = schema;
    out.canvasW = canvasW;
    out.canvasH = canvasH;
    return true;
}

} // namespace enjin2
