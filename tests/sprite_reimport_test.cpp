/**
 * @file sprite_reimport_test.cpp
 * @brief Tests for the re-import merge (Tomodachi #297), run by sprite_import_test
 *
 * Each test imports a source, edits the `.njn` the way the Studio would (CLIP,
 * LPIV, an unknown chunk) and re-imports a changed source over it.
 *
 * Test IDs:
 *   SPR-01  An identical source touches no clip and gives the same bytes, with
 *           the full follow set and with none (the CLI: clashes, no changes).
 *   SPR-02  A frame inserted in the source shifts what an edited clip's frames
 *           show (numbers kept); its followed tag's clip grows.
 *   SPR-03  A removed frame removes the clip frames past the end, reported.
 *   SPR-04  A followed tag's range and direction change updates its clip.
 *   SPR-05  An edited clip isn't touched (ms, events, loop mode kept).
 *   SPR-06  A removed tag keeps its clip, detached and flagged.
 *   SPR-07  A new tag adds a seeded, following clip after the existing ones.
 *   SPR-08  A new tag named like an edited clip doesn't take it over.
 *   SPR-09  A clip left empty is kept and flagged.
 *   SPR-10  LPIV and unknown chunks survive byte-for-byte, in place (layered).
 *   SPR-11  A source that no longer fits the kind fails, both ways.
 *   SPR-12  A renamed clip keeps following its tag; no clip is added for it.
 *   SPR-13  Bad inputs fail cleanly: existing `.njn` (garbage, truncations,
 *           flips), source errors carried through with their colour issues.
 */

#include "sprite_import_fixtures.hpp"

#include <enjin2/import/sprite_reimport.hpp>

