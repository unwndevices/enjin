// The Shape bit-identity corpus (Tomodachi #337, ADR-0016): the scripted run
// on the bundled stand-in Star. Its CRC goldens live with the hosts that
// compare them (Tomodachi's libtomo suite, WASM and the device); here, what
// the script promises: every phase ends asleep, the drag carries the pet,
// breathing redraws only on its steps, and two runs are the same run.

#include <enjin2/graphics/asset_arena.hpp>
#include <enjin2/graphics/shape_pack.hpp>
#include <enjin2/import/shape_bundle.hpp>
#include <enjin2/shape/shape_corpus.hpp>

#include <cstdio>
#include <cstdlib>
#include <fstream>
#include <iterator>
#include <memory>
#include <string>
#include <vector>

using namespace enjin2;
using namespace enjin2::shape;

static int passes = 0;
static int failures = 0;

#define ASSERT(cond, msg) \
    do { \
        if (!(cond)) { \
            fprintf(stderr, "FAIL: %s (line %d)\n", msg, __LINE__); \
            failures++; \
        } else { \
            passes++; \
        } \
    } while (0)

static std::vector<uint8_t> readFile(const std::string& path) {
    std::ifstream in(path, std::ios::binary);
    return {std::istreambuf_iterator<char>(in), std::istreambuf_iterator<char>()};
}

static std::vector<uint8_t> starPack() {
    const std::string dir = std::string(ENJIN2_SOURCE_DIR) + "/tools/testdata/shapes/";
    const std::vector<uint8_t> ledger = readFile(dir + "ledger.json");
    const ShapeBundleResult r =
        bundleShapes({{"star.svg", readFile(dir + "star.svg"), readFile(dir + "star.svg.shape.json")}},
                     std::string(reinterpret_cast<const char*>(ledger.data()), ledger.size()));
    return r.ok() ? r.pack : std::vector<uint8_t>{};
}

static ShapeDrawScratch scratch;

struct Run {
    std::vector<ShapeCorpusFrame> frames;
    bool failed = false;
    FixedVec2 endFeet{};
};

static Run runCorpus(const std::vector<uint8_t>& bytes) {
    Run run;
    ShapePackMemory src(bytes.data(), bytes.size());
    ShapePack pack;
    ShapeError e;
    if (!pack.open(src, &e)) return run;
    AssetArena arena;
    arena.allocateBacking(64 * 1024);
    std::unique_ptr<ShapeCorpus> corpus(new ShapeCorpus);
    if (!corpus->begin(pack, arena, &e)) {
        fprintf(stderr, "  begin: %s\n", e.text);
        return run;
    }
    static ShapeCorpusLayer layer;
    layer.clear(Pixel4(3));  // frame 0 clears it to transparent
    ShapeCorpusFrame f;
    while (corpus->step(layer, scratch, f)) run.frames.push_back(f);
    run.failed = corpus->failed();
    run.endFeet = corpus->pet().position();
    return run;
}

static void testCrc() {
    static ShapeCorpusLayer layer;
    layer.clear(Pixel4(15));
    // CRC-32/ISO-HDLC of 25600 bytes of 0x0F (zlib.crc32(bytes([15]) * 25600)).
    ASSERT(shapeLayerCrc(layer) == 0x85A04443u, "a blank layer's CRC is the zlib CRC-32 of its index bytes");
    layer.setPixel(0, 0, Pixel4(0));
    ASSERT(shapeLayerCrc(layer) != 0x85A04443u, "one pixel changes it");
}

static void testFrameLine() {
    ShapeCorpusFrame f;
    f.index = 172;
    f.phase = ShapeCorpusPhase::Pull;
    f.redrew = true;
    f.asleep = false;
    f.crc = 0x0365399Bu;
    char line[ShapeCorpusFrame::kLineMax];
    ASSERT(formatShapeCorpusFrame(f, line, sizeof line) == 21, "the line's length");
    ASSERT(std::string(line) == "172 pull 0365399B 1 0", "frame, phase, CRC (8 hex digits), redraw, asleep");
    f.index = 0;
    f.phase = ShapeCorpusPhase::Breathe;
    f.redrew = false;
    f.asleep = true;
    f.crc = 0xFFFFFFFFu;
    formatShapeCorpusFrame(f, line, sizeof line);
    ASSERT(std::string(line) == "0 breathe FFFFFFFF 0 1", "the longest phase name fits");
    char small[8];
    ASSERT(formatShapeCorpusFrame(f, small, sizeof small) == 0 && small[0] == '\0', "too small a buffer: nothing");
}

