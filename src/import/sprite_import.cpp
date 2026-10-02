/**
 * @file sprite_import.cpp
 * @brief `.aseprite` → `.njn` v2 sprite importer (ADR-0015, Tomodachi #291)
 *
 * Pipeline: parse (aseprite_file.cpp) → validate visible layers → resolve each
 * (frame, visible layer) to a cel use → convert used cels to palette indices,
 * tallying any pixel that can't be represented → build a flat sheet or a
 * layered sprite → re-read the output before returning it.
 */
#include <enjin2/import/sprite_import.hpp>

#include <algorithm>
#include <cmath>
#include <map>
#include <unordered_map>

#include "aseprite_file.hpp"

namespace enjin2 {

namespace {

using ase::Error;
using ase::tooLarge;
using ase::unsupported;

constexpr uint8_t  CLEAR = PALETTE_TRANSPARENT;  // 15
constexpr uint8_t  BAD   = 0xFF;                 // pixel that can't be imported
constexpr size_t   NAME_MAX_CHARS = 15;
constexpr size_t   MAX_CLIPS = 255;
constexpr size_t   MAX_CLIP_FRAMES = 255;
constexpr uint16_t SHEET_MAX_CELL = 255;         // META stores u8 cell sides
constexpr uint16_t FALLBACK_DURATION_MS = 100;

/// Printable ASCII only: each non-ASCII code point or control byte becomes
/// '_'; capped at 15 characters (the njn2 name field).
std::string asciiName(const std::string& raw) {
    std::string out;
    bool inSequence = false;
    for (char ch : raw) {
        const auto b = static_cast<uint8_t>(ch);
        if (inSequence && (b & 0xC0) == 0x80) continue;  // UTF-8 continuation
        inSequence = b >= 0xC0;
        out.push_back(b >= 0x20 && b < 0x7F ? ch : '_');
        if (out.size() == NAME_MAX_CHARS) break;
    }
    return out;
}

// ---------------------------------------------------------------------------
// Tags → clips
// ---------------------------------------------------------------------------

struct ClipPlan {
    std::string name;
    std::string tag;       ///< The source tag's name as in the file; empty for the default clip.
    bool fromTag = false;
    NjnLoopMode mode = NjnLoopMode::Loop;
    std::vector<uint16_t> frames;
};

/// Frame sequence and loop mode for one tag (see sprite_import.hpp, "Clips").
Error planTag(const ase::Tag& t, uint16_t lo, uint16_t hi, const std::string& name, ClipPlan& out) {
    const bool reverse  = t.direction == ase::TAG_REVERSE || t.direction == ase::TAG_PINGPONG_REVERSE;
    const bool pingpong = t.direction == ase::TAG_PINGPONG || t.direction == ase::TAG_PINGPONG_REVERSE;
    const size_t n = static_cast<size_t>(hi - lo) + 1;
    size_t total = n;
    if (t.repeat > 1) total = pingpong ? n + (t.repeat - 1) * (n - 1) : n * t.repeat;
    if (total > MAX_CLIP_FRAMES) {
        return tooLarge("tag '" + name + "' unrolls to " + std::to_string(total) +
                        " frames; a clip holds at most " + std::to_string(MAX_CLIP_FRAMES));
    }
    std::vector<uint16_t> pass;
    for (size_t i = 0; i < n; ++i) pass.push_back(static_cast<uint16_t>(lo + i));
    if (reverse) std::reverse(pass.begin(), pass.end());

    out.name = name;
    out.frames = pass;
    if (t.repeat == 0) {
        out.mode = pingpong ? NjnLoopMode::PingPong : NjnLoopMode::Loop;
        return {};
    }
    out.mode = NjnLoopMode::Once;
    for (uint16_t r = 1; r < t.repeat; ++r) {
        if (pingpong) {
            // Each ping-pong pass turns around without repeating the turn frame.
            std::reverse(pass.begin(), pass.end());
            out.frames.insert(out.frames.end(), pass.begin() + 1, pass.end());
        } else {
            out.frames.insert(out.frames.end(), pass.begin(), pass.end());
        }
    }
    return {};
}

Error planClips(const ase::File& f, std::vector<ClipPlan>& out) {
    const auto frames = static_cast<uint16_t>(f.durations.size());
    for (const auto& t : f.tags) {
        const uint16_t hi = std::min<uint16_t>(t.to, frames - 1);
        if (t.from > hi) continue;  // empty once clamped to the frame range
        std::string name = asciiName(t.name);
        if (name.empty()) name = "clip" + std::to_string(out.size());
        ClipPlan p;
        if (Error e = planTag(t, t.from, hi, name, p)) return e;
        p.tag = t.name;
        p.fromTag = true;
        out.push_back(std::move(p));
    }
    if (out.size() > MAX_CLIPS) {
        return tooLarge(std::to_string(out.size()) + " tags; a sprite holds at most " + std::to_string(MAX_CLIPS) + " clips");
    }
    if (out.empty()) {
        if (frames > MAX_CLIP_FRAMES) {
            return tooLarge("untagged file has " + std::to_string(frames) + " frames; the default clip holds at most " +
                            std::to_string(MAX_CLIP_FRAMES) + " (tag the frames)");
        }
        ClipPlan p;
        p.name = "default";
        for (uint16_t i = 0; i < frames; ++i) p.frames.push_back(i);
        out.push_back(std::move(p));
    }
    return {};
}

// ---------------------------------------------------------------------------
// Colours
// ---------------------------------------------------------------------------

struct Oklab { float L, a, b; };

float srgbToLinear(uint8_t c) {
    const float v = static_cast<float>(c) / 255.0f;
    return v <= 0.04045f ? v / 12.92f : std::pow((v + 0.055f) / 1.055f, 2.4f);
}

Oklab toOklab(uint8_t r8, uint8_t g8, uint8_t b8) {
    const float r = srgbToLinear(r8), g = srgbToLinear(g8), b = srgbToLinear(b8);
    const float l = std::cbrt(0.4122214708f * r + 0.5363325363f * g + 0.0514459929f * b);
    const float m = std::cbrt(0.2119034982f * r + 0.6806995451f * g + 0.1073969566f * b);
    const float s = std::cbrt(0.0883024619f * r + 0.2817188376f * g + 0.6299787005f * b);
    return {0.2104542553f * l + 0.7936177850f * m - 0.0040720468f * s,
            1.9779984951f * l - 2.4285922050f * m + 0.4505937099f * s,
            0.0259040371f * l + 0.7827717662f * m - 0.8086757660f * s};
}

uint8_t nearestSlot(const std::array<RGB, PALETTE_MAX_ENTRIES>& pal, uint8_t r, uint8_t g, uint8_t b) {
    const Oklab c = toOklab(r, g, b);
    uint8_t best = 0;
    float bestD = 0;
    for (uint8_t i = 0; i < PALETTE_MAX_ENTRIES; ++i) {
        const Oklab p = toOklab(pal[i].r, pal[i].g, pal[i].b);
        const float d = (p.L - c.L) * (p.L - c.L) + (p.a - c.a) * (p.a - c.a) + (p.b - c.b) * (p.b - c.b);
        if (i == 0 || d < bestD) { best = i; bestD = d; }
    }
    return best;
}

/// One stored cel converted to palette indices (CLEAR = transparent, BAD =
/// can't be imported).  `raw` is kept only when there are BAD pixels, to name them.
struct IndexCel {
    uint16_t w = 0, h = 0;
    std::vector<uint8_t> idx;
    std::vector<uint8_t> raw;
    std::vector<uint8_t> counted;  ///< Per BAD pixel: already tallied.
};

/// One (frame, visible layer) that shows a cel.
struct Use {
    IndexCel* cel;
    int x, y;
    int zIndex;
    size_t layer;   ///< Source layer index.
    size_t part;    ///< Position among visible layers.
};

class Importer {
public:
    Importer(const ase::File& f, const SpriteImportOptions& o)
        : m_file(f), m_opts(o), m_budget(o.limits.maxPixelBytes) {}