namespace {

NjnClip makeClip(const char* name, NjnLoopMode mode, const std::vector<NjnFrameEntry>& frames) {
    NjnClip c{};
    std::strncpy(c.name, name, 15);
    c.loopMode = mode;
    c.frames = frames;
    return c;
}

/// @p njn with its CLIP chunk replaced by @p clips.
Bytes withClips(const Bytes& njn, const std::vector<NjnClip>& clips) {
    NjnV2Writer w;
    njn2WriteClip(w, clips.data(), static_cast<uint8_t>(clips.size()));
    Bytes one;
    w.finalise(one);
    NjnV2Reader rd;
    rd.open(one.data(), one.size());
    const NjnV2Chunk* c = rd.find(NJN2_CHUNK_CLIP);
    NjnEditResult e = njn2ReplaceChunk(njn.data(), njn.size(), NJN2_CHUNK_CLIP, c->data, c->size);
    return e.njn;
}

Bytes withChunk(const Bytes& njn, const NjnChunkTag& id, const Bytes& data) {
    return njn2ReplaceChunk(njn.data(), njn.size(), id, data.data(), data.size()).njn;
}

/// A chunk's bytes and its directory position, or {} / -1.
Bytes chunkOf(const Bytes& njn, const NjnChunkTag& id, int* index = nullptr) {
    NjnV2Reader rd;
    if (!rd.open(njn.data(), njn.size())) return {};
    int i = 0;
    for (const NjnV2Chunk& c : rd.chunks()) {
        if (c.id == id) {
            if (index) *index = i;
            return Bytes(c.data, c.data + c.size);
        }
        ++i;
    }
    if (index) *index = -1;
    return {};
}

/// A one-layer indexed source: frame i fills pixel (0,0) with @p colours[i], durations 100 + 10 i.
Bytes sheetSource(const std::vector<uint8_t>& colours, const std::vector<Tag>& tags) {
    AseBuilder a(2, 2, 8, colours.size());
    a.add(0, AseBuilder::layer("body"));
    for (size_t i = 0; i < colours.size(); ++i) {
        a.frames[i].duration = static_cast<uint16_t>(100 + 10 * i);
        a.add(i, AseBuilder::cel(0, 0, 0, 2, 2, {colours[i], 0, 0, 0}));
    }
    if (!tags.empty()) a.add(0, AseBuilder::tags(tags));
    return a.build();
}

/// A two-layer indexed source (layered): frame i's eye pixel is @p colours[i].
Bytes layeredSource(const std::vector<uint8_t>& colours, const std::vector<Tag>& tags) {
    AseBuilder a(4, 4, 8, colours.size());
    a.add(0, AseBuilder::layer("base"));
    a.add(0, AseBuilder::layer("eye"));
    for (size_t i = 0; i < colours.size(); ++i) {
        a.add(i, AseBuilder::cel(0, 0, 2, 4, 2, Bytes(8, 3)));
        a.add(i, AseBuilder::cel(1, 1, 0, 1, 1, {colours[i]}));
    }
    if (!tags.empty()) a.add(0, AseBuilder::tags(tags));
    return a.build();
}

Bytes importOf(const Bytes& source) {
    const SpriteImportResult r = importSprite(source.data(), source.size());
    return r.njn;
}

SpriteReimportResult reimport(const Bytes& njn, const Bytes& source, const SpriteFollowSet& follows) {
    return reimportSprite(njn.data(), njn.size(), source.data(), source.size(), follows);
}

const SpriteReimportClip* reportOf(const SpriteReimportResult& r, const char* name) {
    for (const auto& c : r.clips) {
        if (c.name == name) return &c;
    }
    return nullptr;
}

const NjnClip* clipIn(const std::vector<NjnClip>& clips, const char* name) {
    for (const auto& c : clips) {
        if (std::strncmp(c.name, name, 16) == 0) return &c;
    }
    return nullptr;
}

std::vector<NjnClip> clipsOf(const Bytes& njn) { return readSheet(njn).clips; }

bool hasTagChange(const SpriteReimportResult& r, SpriteTagChangeKind kind, const char* tag, const char* clip,
                  bool clash = false) {
    for (const auto& t : r.tags) {
        if (t.kind == kind && t.tag == tag && t.clip == clip && t.clash == clash) return true;
    }
    return false;
}

using U16 = std::vector<uint16_t>;

// ---------------------------------------------------------------------------
// Tests
// ---------------------------------------------------------------------------

void testR01_identical() {
    const Bytes src = sheetSource({1, 2, 3, 4}, {{0, 1, 0, 0, "walk"}, {2, 3, 2, 0, "Jump!"}});
    const Bytes njn = importOf(src);
    const SpriteFollowSet follows = {{"walk", "walk"}, {"Jump!", "Jump!"}};
    const auto r = reimport(njn, src, follows);
    ASSERT(r.ok(), "SPR-01 identical source re-imports");
    ASSERT(r.touchesNoClip, "SPR-01 identical source touches no clip");
    ASSERT(r.njn == njn, "SPR-01 identical source → identical bytes");
    ASSERT(r.follows == follows, "SPR-01 follow set kept");
    ASSERT(r.tags.empty(), "SPR-01 no tag changes");
    ASSERT(r.kind == SpriteKind::Sheet && r.framesBefore == 4 && r.framesAfter == 4, "SPR-01 kind and frame counts");
    ASSERT(r.clips.size() == 2 && r.clips[0].following && !r.clips[0].changed && r.clips[0].after == (U16{0, 1}),
           "SPR-01 per-clip report");

    const auto cli = reimport(njn, src, {});
    ASSERT(cli.ok() && cli.touchesNoClip && cli.njn == njn, "SPR-01 empty follow set: same bytes, touches no clip");
    ASSERT(cli.follows.empty(), "SPR-01 empty follow set stays empty");
    ASSERT(hasTagChange(cli, SpriteTagChangeKind::Added, "walk", "walk", true) &&
           hasTagChange(cli, SpriteTagChangeKind::Added, "Jump!", "Jump!", true),
           "SPR-01 empty follow set: every tag clashes with its edited clip");
    const auto* walk = reportOf(cli, "walk");
    ASSERT(walk && walk->clashTag == "walk" && !walk->following, "SPR-01 the clash is flagged on the clip");
}

void testR02_insertedFrame() {
    const Bytes src = sheetSource({1, 2, 3, 4}, {{0, 3, 0, 0, "walk"}});
    Bytes njn = importOf(src);
    auto clips = clipsOf(njn);
    clips.push_back(makeClip("pose", NjnLoopMode::Once, {{2, 50, 7}, {3, 60, 0}}));
    njn = withClips(njn, clips);

    // A frame (colour 9) inserted at 1: old frames 1..3 are now 2..4.
    const Bytes src2 = sheetSource({1, 9, 2, 3, 4}, {{0, 4, 0, 0, "walk"}});
    const auto r = reimport(njn, src2, {{"walk", "walk"}});
    ASSERT(r.ok() && !r.touchesNoClip, "SPR-02 re-import ok, touches clips");
    const Sheet s = readSheet(r.njn);
    ASSERT(s.ok && s.cols == 5 && s.pixl.size() == 20, "SPR-02 PIXL replaced: five cells");
    ASSERT(s.ok && s.pixl[4 * 2] == 2 && s.pixl[4 * 3] == 3, "SPR-02 cells 2 and 3 now show the old frames 1 and 2");
    const NjnClip* pose = clipIn(s.clips, "pose");
    ASSERT(pose && frameIdx(*pose) == (U16{2, 3}) && pose->frames[0].durationMs == 50 && pose->frames[0].eventId == 7,
           "SPR-02 the edited clip keeps its frame numbers, ms and events");
    const NjnClip* walk = clipIn(s.clips, "walk");
    ASSERT(walk && frameIdx(*walk) == (U16{0, 1, 2, 3, 4}) && walk->frames[1].durationMs == 110,
           "SPR-02 the followed clip takes the tag's new range and ms");
    const auto* rw = reportOf(r, "walk");
    ASSERT(rw && rw->before == (U16{0, 1, 2, 3}) && rw->after == (U16{0, 1, 2, 3, 4}) && rw->changed && rw->following,
           "SPR-02 report: walk before → after");
    const auto* rp = reportOf(r, "pose");
    ASSERT(rp && !rp->changed && rp->removed.empty(), "SPR-02 report: pose unchanged");
    ASSERT(hasTagChange(r, SpriteTagChangeKind::Changed, "walk", "walk"), "SPR-02 tag change: walk changed");
    ASSERT(r.framesBefore == 4 && r.framesAfter == 5, "SPR-02 frame counts");
}

void testR03_removedFrame() {
    const Bytes src = sheetSource({1, 2, 3, 4}, {});
    Bytes njn = importOf(src);
    njn = withClips(njn, {makeClip("pose", NjnLoopMode::Loop, {{0, 40, 0}, {3, 40, 1}, {1, 40, 0}, {3, 40, 2}})});
    const auto r = reimport(njn, sheetSource({1, 2, 3}, {}), {});
    ASSERT(r.ok() && !r.touchesNoClip, "SPR-03 re-import ok, touches a clip");
    const auto clips = clipsOf(r.njn);
    ASSERT(clips.size() == 1 && frameIdx(clips[0]) == (U16{0, 1}) && clips[0].frames[1].durationMs == 40,
           "SPR-03 clip frames past the end removed, the rest kept");
    const auto* rp = reportOf(r, "pose");
    ASSERT(rp && rp->removed == (U16{1, 3}) && rp->before == (U16{0, 3, 1, 3}) && rp->after == (U16{0, 1}) && rp->changed,
           "SPR-03 the removed clip frames are reported by position");
    ASSERT(clipIn(clips, "default") == nullptr, "SPR-03 no default clip added on a re-import");
}

void testR04_tagRangeChange() {
    Bytes njn = importOf(sheetSource({1, 2, 3, 4}, {{0, 1, 0, 0, "walk"}}));
    const auto r = reimport(njn, sheetSource({1, 2, 3, 4}, {{1, 3, 1, 0, "walk"}}), {{"walk", "walk"}});
    ASSERT(r.ok(), "SPR-04 re-import ok");
    const auto clips = clipsOf(r.njn);
    ASSERT(clips.size() == 1 && frameIdx(clips[0]) == (U16{3, 2, 1}) && clips[0].loopMode == NjnLoopMode::Loop,
           "SPR-04 the followed clip takes the tag's new reversed range");
    ASSERT(clips.size() == 1 && clips[0].frames[0].durationMs == 130, "SPR-04 ms from the source");
    const auto* rw = reportOf(r, "walk");
    ASSERT(rw && rw->before == (U16{0, 1}) && rw->after == (U16{3, 2, 1}) && rw->following && rw->changed,
           "SPR-04 report: before → after");
    ASSERT(r.follows == (SpriteFollowSet{{"walk", "walk"}}), "SPR-04 still following");

    const auto pp = reimport(njn, sheetSource({1, 2, 3, 4}, {{0, 1, 2, 0, "walk"}}), {{"walk", "walk"}});
    const auto* pw = reportOf(pp, "walk");
    ASSERT(pp.ok() && pw && pw->loopBefore == NjnLoopMode::Loop && pw->loopAfter == NjnLoopMode::PingPong && pw->changed &&
           !pp.touchesNoClip, "SPR-04 a direction change alone changes the clip");
}

void testR05_editedUntouched() {
    Bytes njn = importOf(sheetSource({1, 2, 3, 4}, {{0, 1, 0, 0, "walk"}}));
    njn = withClips(njn, {makeClip("walk", NjnLoopMode::PingPong, {{1, 33, 4}, {0, 44, 0}, {1, 55, 9}})});
    const Bytes before = chunkOf(njn, NJN2_CHUNK_CLIP);
    const auto r = reimport(njn, sheetSource({5, 6, 7, 8}, {{0, 3, 0, 0, "walk"}}), {});
    ASSERT(r.ok() && r.touchesNoClip, "SPR-05 re-import ok; the edited clip isn't touched");
    ASSERT(chunkOf(r.njn, NJN2_CHUNK_CLIP) == before, "SPR-05 CLIP byte-for-byte");
    ASSERT(readSheet(r.njn).pixl[0] == 5, "SPR-05 pixels replaced");
    const auto* rw = reportOf(r, "walk");
    ASSERT(rw && !rw->changed && !rw->following && rw->loopAfter == NjnLoopMode::PingPong, "SPR-05 report: unchanged, edited");
}

void testR06_removedTag() {
    Bytes njn = importOf(sheetSource({1, 2, 3, 4}, {{0, 1, 0, 0, "walk"}, {2, 3, 0, 0, "jump"}}));
    const auto r = reimport(njn, sheetSource({1, 2, 3, 4}, {{0, 1, 0, 0, "walk"}}), {{"walk", "walk"}, {"jump", "jump"}});
    ASSERT(r.ok() && !r.touchesNoClip, "SPR-06 re-import ok; a removed tag touches its clip");
    const auto clips = clipsOf(r.njn);
    const NjnClip* jump = clipIn(clips, "jump");
    ASSERT(clips.size() == 2 && jump && frameIdx(*jump) == (U16{2, 3}), "SPR-06 the clip stays, frames kept");
    const auto* rj = reportOf(r, "jump");
    ASSERT(rj && rj->tagRemoved && !rj->following && !rj->changed, "SPR-06 flagged, detached");
    ASSERT(r.follows == (SpriteFollowSet{{"walk", "walk"}}), "SPR-06 dropped from the follow set");
    ASSERT(hasTagChange(r, SpriteTagChangeKind::Removed, "jump", "jump"), "SPR-06 tag change: jump removed");
}

void testR07_newTag() {
    Bytes njn = importOf(sheetSource({1, 2, 3, 4}, {{0, 1, 0, 0, "walk"}}));
    const auto r = reimport(njn, sheetSource({1, 2, 3, 4}, {{0, 1, 0, 0, "walk"}, {2, 3, 2, 0, "Run"}}),
                            {{"walk", "walk"}});
    ASSERT(r.ok() && !r.touchesNoClip, "SPR-07 a new tag touches the clips");
    const auto clips = clipsOf(r.njn);
    ASSERT(clips.size() == 2 && std::strcmp(clips[1].name, "Run") == 0 && frameIdx(clips[1]) == (U16{2, 3}) &&
           clips[1].loopMode == NjnLoopMode::PingPong, "SPR-07 the new clip is seeded, after the existing ones");
    const auto* rr = reportOf(r, "Run");
    ASSERT(rr && rr->added && rr->following && rr->before.empty() && rr->after == (U16{2, 3}), "SPR-07 report: added");
    ASSERT(r.follows == (SpriteFollowSet{{"walk", "walk"}, {"Run", "Run"}}), "SPR-07 the new clip follows its tag");
    ASSERT(hasTagChange(r, SpriteTagChangeKind::Added, "Run", "Run"), "SPR-07 tag change: Run added");
}

void testR08_nameClash() {
    Bytes njn = importOf(sheetSource({1, 2, 3, 4}, {{0, 1, 0, 0, "walk"}}));
    njn = withClips(njn, {makeClip("walk", NjnLoopMode::Loop, {{0, 100, 0}}), makeClip("run", NjnLoopMode::Once, {{3, 20, 0}})});
    const auto r = reimport(njn, sheetSource({1, 2, 3, 4}, {{0, 1, 0, 0, "walk"}, {1, 2, 0, 0, "run"}}),
                            {{"walk", "walk"}});
    ASSERT(r.ok(), "SPR-08 re-import ok");
    const auto clips = clipsOf(r.njn);
    const NjnClip* run = clipIn(clips, "run");
    ASSERT(clips.size() == 2 && run && frameIdx(*run) == (U16{3}) && run->loopMode == NjnLoopMode::Once,
           "SPR-08 the edited clip isn't taken over");
    const auto* rr = reportOf(r, "run");
    ASSERT(rr && rr->clashTag == "run" && !rr->following && !rr->changed, "SPR-08 the clash is flagged on the clip");
    ASSERT(hasTagChange(r, SpriteTagChangeKind::Added, "run", "run", true), "SPR-08 tag change: run added, clashed");
    ASSERT(r.follows.count("run") == 0, "SPR-08 the clashing clip doesn't follow");
}

void testR09_emptied() {
    Bytes njn = importOf(sheetSource({1, 2, 3, 4}, {}));
    njn = withClips(njn, {makeClip("tail", NjnLoopMode::Once, {{3, 10, 0}, {2, 10, 0}}),
                          makeClip("blank", NjnLoopMode::Loop, {})});
    const auto r = reimport(njn, sheetSource({1, 2}, {}), {});
    ASSERT(r.ok() && !r.touchesNoClip, "SPR-09 re-import ok");
    const auto clips = clipsOf(r.njn);
    const NjnClip* tail = clipIn(clips, "tail");
    ASSERT(clips.size() == 2 && tail && tail->frames.empty(), "SPR-09 the emptied clip is kept, empty");
    const auto* rt = reportOf(r, "tail");
    ASSERT(rt && rt->emptied && rt->removed == (U16{0, 1}), "SPR-09 flagged emptied");
    const auto* rb = reportOf(r, "blank");
    ASSERT(rb && !rb->emptied && !rb->changed, "SPR-09 an already-empty clip isn't 'emptied'");
}

void testR10_layeredKeepsChunks() {
    const Bytes src = layeredSource({5, 6, 7}, {{0, 2, 0, 0, "blink"}});
    Bytes njn = importOf(src);
    const Bytes pivot = {0xFE, 0xFF, 0x07, 0x00};  // (-2, 7)
    njn = withChunk(njn, NJN2_CHUNK_LPIV, pivot);
    const NjnChunkTag xtra = njnTag('X', 'T', 'R', 'A');
    njn = withChunk(njn, xtra, {1, 2, 3});
    int lpivAt = -1, xtraAt = -1;
    chunkOf(njn, NJN2_CHUNK_LPIV, &lpivAt);
    chunkOf(njn, xtra, &xtraAt);

    const auto r = reimport(njn, layeredSource({8, 9, 10, 11}, {{0, 3, 0, 0, "blink"}}), {{"blink", "blink"}});
    ASSERT(r.ok() && r.kind == SpriteKind::Layered, "SPR-10 layered re-import ok");
    int lpivAfter = -2, xtraAfter = -2;
    ASSERT(chunkOf(r.njn, NJN2_CHUNK_LPIV, &lpivAfter) == pivot && lpivAfter == lpivAt, "SPR-10 LPIV kept, in place");
    ASSERT(chunkOf(r.njn, xtra, &xtraAfter) == (Bytes{1, 2, 3}) && xtraAfter == xtraAt, "SPR-10 unknown chunk kept, in place");
    NjnLayered L;
    ASSERT(readLayered(r.njn, L) && L.durations.size() == 4 && L.pivotX == -2 && L.pivotY == 7,
           "SPR-10 the result decodes: new frames, kept pivot");
    ASSERT(L.clips.size() == 1 && frameIdx(L.clips[0]) == (U16{0, 1, 2, 3}), "SPR-10 followed clip grows");

    // No LPIV before (pivot (0,0)): none after, though a fresh layered import writes one.
    Bytes noPivot;
    {
        NjnV2Reader rd;
        rd.open(njn.data(), njn.size());
        NjnV2Writer w;
        for (const NjnV2Chunk& c : rd.chunks()) {
            if (c.id == NJN2_CHUNK_LPIV) continue;
            w.beginChunk(c.id);
            w.writeBytes(c.data, c.size);
            w.endChunk();
        }
        w.finalise(noPivot);
    }
    const auto r2 = reimport(noPivot, layeredSource({8, 9}, {{0, 1, 0, 0, "blink"}}), {{"blink", "blink"}});
    int none = 0;
    ASSERT(r2.ok() && chunkOf(r2.njn, NJN2_CHUNK_LPIV, &none).empty() && none == -1, "SPR-10 an absent LPIV stays absent");
}

void testR11_kindMismatch() {
    const Bytes sheet = importOf(sheetSource({1, 2}, {}));
    const auto a = reimport(sheet, layeredSource({5, 6}, {}), {});
    ASSERT(a.status == SpriteImportStatus::KindMismatch && contains(a.error, "sheet") && a.njn.empty(),
           "SPR-11 a sheet's source gaining visible layers fails");
    const Bytes layered = importOf(layeredSource({5, 6}, {}));
    const auto b = reimport(layered, sheetSource({1, 2}, {}), {});
    ASSERT(b.status == SpriteImportStatus::KindMismatch && contains(b.error, "layered"),
           "SPR-11 a layered sprite's source down to one visible layer fails");
    ASSERT(std::strcmp(spriteImportStatusName(SpriteImportStatus::KindMismatch), "kind-mismatch") == 0,
           "SPR-11 status name");
}

void testR12_renamedFollows() {
    Bytes njn = importOf(sheetSource({1, 2, 3, 4}, {{0, 1, 0, 0, "walk"}}));
    auto clips = clipsOf(njn);
    std::strncpy(clips[0].name, "stroll", 16);
    njn = withClips(njn, clips);
    const auto r = reimport(njn, sheetSource({1, 2, 3, 4}, {{0, 2, 0, 0, "walk"}}), {{"stroll", "walk"}});
    const auto out = clipsOf(r.njn);
    ASSERT(r.ok() && out.size() == 1 && std::strcmp(out[0].name, "stroll") == 0 && frameIdx(out[0]) == (U16{0, 1, 2}),
           "SPR-12 the renamed clip follows its tag's new range");
    ASSERT(r.follows == (SpriteFollowSet{{"stroll", "walk"}}), "SPR-12 still following, by its new name");
    ASSERT(!hasTagChange(r, SpriteTagChangeKind::Added, "walk", "walk"), "SPR-12 the followed tag isn't new");

    const auto stale = reimport(njn, sheetSource({1, 2, 3, 4}, {{0, 1, 0, 0, "walk"}}), {{"gone", "walk"}, {"stroll", "walk"}});
    ASSERT(stale.ok() && stale.follows == (SpriteFollowSet{{"stroll", "walk"}}), "SPR-12 a follow entry for no clip is dropped");
}

void testR13_badInputs() {
    const Bytes src = sheetSource({1, 2}, {{0, 1, 0, 0, "walk"}});
    const Bytes njn = importOf(src);
    const auto garbage = reimport(Bytes{1, 2, 3}, src, {});
    ASSERT(garbage.status == SpriteImportStatus::Malformed && contains(garbage.error, "existing"),
           "SPR-13 an unreadable existing .njn fails, named");
    Bytes clipOnly;
    {
        NjnV2Writer w;
        njn2WriteClip(w, nullptr, 0);
        w.finalise(clipOnly);
    }
    const auto noSprite = reimport(clipOnly, src, {});
    ASSERT(noSprite.status == SpriteImportStatus::Malformed, "SPR-13 a .njn with no sprite chunks fails");

    // An RGBA source off the palette: the importer's error and issues come through.
    AseBuilder rgba(1, 1, 32, 1);
    rgba.add(0, AseBuilder::layer("body"));
    rgba.add(0, AseBuilder::cel(0, 0, 0, 1, 1, {1, 2, 3, 255}));
    const Bytes bad = rgba.build();
    const auto pm = reimport(njn, bad, {});
    ASSERT(pm.status == SpriteImportStatus::PaletteMismatch && pm.colourIssues.size() == 1 && pm.njn.empty(),
           "SPR-13 a palette mismatch carries its colour issues");

    // Truncations and byte flips of the existing .njn never crash (ASan in the gate).
    bool ok = true;
    for (size_t cut = 0; cut < njn.size(); ++cut) {
        const Bytes b(njn.begin(), njn.begin() + static_cast<long>(cut));
        const auto r = reimport(b, src, {{"walk", "walk"}});
        ok = ok && (r.ok() || !r.error.empty());
    }
    uint32_t seed = 0x2970u;
    auto rnd = [&]() { seed = seed * 1664525u + 1013904223u; return seed >> 8; };
    for (int i = 0; i < 3000; ++i) {
        Bytes b = njn;
        for (int k = 0; k < 1 + static_cast<int>(rnd() % 3); ++k) b[rnd() % b.size()] = static_cast<uint8_t>(rnd());
        const auto r = reimport(b, src, {{"walk", "walk"}});
        ok = ok && (r.ok() || !r.error.empty());
        NjnLayered L;
        if (r.ok()) ok = ok && (readSheet(r.njn).ok || readLayered(r.njn, L));
    }
    ASSERT(ok, "SPR-13 truncated or corrupted .njn: an error or a readable result, never a crash");
}

} // namespace

void runReimportTests() {
    testR01_identical();
    testR02_insertedFrame();
    testR03_removedFrame();
    testR04_tagRangeChange();
    testR05_editedUntouched();
    testR06_removedTag();
    testR07_newTag();
    testR08_nameClash();
    testR09_emptied();
    testR10_layeredKeepsChunks();
    testR11_kindMismatch();
    testR12_renamedFollows();
    testR13_badInputs();
}