static void testScript() {
    const std::vector<uint8_t> bytes = starPack();
    ASSERT(!bytes.empty(), "the Star bundles");
    const Run run = runCorpus(bytes);
    ASSERT(!run.failed, "the grab lands on the pet");
    ASSERT(run.frames.size() == ShapeCorpus::kFrames, "every frame runs");
    if (run.frames.size() != ShapeCorpus::kFrames) return;
    for (size_t i = 0; i < run.frames.size(); ++i) ASSERT(run.frames[i].index == i, "frames in order");

    ASSERT(run.frames[0].phase == ShapeCorpusPhase::Draw && run.frames[0].redrew, "frame 0 draws the pet");
    if (getenv("SHAPE_VERBOSE")) {
        // Per phase: redraws, and the frame the jelly fell asleep (the script's margins).
        for (size_t i = 0; i < run.frames.size();) {
            size_t j = i;
            int redraws = 0;
            size_t sleptAt = i;
            while (j < run.frames.size() && run.frames[j].phase == run.frames[i].phase) {
                redraws += run.frames[j].redrew;
                if (!run.frames[j].asleep) sleptAt = j + 1;
                ++j;
            }
            fprintf(stderr, "  %-7s frames %3zu..%3zu  %3d redraws  asleep from %zu\n",
                    shapeCorpusPhaseName(run.frames[i].phase), i, j - 1, redraws, sleptAt);
            i = j;
        }
    }
    // Each phase's last frame is asleep, and only the scripted inputs wake it.
    for (size_t i = 1; i < run.frames.size(); ++i) {
        const bool last = i + 1 == run.frames.size() || run.frames[i + 1].phase != run.frames[i].phase;
        if (last) ASSERT(run.frames[i].asleep, "every phase settles before the next");
    }
    // The script's frames, by phase.
    const auto first = [&](ShapeCorpusPhase p) -> const ShapeCorpusFrame& {
        for (const auto& f : run.frames)
            if (f.phase == p) return f;
        return run.frames.back();
    };
    const auto last = [&](ShapeCorpusPhase p) -> const ShapeCorpusFrame& {
        for (size_t i = run.frames.size(); i-- > 0;)
            if (run.frames[i].phase == p) return run.frames[i];
        return run.frames.front();
    };
    ASSERT(first(ShapeCorpusPhase::Kick).index == 11 && !first(ShapeCorpusPhase::Kick).asleep, "the kick wakes it");
    ASSERT(first(ShapeCorpusPhase::Drag).index == 81 && !first(ShapeCorpusPhase::Drag).asleep, "the drag wakes it");
    ASSERT(first(ShapeCorpusPhase::Pull).index == 171 && !first(ShapeCorpusPhase::Pull).asleep, "the grab wakes it");
    ASSERT(first(ShapeCorpusPhase::Breathe).index == 271, "breathing starts at 271");

    const uint32_t rest = last(ShapeCorpusPhase::Rest).crc;
    ASSERT(last(ShapeCorpusPhase::Kick).crc == rest, "kicked, it settles back to the rest frame");
    ASSERT(first(ShapeCorpusPhase::Drag).crc != rest, "the drag moves it");
    // The drag carried the feet to (110, 140), and the pull leaves them there.
    ASSERT(run.endFeet.x == Fixed::fromInt(110) && run.endFeet.y == Fixed::fromInt(140), "the drag ends at (110, 140)");
    const uint32_t moved = last(ShapeCorpusPhase::Drag).crc;
    ASSERT(last(ShapeCorpusPhase::Pull).crc == moved, "pulled and thrown, it settles back to the moved rest frame");

    // Breathing: two step changes, two redraws, and the jelly never wakes.
    int breathRedraws = 0;
    bool awake = false;
    for (const auto& f : run.frames)
        if (f.phase == ShapeCorpusPhase::Breathe) {
            breathRedraws += f.redrew;
            awake = awake || !f.asleep;
        }
    ASSERT(breathRedraws == 2 && !awake, "two breath steps: two redraws, asleep throughout");
    ASSERT(last(ShapeCorpusPhase::Breathe).crc == moved, "the second step is back at the rest frame");

    // Redraws only when something moved: no redraw while asleep and unchanged.
    for (size_t i = 1; i < run.frames.size(); ++i)
        if (!run.frames[i].redrew) ASSERT(run.frames[i].crc == run.frames[i - 1].crc, "no redraw, same layer");

    // The same code on the same pack is the same run.
    const Run again = runCorpus(bytes);
    bool same = again.frames.size() == run.frames.size();
    for (size_t i = 0; same && i < run.frames.size(); ++i)
        same = again.frames[i].crc == run.frames[i].crc && again.frames[i].redrew == run.frames[i].redrew;
    ASSERT(same, "a second run gives the same CRCs");
}

static void testRefusals() {
    ShapeCorpus corpus;
    static ShapeCorpusLayer layer;
    ShapeCorpusFrame f;
    ASSERT(!corpus.step(layer, scratch, f), "no step before begin()");

    // The Chestnut alone: its pieces are id 2, the fixed Look wants id 1.
    const std::string dir = std::string(ENJIN2_SOURCE_DIR) + "/tools/testdata/shapes/";
    const std::vector<uint8_t> ledger = readFile(dir + "ledger.json");
    const ShapeBundleResult r =
        bundleShapes({{"chestnut.svg", readFile(dir + "chestnut.svg"), readFile(dir + "chestnut.svg.shape.json")}},
                     std::string(reinterpret_cast<const char*>(ledger.data()), ledger.size()));
    ASSERT(r.ok(), "the Chestnut bundles");
    if (!r.ok()) return;
    ShapePackMemory src(r.pack.data(), r.pack.size());
    ShapePack pack;
    ShapeError e;
    ASSERT(pack.open(src, &e), "and opens");
    AssetArena arena;
    arena.allocateBacking(64 * 1024);
    ASSERT(!corpus.begin(pack, arena, &e), "a pack without the Look's pieces is refused");
    ASSERT(!corpus.step(layer, scratch, f), "and does not run");
}

int main() {
    testCrc();
    testFrameLine();
    testScript();
    testRefusals();
    printf("shape_corpus_test: %d passed, %d failed\n", passes, failures);
    return failures ? 1 : 0;
}