    SpriteImportResult run();

private:
    Error collectUses();
    Error convert(const ase::Cel& cel, size_t frame, size_t layer, IndexCel& out);
    void  tally(const Use& u);
    Error badPixelError(SpriteImportResult& r) const;
    Error buildSheet(NjnV2Writer& w);
    Error buildLayered(NjnV2Writer& w, SpriteImportResult& r);
    std::vector<NjnClip> clips() const;

    const ase::File& m_file;
    const SpriteImportOptions& m_opts;
    uint64_t m_budget;
    std::vector<size_t> m_visible;                 ///< Visible layer indices, painter order.
    std::vector<uint16_t> m_durations;             ///< Effective per-frame ms.
    std::vector<ClipPlan> m_clips;
    std::unordered_map<size_t, IndexCel> m_cels;   ///< Keyed by owner frame * layers + layer.
    std::vector<std::vector<Use>> m_uses;          ///< Per frame, in visible-layer order.
    std::map<uint32_t, uint32_t> m_bad;            ///< RGBA (or index) → pixel count.
};

Error Importer::convert(const ase::Cel& cel, size_t frame, size_t layer, IndexCel& out) {
    std::vector<uint8_t> px;
    if (Error e = ase::decodeCel(m_file, cel, frame, layer, m_budget, px)) return e;
    out.w = cel.w;
    out.h = cel.h;
    const size_t n = static_cast<size_t>(cel.w) * cel.h;
    out.idx.resize(n);
    bool anyBad = false;
    if (m_file.depth == ase::DEPTH_INDEXED) {
        for (size_t i = 0; i < n; ++i) {
            const uint8_t v = px[i];
            out.idx[i] = v == m_file.transparentIndex ? CLEAR : (v < PALETTE_MAX_ENTRIES ? v : BAD);
            anyBad = anyBad || out.idx[i] == BAD;
        }
    } else {
        for (size_t i = 0; i < n; ++i) {
            const uint8_t* p = &px[i * 4];
            uint8_t v = BAD;
            if (p[3] == 0) {
                v = CLEAR;
            } else if (p[3] == 255) {
                for (uint8_t s = 0; s < PALETTE_MAX_ENTRIES; ++s) {
                    const RGB& c = m_opts.palette[s];
                    if (c.r == p[0] && c.g == p[1] && c.b == p[2]) { v = s; break; }
                }
            }
            out.idx[i] = v;
            anyBad = anyBad || v == BAD;
        }
    }
    if (anyBad) {
        out.raw = std::move(px);
        out.counted.assign(n, 0);
    }
    return {};
}

/// Count this use's in-canvas BAD pixels not counted before.
void Importer::tally(const Use& u) {
    IndexCel& c = *u.cel;
    if (c.raw.empty()) return;
    for (int row = 0; row < c.h; ++row) {
        const int cy = u.y + row;
        if (cy < 0 || cy >= m_file.height) continue;
        for (int col = 0; col < c.w; ++col) {
            const int cx = u.x + col;
            if (cx < 0 || cx >= m_file.width) continue;
            const size_t i = static_cast<size_t>(row) * c.w + col;
            if (c.idx[i] != BAD || c.counted[i]) continue;
            c.counted[i] = 1;
            uint32_t key = c.raw[i * m_file.bytesPerPixel()];
            if (m_file.depth == ase::DEPTH_RGBA) {
                const uint8_t* p = &c.raw[i * 4];
                key = (uint32_t(p[0]) << 24) | (uint32_t(p[1]) << 16) | (uint32_t(p[2]) << 8) | p[3];
            }
            ++m_bad[key];
        }
    }
}

Error Importer::collectUses() {
    const size_t frames = m_file.durations.size();
    const size_t layers = m_file.layers.size();
    m_uses.assign(frames, {});
    for (size_t fi = 0; fi < frames; ++fi) {
        for (size_t part = 0; part < m_visible.size(); ++part) {
            const size_t li = m_visible[part];
            const ase::Cel& use = m_file.cels[fi][li];
            Error err;
            size_t owner = 0;
            const ase::Cel* src = ase::resolve(m_file, fi, li, owner, err);
            if (err) return err;
            if (src == nullptr) continue;
            if (use.opacity != 255) {
                return unsupported("cel opacity " + std::to_string(use.opacity) + " on layer '" + m_file.layers[li].name +
                                   "' in frame " + std::to_string(fi) + "; only 255 is supported");
            }
            const size_t key = owner * layers + li;
            auto it = m_cels.find(key);
            if (it == m_cels.end()) {
                IndexCel ic;
                if (Error e = convert(*src, owner, li, ic)) return e;
                it = m_cels.emplace(key, std::move(ic)).first;
            }
            Use u{&it->second, use.x, use.y, use.zIndex, li, part};
            tally(u);
            m_uses[fi].push_back(u);
        }
    }
    return {};
}

Error Importer::badPixelError(SpriteImportResult& r) const {
    if (m_file.depth == ase::DEPTH_INDEXED) {
        std::string list;
        for (const auto& [index, count] : m_bad) {
            list += (list.empty() ? "" : ", ") + std::to_string(index) + " (" + std::to_string(count) + " px)";
        }
        return unsupported("opaque palette indices outside 0..14: " + list +
                           "; enjin sprites use slots 0..14 and 15 is transparent");
    }
    for (const auto& [key, count] : m_bad) {
        SpriteColourIssue i{static_cast<uint8_t>(key >> 24), static_cast<uint8_t>(key >> 16),
                            static_cast<uint8_t>(key >> 8), static_cast<uint8_t>(key), count, 0};
        i.nearestSlot = nearestSlot(m_opts.palette, i.r, i.g, i.b);
        r.colourIssues.push_back(i);
    }
    std::stable_sort(r.colourIssues.begin(), r.colourIssues.end(),
                     [](const SpriteColourIssue& a, const SpriteColourIssue& b) { return a.pixels > b.pixels; });
    size_t alpha = 0;
    for (const auto& i : r.colourIssues) alpha += i.partialAlpha() ? 1 : 0;
    return {SpriteImportStatus::PaletteMismatch,
            std::to_string(r.colourIssues.size() - alpha) + " colour(s) not in the palette and " +
                std::to_string(alpha) + " partial-alpha value(s); RGBA sprites need binary alpha "
                "and exact palette colours"};
}

std::vector<NjnClip> Importer::clips() const {
    std::vector<NjnClip> out;
    for (const auto& p : m_clips) {
        NjnClip c{};
        std::copy(p.name.begin(), p.name.end(), c.name);
        c.loopMode = p.mode;
        for (uint16_t fi : p.frames) c.frames.push_back({fi, m_durations[fi], 0});
        out.push_back(std::move(c));
    }
    return out;
}

/// The sheet `.njn` grid for @p count frames (mirrors enjin_assets.emit.choose_grid).
/// False when the frames don't fit META's 255x255 cells.
bool sheetGrid(size_t count, uint8_t& cols, uint8_t& rows) {
    if (count <= 255) {
        cols = static_cast<uint8_t>(count);
        rows = 1;
        return true;
    }
    const size_t r = (count + 254) / 255;
    if (r > 255) return false;
    cols = 255;
    rows = static_cast<uint8_t>(r);
    return true;
}

Error Importer::buildSheet(NjnV2Writer& w) {
    const uint16_t cw = m_file.width, ch = m_file.height;
    if (cw > SHEET_MAX_CELL || ch > SHEET_MAX_CELL) {
        return tooLarge("a sheet cell is at most 255x255; this canvas is " + std::to_string(cw) + "x" +
                        std::to_string(ch) + " (import it as a layered sprite)");
    }
    uint8_t cols = 0, rows = 0;
    if (!sheetGrid(m_uses.size(), cols, rows)) {
        return tooLarge(std::to_string(m_uses.size()) + " frames exceed the 255x255 sheet grid");
    }
    const size_t cell = static_cast<size_t>(cw) * ch;
    const uint64_t sheetBytes = static_cast<uint64_t>(cell) * cols * rows;
    if (sheetBytes > m_budget) return tooLarge("the sheet's pixels exceed the decoded-pixel cap");
    m_budget -= sheetBytes;
    std::vector<uint8_t> pixl(cell * cols * rows, CLEAR);
    for (size_t fi = 0; fi < m_uses.size(); ++fi) {
        // Aseprite spec NOTE.5: order = layer + z-index, ties broken by z-index.
        std::vector<Use> order = m_uses[fi];
        std::stable_sort(order.begin(), order.end(), [](const Use& a, const Use& b) {
            const long oa = long(a.layer) + a.zIndex, ob = long(b.layer) + b.zIndex;
            return oa != ob ? oa < ob : a.zIndex < b.zIndex;
        });
        uint8_t* dst = &pixl[fi * cell];
        for (const Use& u : order) {
            for (int row = 0; row < u.cel->h; ++row) {
                const int cy = u.y + row;
                if (cy < 0 || cy >= ch) continue;
                for (int col = 0; col < u.cel->w; ++col) {
                    const int cx = u.x + col;
                    const uint8_t v = u.cel->idx[static_cast<size_t>(row) * u.cel->w + col];
                    if (cx >= 0 && cx < cw && v != CLEAR) dst[static_cast<size_t>(cy) * cw + cx] = v;
                }
            }
        }
    }
    njn2WriteMeta(w, static_cast<uint8_t>(cw), static_cast<uint8_t>(ch), cols, rows);
    njn2WritePixl(w, pixl.data(), static_cast<uint32_t>(pixl.size()));
    const auto cl = clips();
    njn2WriteClip(w, cl.data(), static_cast<uint8_t>(cl.size()));
    return {};
}

Error Importer::buildLayered(NjnV2Writer& w, SpriteImportResult& r) {
    NjnLayered a;
    a.canvasW = m_file.width;
    a.canvasH = m_file.height;
    a.durations = m_durations;
    for (size_t li : m_visible) {
        std::string name = asciiName(m_file.layers[li].name);
        if (name.empty()) name = "part" + std::to_string(li);
        NjnPart p{};
        std::copy(name.begin(), name.end(), p.name);
        a.parts.push_back(p);
        r.parts.push_back(name);
    }
    for (const auto& frameUses : m_uses) {
        for (const Use& u : frameUses) {
            if (u.zIndex != 0) {
                return unsupported("nonzero cel z-index " + std::to_string(u.zIndex) + " on layer '" +
                                   m_file.layers[u.layer].name + "'; layered sprites paint in layer order");
            }
        }
    }

    // Per part: (w, h, pixels) → image index, so identical images are shared.
    std::vector<std::map<std::vector<uint8_t>, uint16_t>> seen(m_visible.size());
    const NjnFramePartRef invisible{NJN2_LAYERED_INVISIBLE, 0, 0};
    for (const auto& frameUses : m_uses) {
        std::vector<NjnFramePartRef> refs(m_visible.size(), invisible);
        for (const Use& u : frameUses) {
            // Clip to the canvas, then crop to the opaque bounds.
            const int x0 = std::max(0, u.x), y0 = std::max(0, u.y);
            const int x1 = std::min<int>(m_file.width, u.x + u.cel->w), y1 = std::min<int>(m_file.height, u.y + u.cel->h);
            int minX = x1, minY = y1, maxX = -1, maxY = -1;
            for (int y = y0; y < y1; ++y) {
                for (int x = x0; x < x1; ++x) {
                    if (u.cel->idx[static_cast<size_t>(y - u.y) * u.cel->w + (x - u.x)] == CLEAR) continue;
                    minX = std::min(minX, x); maxX = std::max(maxX, x);
                    minY = std::min(minY, y); maxY = std::max(maxY, y);
                }
            }
            if (maxX < 0) continue;  // empty within the canvas → invisible
            const auto cw = static_cast<uint16_t>(maxX - minX + 1), ch = static_cast<uint16_t>(maxY - minY + 1);
            std::vector<uint8_t> key = {uint8_t(cw), uint8_t(cw >> 8), uint8_t(ch), uint8_t(ch >> 8)};
            for (int y = minY; y <= maxY; ++y) {
                const uint8_t* row = &u.cel->idx[static_cast<size_t>(y - u.y) * u.cel->w + (minX - u.x)];
                key.insert(key.end(), row, row + cw);
            }
            auto [it, fresh] = seen[u.part].emplace(key, static_cast<uint16_t>(a.images.size()));
            if (fresh) {
                if (a.images.size() >= NJN2_LAYERED_INVISIBLE) return tooLarge("more than 65534 part images");
                a.images.push_back({cw, ch, std::vector<uint8_t>(key.begin() + 4, key.end())});
            }
            refs[u.part] = {it->second, static_cast<int16_t>(minX), static_cast<int16_t>(minY)};
        }
        a.refs.insert(a.refs.end(), refs.begin(), refs.end());
    }
    if (a.images.empty()) return unsupported("no visible artwork: every cel is empty");

    // New layered imports pivot at the bottom centre.
    a.pivotX = static_cast<int16_t>(a.canvasW / 2);
    a.pivotY = static_cast<int16_t>(a.canvasH - 1);
    a.clips = clips();
    njn2WriteLayered(w, a);
    r.images = static_cast<uint16_t>(a.images.size());
    return {};
}

/// Re-read our own output the way the runtime will.
bool validates(const std::vector<uint8_t>& njn, SpriteKind kind) {
    NjnV2Reader rd;
    if (!rd.open(njn.data(), njn.size())) return false;
    if (kind == SpriteKind::Layered) {
        NjnLayered a;
        return njn2DecodeLayered(rd, a);
    }
    uint8_t cw = 0, ch = 0, cols = 0, rows = 0;
    const NjnV2Chunk* pixl = rd.find(NJN2_CHUNK_PIXL);
    std::vector<NjnClip> cl;
    if (!njn2DecodeMeta(rd.find(NJN2_CHUNK_META), cw, ch, cols, rows) || pixl == nullptr ||
        pixl->size != size_t(cw) * ch * cols * rows || !njn2DecodeClip(rd.find(NJN2_CHUNK_CLIP), cl)) {
        return false;
    }
    for (const auto& c : cl) {
        for (const auto& f : c.frames) {
            if (f.frameIndex >= size_t(cols) * rows) return false;
        }
    }
    return true;
}

SpriteImportResult Importer::run() {
    SpriteImportResult r;
    auto fail = [&r](Error e) {
        r.status = e.status;
        r.error = std::move(e.message);
        r.njn.clear();
        return r;
    };

    for (size_t li = 0; li < m_file.layers.size(); ++li) {
        const auto& l = m_file.layers[li];
        if (!l.visible) {
            r.hiddenLayers.push_back(l.name);
            continue;
        }
        const std::string label = "layer '" + l.name + "'";
        if (l.type == ase::LAYER_GROUP || l.childLevel != 0) return fail(unsupported("group layers are unsupported (" + label + ")"));
        if (l.type == ase::LAYER_TILEMAP) return fail(unsupported("tilemap layers are unsupported (" + label + ")"));
        if (l.type != ase::LAYER_IMAGE) return fail(unsupported("unsupported layer type " + std::to_string(l.type) + " (" + label + ")"));
        if (l.blendMode != 0) {
            return fail(unsupported("blend mode " + std::to_string(l.blendMode) + " on " + label + "; only normal blend is supported"));
        }
        if (l.opacity != 255) {
            return fail(unsupported("layer opacity " + std::to_string(l.opacity) + " on " + label + "; only 255 is supported"));
        }
        m_visible.push_back(li);
    }
    if (m_visible.empty()) return fail(unsupported("no visible layers"));

    const SpriteKind kind = m_opts.kind != SpriteKind::Auto ? m_opts.kind
                          : (m_visible.size() > 1 ? SpriteKind::Layered : SpriteKind::Sheet);

    for (uint16_t d : m_file.durations) {
        m_durations.push_back(d != 0 ? d : (m_file.speed != 0 ? m_file.speed : FALLBACK_DURATION_MS));
    }
    if (Error e = planClips(m_file, m_clips)) return fail(e);
    if (Error e = collectUses()) return fail(e);
    if (!m_bad.empty()) return fail(badPixelError(r));

    NjnV2Writer w;
    if (Error e = kind == SpriteKind::Sheet ? buildSheet(w) : buildLayered(w, r)) return fail(e);
    w.finalise(r.njn);
    if (!validates(r.njn, kind)) {
        return fail({SpriteImportStatus::Malformed, "internal error: the importer's output failed to re-read"});
    }

    r.status = SpriteImportStatus::Ok;
    r.kind = kind;
    r.canvasW = m_file.width;
    r.canvasH = m_file.height;
    r.frames = static_cast<uint16_t>(m_file.durations.size());
    for (const auto& c : m_clips) r.clips.push_back({c.name, c.mode, static_cast<uint16_t>(c.frames.size()), c.tag, c.fromTag});
    return r;
}

} // namespace

std::array<RGB, PALETTE_MAX_ENTRIES> systemPalette() {
    const Palette p;
    std::array<RGB, PALETTE_MAX_ENTRIES> out{};
    std::copy(std::begin(p.colors), std::end(p.colors), out.begin());
    return out;
}

SpriteImportResult importSprite(const uint8_t* data, size_t size, const SpriteImportOptions& opts) {
    ase::File f;
    std::vector<uint8_t> pixels, cells;
    Error error;
    if (ase::isPng(data, size)) {
        bool indexed = false;
        error = ase::decodePng(data, size, opts.limits, f.width, f.height, indexed, pixels, f.palette);
        if (!error) {
            const uint16_t w = f.width, h = f.height;
            const uint16_t cw = opts.cellW ? opts.cellW : w, ch = opts.cellH ? opts.cellH : h;
            if ((opts.cellW == 0) != (opts.cellH == 0) || w % cw || h % ch) error = ase::malformed("PNG cell size must divide the image exactly");
            else if (opts.kind == SpriteKind::Layered) error = unsupported("PNG imports only as a sheet");
            else if (cw > 255 || ch > 255) error = tooLarge("PNG sheet cells must be at most 255x255");
            else if (!opts.limits.maxLayers || cw > opts.limits.maxCelSide || ch > opts.limits.maxCelSide)
                error = tooLarge("PNG exceeds the layer or cel-size cap");
            else {
                const size_t count = size_t(w / cw) * (h / ch);
                if (count > opts.limits.maxFrames || count > MAX_CLIP_FRAMES) error = tooLarge("PNG has too many animation frames for one clip");
                else {
                    f.width = cw; f.height = ch;
                    f.depth = indexed ? ase::DEPTH_INDEXED : ase::DEPTH_RGBA;
                    f.transparentIndex = 15;
                    f.layers.push_back({"png", true});
                    f.durations.assign(count, FALLBACK_DURATION_MS);
                    f.cels.resize(count);
                    cells.resize(pixels.size());
                    const size_t bpp = f.bytesPerPixel(), cellBytes = size_t(cw) * ch * bpp;
                    for (size_t fi = 0; fi < count; ++fi) {
                        for (size_t y = 0; y < ch; ++y) {
                            const size_t src = ((fi / (w / cw) * ch + y) * w + fi % (w / cw) * cw) * bpp;
                            std::copy_n(pixels.data() + src, size_t(cw) * bpp, cells.data() + fi * cellBytes + y * cw * bpp);
                        }
                        ase::Cel cel;
                        cel.present = true; cel.w = cw; cel.h = ch;
                        cel.payload = cells.data() + fi * cellBytes; cel.payloadSize = cellBytes;
                        f.cels[fi].push_back(cel);
                    }
                }
            }
        }
    } else error = ase::parse(data, size, opts.limits, f);
    if (Error e = error) {
        SpriteImportResult r;
        r.status = e.status;
        r.error = std::move(e.message);
        return r;
    }
    SpriteImportResult r = Importer(f, opts).run();
    r.sourcePalette = f.palette;
    return r;
}

const char* spriteImportStatusName(SpriteImportStatus s) {
    switch (s) {
    case SpriteImportStatus::Ok:              return "ok";
    case SpriteImportStatus::Malformed:       return "malformed";
    case SpriteImportStatus::Unsupported:     return "unsupported";
    case SpriteImportStatus::TooLarge:        return "too-large";
    case SpriteImportStatus::PaletteMismatch: return "palette-mismatch";
    case SpriteImportStatus::KindMismatch:    return "kind-mismatch";
    }
    return "unknown";
}

} // namespace enjin2
