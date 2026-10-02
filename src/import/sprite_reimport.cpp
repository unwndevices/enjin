/**
 * @file sprite_reimport.cpp
 * @brief Re-import merge: a new sprite source into an existing `.njn` (Tomodachi #297)
 *
 * Pipeline: read the existing `.njn` (kind, animation frames, clips) → check the
 * new source still fits that kind → import it with importSprite() at that kind
 * → merge clips by name (sprite_reimport.hpp) → write the existing chunks in
 * their order, the source-owned ones and `CLIP` swapped → re-read the result.
 */
#include <enjin2/import/sprite_reimport.hpp>

#include <algorithm>
#include <cstring>
#include <set>

#include "aseprite_file.hpp"

namespace enjin2 {

namespace {

using ase::Error;

constexpr size_t MAX_CLIPS = 255;

/// The chunks a sprite source owns, by kind: a re-import replaces them.
const std::vector<NjnChunkTag>& sourceChunks(SpriteKind kind) {
    static const std::vector<NjnChunkTag> sheet = {NJN2_CHUNK_META, NJN2_CHUNK_PIXL};
    static const std::vector<NjnChunkTag> layered = {NJN2_CHUNK_LHDR, NJN2_CHUNK_LIMG, NJN2_CHUNK_LPRT,
                                                     NJN2_CHUNK_LREF, NJN2_CHUNK_LDUR};
    return kind == SpriteKind::Layered ? layered : sheet;
}

bool ownedBySource(const NjnChunkTag& id, SpriteKind kind) {
    const auto& owned = sourceChunks(kind);
    return std::find(owned.begin(), owned.end(), id) != owned.end();
}

/// A clip's name as the CLIP writer writes it (at most 15 characters).
std::string clipName(const NjnClip& c) {
    return std::string(c.name, strnlen(c.name, sizeof c.name - 1));
}

std::vector<uint16_t> frameNumbers(const NjnClip& c) {
    std::vector<uint16_t> out;
    for (const auto& f : c.frames) out.push_back(f.frameIndex);
    return out;
}

bool sameClip(const NjnClip& a, const NjnClip& b) {
    if (a.loopMode != b.loopMode || a.frames.size() != b.frames.size()) return false;
    for (size_t i = 0; i < a.frames.size(); ++i) {
        const NjnFrameEntry& x = a.frames[i];
        const NjnFrameEntry& y = b.frames[i];
        if (x.frameIndex != y.frameIndex || x.durationMs != y.durationMs || x.eventId != y.eventId) return false;
    }
    return true;
}

/// The existing `.njn` as far as the merge needs it.
struct Existing {
    SpriteKind kind = SpriteKind::Sheet;
    uint32_t frames = 0;              ///< Animation frames (a sheet's cells).
    std::vector<NjnClip> clips;
};

/// Read the existing `.njn` the way the runtime would (it is untrusted, like the source).
Error readExisting(const NjnV2Reader& rd, Existing& out) {
    auto bad = [](const std::string& m) { return ase::malformed("the existing .njn " + m); };
    for (const NjnChunkTag& id : {NJN2_CHUNK_META, NJN2_CHUNK_PIXL, NJN2_CHUNK_LHDR, NJN2_CHUNK_LIMG, NJN2_CHUNK_LPRT,
                                  NJN2_CHUNK_LREF, NJN2_CHUNK_LDUR, NJN2_CHUNK_CLIP}) {
        if (rd.count(id) > 1) return bad("has more than one " + njn2TagString(id) + " chunk");
    }
    if (rd.find(NJN2_CHUNK_LHDR) != nullptr) {
        NjnLayered a;
        const char* err = nullptr;
        if (!njn2DecodeLayered(rd, a, &err, /*materializePixels=*/false)) {
            return bad(std::string("isn't a readable layered sprite: ") + (err ? err : "malformed"));
        }
        out.kind = SpriteKind::Layered;
        out.frames = static_cast<uint32_t>(a.durations.size());
        out.clips = std::move(a.clips);
        return {};
    }
    uint8_t cw = 0, ch = 0, cols = 0, rows = 0;
    if (!njn2DecodeMeta(rd.find(NJN2_CHUNK_META), cw, ch, cols, rows)) {
        return bad("holds no sprite (no readable META or LHDR chunk)");
    }
    const NjnV2Chunk* pixl = rd.find(NJN2_CHUNK_PIXL);
    if (pixl == nullptr || pixl->size != size_t(cw) * ch * cols * rows) return bad("has no PIXL chunk of its META's size");
    const NjnV2Chunk* clip = rd.find(NJN2_CHUNK_CLIP);
    if (clip != nullptr && !njn2DecodeClip(clip, out.clips)) return bad("has a malformed CLIP chunk");
    out.kind = SpriteKind::Sheet;
    out.frames = uint32_t(cols) * rows;
    for (const auto& c : out.clips) {
        for (const auto& f : c.frames) {
            if (f.frameIndex >= out.frames) return bad("has a CLIP frame index out of range");
        }
    }
    return {};
}

/// The new source no longer fits the `.njn`'s kind (auto-detected as on first import).
Error kindCheck(const ase::File& f, SpriteKind existing) {
    const auto visible = static_cast<size_t>(std::count_if(f.layers.begin(), f.layers.end(),
                                                           [](const ase::Layer& l) { return l.visible; }));
    if (visible == 0) return {};  // importSprite() reports it
    const SpriteKind fits = visible > 1 ? SpriteKind::Layered : SpriteKind::Sheet;
    if (fits == existing) return {};
    if (existing == SpriteKind::Sheet) {
        return {SpriteImportStatus::KindMismatch,
                "the .njn is a sheet, but the new source has " + std::to_string(visible) +
                    " visible layers (a layered sprite); hide all but one layer, or import it as a new sprite"};
    }
    return {SpriteImportStatus::KindMismatch,
            "the .njn is a layered sprite, but the new source has one visible layer (a sheet); "
            "show its other layers, or import it as a new sprite"};
}

/// Re-read the merged file the way the runtime will.
bool validates(const std::vector<uint8_t>& njn, SpriteKind kind) {
    NjnV2Reader rd;
    if (!rd.open(njn.data(), njn.size())) return false;
    Existing e;
    return !readExisting(rd, e) && e.kind == kind;
}

} // namespace

SpriteReimportResult reimportSprite(const uint8_t* njn, size_t njnSize,
                                    const uint8_t* source, size_t sourceSize,
                                    const SpriteFollowSet& follows,
                                    const SpriteReimportOptions& opts) {
    SpriteReimportResult r;
    auto fail = [&r](Error e) {
        r.status = e.status;
        r.error = std::move(e.message);
        r.njn.clear();
        return r;
    };

    // --- The existing .njn ---
    NjnV2Reader old;
    const char* openErr = nullptr;
    if (njn == nullptr || !old.open(njn, njnSize, &openErr)) {
        return fail(ase::malformed(std::string("the existing .njn doesn't read: ") + (openErr ? openErr : "malformed")));
    }
    Existing ex;
    if (Error e = readExisting(old, ex)) return fail(e);
    r.kind = ex.kind;
    r.framesBefore = ex.frames;

    // --- The new source, at the existing kind ---
    if (ase::isPng(source, sourceSize)) {
        if (ex.kind != SpriteKind::Sheet) return fail({SpriteImportStatus::KindMismatch, "PNG source is a sheet, but the .njn is layered"});
    } else {
        ase::File file;
        if (Error e = ase::parse(source, sourceSize, opts.limits, file)) return fail(e);
        if (Error e = kindCheck(file, ex.kind)) return fail(e);
    }
    SpriteImportOptions io;
    io.kind = ex.kind;
    io.palette = opts.palette;
    io.limits = opts.limits;
    io.cellW = opts.cellW;
    io.cellH = opts.cellH;
    SpriteImportResult imp = importSprite(source, sourceSize, io);
    if (!imp.ok()) {
        r.colourIssues = imp.colourIssues;
        return fail({imp.status, imp.error});
    }
    NjnV2Reader fresh;
    std::vector<NjnClip> seeded;  // in imp.clips order
    if (!fresh.open(imp.njn.data(), imp.njn.size()) || !njn2DecodeClip(fresh.find(NJN2_CHUNK_CLIP), seeded) ||
        seeded.size() != imp.clips.size()) {
        return fail(ase::malformed("internal error: the new source's import failed to re-read"));
    }
    const uint16_t frames = imp.frames;
    r.framesAfter = frames;

    auto seededFor = [&](const std::string& tag) -> int {
        for (size_t i = 0; i < imp.clips.size(); ++i) {
            if (imp.clips[i].fromTag && imp.clips[i].tag == tag) return static_cast<int>(i);
        }
        return -1;
    };

    // --- Existing clips, by name, in place ---
    std::vector<NjnClip> merged;
    std::set<size_t> followed;  // seeded clips some existing clip follows
    for (const NjnClip& c : ex.clips) {
        SpriteReimportClip rep;
        rep.name = clipName(c);
        rep.before = frameNumbers(c);
        rep.loopBefore = c.loopMode;
        NjnClip next = c;
        const auto follow = follows.find(rep.name);
        const int si = follow == follows.end() ? -1 : seededFor(follow->second);
        if (si >= 0) {
            // Followed: the tag's new frames, seeded as on first import; the name stays.
            followed.insert(static_cast<size_t>(si));
            next.frames = seeded[si].frames;
            next.loopMode = seeded[si].loopMode;
            rep.following = true;
            r.follows[rep.name] = follow->second;
        } else {
            // Edited (or its tag is gone): keep it, minus the frames that no longer exist.
            if (follow != follows.end()) {
                rep.tagRemoved = true;
                r.tags.push_back({SpriteTagChangeKind::Removed, follow->second, rep.name, false});
            }
            next.frames.clear();
            for (size_t i = 0; i < c.frames.size(); ++i) {
                if (c.frames[i].frameIndex < frames) next.frames.push_back(c.frames[i]);
                else rep.removed.push_back(static_cast<uint16_t>(i));
            }
            rep.emptied = !c.frames.empty() && next.frames.empty();
        }
        rep.after = frameNumbers(next);
        rep.loopAfter = next.loopMode;
        rep.changed = !sameClip(c, next);
        if (rep.following && rep.changed) {
            r.tags.push_back({SpriteTagChangeKind::Changed, follow->second, rep.name, false});
        }
        merged.push_back(std::move(next));
        r.clips.push_back(std::move(rep));
    }

    // --- New tags: a seeded, following clip each, unless the name is taken ---
    for (size_t i = 0; i < seeded.size(); ++i) {
        if (!imp.clips[i].fromTag || followed.count(i) != 0) continue;
        const std::string& tag = imp.clips[i].tag;
        const std::string name = clipName(seeded[i]);
        const auto taken = std::find_if(r.clips.begin(), r.clips.end(),
                                        [&](const SpriteReimportClip& c) { return c.name == name; });
        if (taken != r.clips.end()) {
            if (taken->clashTag.empty()) taken->clashTag = tag;
            r.tags.push_back({SpriteTagChangeKind::Added, tag, name, true});
            continue;
        }
        if (merged.size() >= MAX_CLIPS) {
            return fail(ase::tooLarge("new tag '" + tag + "' would make more than " + std::to_string(MAX_CLIPS) + " clips"));
        }
        SpriteReimportClip rep;
        rep.name = name;
        rep.after = frameNumbers(seeded[i]);
        rep.loopBefore = rep.loopAfter = seeded[i].loopMode;
        rep.added = rep.following = rep.changed = true;
        r.follows[name] = tag;
        r.tags.push_back({SpriteTagChangeKind::Added, tag, name, false});
        merged.push_back(seeded[i]);
        r.clips.push_back(std::move(rep));
    }

    r.touchesNoClip = std::none_of(r.clips.begin(), r.clips.end(), [](const SpriteReimportClip& c) {
        return c.changed || c.added || c.tagRemoved || c.emptied;
    });

    // --- The merged file: the existing chunks in order, source-owned ones and CLIP swapped ---
    const bool clipsKept = std::none_of(r.clips.begin(), r.clips.end(),
                                        [](const SpriteReimportClip& c) { return c.changed; });
    NjnV2Writer w;
    auto copy = [&w](const NjnChunkTag& id, const uint8_t* data, size_t size) {
        w.beginChunk(id);
        if (size != 0) w.writeBytes(data, size);
        w.endChunk();
    };
    bool wroteClip = false;
    for (const NjnV2Chunk& c : old.chunks()) {
        if (c.id == NJN2_CHUNK_CLIP) {
            if (clipsKept) copy(c.id, c.data, c.size);  // byte-for-byte when nothing changed
            else njn2WriteClip(w, merged.data(), static_cast<uint8_t>(merged.size()));
            wroteClip = true;
        } else if (ownedBySource(c.id, ex.kind)) {
            const NjnV2Chunk* n = fresh.find(c.id);
            if (n != nullptr) copy(c.id, n->data, n->size);
        } else {
            copy(c.id, c.data, c.size);  // LPIV, unknown chunks, …
        }
    }
    for (const NjnChunkTag& id : sourceChunks(ex.kind)) {
        const NjnV2Chunk* n = fresh.find(id);
        if (old.find(id) == nullptr && n != nullptr) copy(id, n->data, n->size);
    }
    if (!wroteClip) njn2WriteClip(w, merged.data(), static_cast<uint8_t>(merged.size()));
    w.finalise(r.njn);
    r.njn[3] = njn[3];  // reserved header byte, kept as found
    if (!validates(r.njn, ex.kind)) {
        return fail(ase::malformed("internal error: the merged .njn failed to re-read"));
    }

    imp.njn.clear();
    r.source = std::move(imp);
    r.status = SpriteImportStatus::Ok;
    return r;
}

} // namespace enjin2
