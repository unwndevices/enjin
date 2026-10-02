/**
 * @file aseprite_file.cpp
 * @brief Strict `.aseprite` reader for the sprite importer (internal, #291)
 *
 * Every read is bounds-checked against its chunk; nothing asserts.  A problem
 * becomes an Error with SpriteImportStatus::Malformed (or TooLarge for a cap).
 */
#include "aseprite_file.hpp"

#include <string>

namespace enjin2 {
namespace ase {

namespace {

constexpr uint16_t FILE_MAGIC  = 0xA5E0;
constexpr uint16_t FRAME_MAGIC = 0xF1FA;
constexpr size_t   HEADER_SIZE = 128;
constexpr size_t   FRAME_HEADER_SIZE = 16;
constexpr size_t   CHUNK_HEADER_SIZE = 6;

constexpr uint16_t CHUNK_OLD_PALETTE = 0x0004;
constexpr uint16_t CHUNK_LAYER       = 0x2004;
constexpr uint16_t CHUNK_CEL         = 0x2005;
constexpr uint16_t CHUNK_TAGS        = 0x2018;
constexpr uint16_t CHUNK_PALETTE     = 0x2019;

constexpr uint32_t HEADER_FLAG_LAYER_OPACITY = 0x1;
constexpr uint16_t LAYER_FLAG_VISIBLE = 0x1;
constexpr uint16_t PALETTE_ENTRY_HAS_NAME = 0x1;

/// Bounds-checked little-endian cursor over [p, end).
class Cursor {
public:
    Cursor(const uint8_t* p, size_t n) : m_p(p), m_end(p + n) {}
    size_t left() const { return static_cast<size_t>(m_end - m_p); }
    const uint8_t* pos() const { return m_p; }
    bool skip(size_t n) {
        if (left() < n) return false;
        m_p += n;
        return true;
    }
    bool u8(uint8_t& v) {
        if (left() < 1) return false;
        v = *m_p++;
        return true;
    }
    bool u16(uint16_t& v) {
        if (left() < 2) return false;
        v = static_cast<uint16_t>(m_p[0] | (m_p[1] << 8));
        m_p += 2;
        return true;
    }
    bool s16(int16_t& v) {
        uint16_t u = 0;
        if (!u16(u)) return false;
        v = static_cast<int16_t>(u);
        return true;
    }
    bool u32(uint32_t& v) {
        if (left() < 4) return false;
        v = static_cast<uint32_t>(m_p[0]) | (static_cast<uint32_t>(m_p[1]) << 8) |
            (static_cast<uint32_t>(m_p[2]) << 16) | (static_cast<uint32_t>(m_p[3]) << 24);
        m_p += 4;
        return true;
    }
    bool str(std::string& s) {
        uint16_t n = 0;
        if (!u16(n) || left() < n) return false;
        s.assign(reinterpret_cast<const char*>(m_p), n);
        m_p += n;
        return true;
    }

private:
    const uint8_t* m_p;
    const uint8_t* m_end;
};

std::string at(size_t frame) { return " in frame " + std::to_string(frame); }

Error parseLayer(Cursor c, bool opacityValid, Layer& out) {
    uint16_t flags = 0, defW = 0, defH = 0;
    uint8_t opacity = 0;
    if (!c.u16(flags) || !c.u16(out.type) || !c.u16(out.childLevel) || !c.u16(defW) ||
        !c.u16(defH) || !c.u16(out.blendMode) || !c.u8(opacity) || !c.skip(3) || !c.str(out.name)) {
        return malformed("truncated layer chunk");
    }
    out.visible = (flags & LAYER_FLAG_VISIBLE) != 0;
    out.opacity = opacityValid ? opacity : 255;
    return {};
}

Error parseCel(Cursor c, size_t frame, const SpriteImportLimits& limits, uint16_t& layer, Cel& out) {
    if (!c.u16(layer) || !c.s16(out.x) || !c.s16(out.y) || !c.u8(out.opacity) ||
        !c.u16(out.type) || !c.s16(out.zIndex) || !c.skip(5)) {
        return malformed("truncated cel header" + at(frame));
    }
    out.present = true;
    switch (out.type) {
    case CEL_LINKED:
        if (!c.u16(out.linkedFrame)) return malformed("truncated linked cel" + at(frame));
        return {};
    case CEL_RAW:
    case CEL_COMPRESSED:
        if (!c.u16(out.w) || !c.u16(out.h)) return malformed("truncated cel size" + at(frame));
        if (out.w > limits.maxCelSide || out.h > limits.maxCelSide) {
            return tooLarge("cel " + std::to_string(out.w) + "x" + std::to_string(out.h) + at(frame) +
                            " exceeds the " + std::to_string(limits.maxCelSide) + " px cap");
        }
        out.payload = c.pos();
        out.payloadSize = c.left();
        return {};
    default:
        // Tilemap (3) and unknown cel types are kept undecoded; they fail only
        // if a visible layer uses them.
        return {};
    }
}

Error parseTags(Cursor c, std::vector<Tag>& out) {
    uint16_t n = 0;
    if (!c.u16(n) || !c.skip(8)) return malformed("truncated tags chunk");
    for (uint16_t i = 0; i < n; ++i) {
        Tag t;
        if (!c.u16(t.from) || !c.u16(t.to) || !c.u8(t.direction) || !c.u16(t.repeat) ||
            !c.skip(6 + 3 + 1) || !c.str(t.name)) {
            return malformed("truncated tag " + std::to_string(i));
        }
        out.push_back(std::move(t));
    }
    return {};
}

void setPaletteEntry(std::vector<std::array<uint8_t, 4>>& pal, size_t index,
                     const std::array<uint8_t, 4>& rgba) {
    if (pal.size() <= index) pal.resize(index + 1, {0, 0, 0, 0});
    pal[index] = rgba;
}

Error parseNewPalette(Cursor c, std::vector<std::array<uint8_t, 4>>& pal) {
    uint32_t size = 0, first = 0, last = 0;
    if (!c.u32(size) || !c.u32(first) || !c.u32(last) || !c.skip(8)) {
        return malformed("truncated palette chunk");
    }
    // RGBA palettes may exceed 256 entries; each entry is read from the chunk,
    // so the range only bounds the resize.
    if (last < first || last > 0xFFFF) return malformed("invalid palette range");
    for (uint32_t i = first; i <= last; ++i) {
        uint16_t flags = 0;
        std::array<uint8_t, 4> rgba{};
        if (!c.u16(flags) || !c.u8(rgba[0]) || !c.u8(rgba[1]) || !c.u8(rgba[2]) || !c.u8(rgba[3])) {
            return malformed("truncated palette entry " + std::to_string(i));
        }
        std::string name;
        if ((flags & PALETTE_ENTRY_HAS_NAME) && !c.str(name)) return malformed("truncated palette entry name");
        setPaletteEntry(pal, i, rgba);
    }
    return {};
}

Error parseOldPalette(Cursor c, std::vector<std::array<uint8_t, 4>>& pal) {
    uint16_t packets = 0;
    if (!c.u16(packets)) return malformed("truncated old palette chunk");
    size_t index = 0;
    for (uint16_t p = 0; p < packets; ++p) {
        uint8_t skip = 0, count = 0;
        if (!c.u8(skip) || !c.u8(count)) return malformed("truncated old palette packet");
        index += skip;
        const size_t n = count == 0 ? 256 : count;
        if (index + n > 256) return malformed("old palette runs past 256 colours");
        for (size_t k = 0; k < n; ++k, ++index) {
            std::array<uint8_t, 4> rgba{0, 0, 0, 255};
            if (!c.u8(rgba[0]) || !c.u8(rgba[1]) || !c.u8(rgba[2])) {
                return malformed("truncated old palette colour");
            }
            setPaletteEntry(pal, index, rgba);
        }
    }
    return {};
}

} // namespace

Error parse(const uint8_t* data, size_t size, const SpriteImportLimits& limits, File& out) {
    out = File{};
    if (data == nullptr || size < HEADER_SIZE) return malformed("not an .aseprite file (too small)");
    if (size > limits.maxFileBytes) {
        return tooLarge("file is " + std::to_string(size) + " bytes; the cap is " +
                        std::to_string(limits.maxFileBytes));
    }
    Cursor h(data, HEADER_SIZE);
    uint32_t fileSize = 0, flags = 0;
    uint16_t magic = 0, frames = 0;
    h.u32(fileSize);
    h.u16(magic);
    h.u16(frames);
    h.u16(out.width);
    h.u16(out.height);
    h.u16(out.depth);
    h.u32(flags);
    h.u16(out.speed);
    h.skip(8);
    h.u8(out.transparentIndex);

    if (magic != FILE_MAGIC) return malformed("not an .aseprite file (bad magic)");
    if (fileSize > size) {
        return malformed("truncated file: header says " + std::to_string(fileSize) + " bytes, found " +
                         std::to_string(size));
    }
    if (frames == 0) return malformed("file has no frames");
    if (out.width == 0 || out.height == 0) return malformed("zero canvas size");
    if (out.depth != DEPTH_INDEXED && out.depth != DEPTH_RGBA) {
        return unsupported("only indexed and RGBA sprites are supported (found " + std::to_string(out.depth) + "-bit)");
    }
    if (out.width > limits.maxCanvasSide || out.height > limits.maxCanvasSide) {
        return tooLarge("canvas " + std::to_string(out.width) + "x" + std::to_string(out.height) +
                        " exceeds the " + std::to_string(limits.maxCanvasSide) + " px cap");
    }
    if (frames > limits.maxFrames) {
        return tooLarge(std::to_string(frames) + " frames exceed the cap of " + std::to_string(limits.maxFrames));
    }

    const bool opacityValid = (flags & HEADER_FLAG_LAYER_OPACITY) != 0;
    // Cels are indexed by layer; layers may (in principle) arrive after cels,
    // so collect (layer, cel) pairs per frame and place them at the end.
    std::vector<std::vector<std::pair<uint16_t, Cel>>> frameCels(frames);
    out.durations.resize(frames);

    size_t offset = HEADER_SIZE;
    for (size_t fi = 0; fi < frames; ++fi) {
        if (size - offset < FRAME_HEADER_SIZE) return malformed("truncated frame header" + at(fi));
        Cursor fh(data + offset, FRAME_HEADER_SIZE);
        uint32_t frameBytes = 0, chunksNew = 0;
        uint16_t fmagic = 0, chunksOld = 0;
        fh.u32(frameBytes);
        fh.u16(fmagic);
        fh.u16(chunksOld);
        fh.u16(out.durations[fi]);
        fh.skip(2);
        fh.u32(chunksNew);
        if (fmagic != FRAME_MAGIC) return malformed("bad frame magic" + at(fi));
        if (frameBytes < FRAME_HEADER_SIZE || frameBytes > size - offset) {
            return malformed("invalid or truncated frame" + at(fi));
        }
        const uint32_t chunks = chunksNew != 0 ? chunksNew : chunksOld;
        const size_t frameEnd = offset + frameBytes;
        size_t co = offset + FRAME_HEADER_SIZE;

        for (uint32_t ci = 0; ci < chunks; ++ci) {
            if (frameEnd - co < CHUNK_HEADER_SIZE) return malformed("truncated chunk header" + at(fi));
            Cursor ch(data + co, CHUNK_HEADER_SIZE);
            uint32_t chunkBytes = 0;
            uint16_t type = 0;
            ch.u32(chunkBytes);
            ch.u16(type);
            if (chunkBytes < CHUNK_HEADER_SIZE || chunkBytes > frameEnd - co) {
                return malformed("invalid chunk size" + at(fi));
            }
            const Cursor body(data + co + CHUNK_HEADER_SIZE, chunkBytes - CHUNK_HEADER_SIZE);
            Error err;
            switch (type) {
            case CHUNK_LAYER: {
                if (out.layers.size() >= limits.maxLayers) {
                    return tooLarge("more than " + std::to_string(limits.maxLayers) + " layers");
                }
                Layer l;
                err = parseLayer(body, opacityValid, l);
                out.layers.push_back(std::move(l));
                break;
            }
            case CHUNK_CEL: {
                uint16_t layer = 0;
                Cel cel;
                err = parseCel(body, fi, limits, layer, cel);
                frameCels[fi].emplace_back(layer, cel);
                break;
            }
            case CHUNK_TAGS:
                err = parseTags(body, out.tags);
                break;
            case CHUNK_PALETTE:
                err = parseNewPalette(body, out.palette);
                break;
            case CHUNK_OLD_PALETTE:
                err = parseOldPalette(body, out.palette);
                break;
            default:
                break;  // cel extra, colour profile, user data, slices, tileset, …
            }
            if (err) return err;
            co += chunkBytes;
        }
        if (co != frameEnd) return malformed("chunk data does not fill the declared frame size" + at(fi));
        offset = frameEnd;
    }

    out.cels.assign(frames, std::vector<Cel>(out.layers.size()));
    for (size_t fi = 0; fi < frames; ++fi) {
        for (const auto& [layer, cel] : frameCels[fi]) {
            if (layer >= out.layers.size()) {
                return malformed("cel on unknown layer " + std::to_string(layer) + at(fi));
            }
            if (out.cels[fi][layer].present) {
                return malformed("two cels on layer " + std::to_string(layer) + at(fi));
            }
            out.cels[fi][layer] = cel;
        }
    }
    return {};
}

const Cel* resolve(const File& f, size_t frame, size_t layer, size_t& frameOut, Error& err) {
    size_t fi = frame;
    // A chain longer than the frame count must revisit a frame: a cycle.
    for (size_t hops = 0; hops <= f.cels.size(); ++hops) {
        const Cel& c = f.cels[fi][layer];
        if (!c.present) {
            if (fi != frame) {
                err = malformed("linked cel" + at(frame) + ", layer " + std::to_string(layer) + " has no source cel");
            }
            return nullptr;
        }
        if (c.type != CEL_LINKED) {
            frameOut = fi;
            return &c;
        }
        if (c.linkedFrame >= f.cels.size()) {
            err = malformed("linked cel" + at(fi) + " references frame " + std::to_string(c.linkedFrame));
            return nullptr;
        }
        fi = c.linkedFrame;
    }
    err = malformed("linked cel cycle" + at(frame) + ", layer " + std::to_string(layer));
    return nullptr;
}

Error decodeCel(const File& f, const Cel& cel, size_t frame, size_t layer,
                uint64_t& budget, std::vector<uint8_t>& out) {
    const std::string where = at(frame) + ", layer " + std::to_string(layer);
    if (cel.type != CEL_RAW && cel.type != CEL_COMPRESSED) {
        return unsupported("unsupported cel type " + std::to_string(cel.type) + where);
    }
    const uint64_t bytes = static_cast<uint64_t>(cel.w) * cel.h * f.bytesPerPixel();
    if (bytes > budget) return tooLarge("decoded cel pixels exceed the cap" + where);
    budget -= bytes;
    out.assign(static_cast<size_t>(bytes), 0);
    if (cel.type == CEL_RAW) {
        if (cel.payloadSize != bytes) {
            return malformed("raw cel" + where + " has " + std::to_string(cel.payloadSize) +
                             " pixel bytes; expected " + std::to_string(bytes));
        }
        if (bytes != 0) std::copy(cel.payload, cel.payload + bytes, out.begin());
        return {};
    }
    if (bytes == 0) return {};
    if (!inflateZlib(cel.payload, cel.payloadSize, out.data(), out.size())) {
        return malformed("corrupt compressed cel" + where);
    }
    return {};
}

} // namespace ase
} // namespace enjin2
