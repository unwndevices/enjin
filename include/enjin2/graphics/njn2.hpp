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
 * Chunk IDs not in the table above are *unknown* and must be skipped silently.
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
struct NjnTileAttr {
    uint8_t flags; ///< bit0=SOLID, bit1=ONEWAY, bits2-3=DIR, bits4-7 reserved
    uint8_t kind;  ///< 0–255 material/trigger kind
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

} // namespace enjin2
