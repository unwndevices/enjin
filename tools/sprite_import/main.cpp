/**
 * @file main.cpp
 * @brief enjin_sprite_import — `.aseprite` → `.njn` v2 command line (Tomodachi #291)
 *
 *   enjin_sprite_import <input.aseprite> -o <out.njn> [--layered|--sheet] [--palette NAME]
 *   enjin_sprite_import <input.aseprite> --reimport <existing.njn> [-o <out.njn>] [--palette NAME]
 *
 * Runs enjin2::importSprite(), the function the Studio's asset-tools worker
 * calls.  --palette picks the RGBA target palette by preset name (default: the
 * system palette).  --reimport runs enjin2::reimportSprite() (Tomodachi #297)
 * over an existing `.njn` instead, writing over it unless -o says otherwise.
 * The CLI has no sprite sidecar, so the follow set is empty: every clip counts
 * as edited and keeps its clip frames.  Exit status: 0 ok, 1 import failed,
 * 2 usage or I/O error.
 */
#include <enjin2/import/sprite_import.hpp>
#include <enjin2/import/sprite_reimport.hpp>

#include <algorithm>
#include <charconv>
#include <cstdio>
#include <cstring>
#include <fstream>
#include <iterator>
#include <string>
#include <vector>

using namespace enjin2;

namespace {

void usage() {
    std::fprintf(stderr,
                  "usage: enjin_sprite_import <input.aseprite|input.png> -o <out.njn> [--layered|--sheet] [--palette NAME] [--cell WxH]\n"
                  "       enjin_sprite_import <input.aseprite|input.png> --reimport <existing.njn> [-o <out.njn>] [--palette NAME] [--cell WxH]\n"
                 "  --layered / --sheet  override the kind (default: layered when >1 layer is visible)\n"
                 "  --reimport FILE      replace FILE's pixels and frames, keeping its clips by name\n"
                 "                       (written over FILE unless -o is given; the kind is FILE's)\n"
                  "  --palette NAME       RGBA target palette preset (default: default, the system palette)\n"
                  "  --cell WxH           PNG cell size (default: whole image)\n");
}

std::vector<uint8_t> readAll(const std::string& path, bool& ok) {
    std::ifstream in(path, std::ios::binary);
    ok = static_cast<bool>(in);
    return std::vector<uint8_t>((std::istreambuf_iterator<char>(in)), std::istreambuf_iterator<char>());
}

bool writeAll(const std::string& path, const std::vector<uint8_t>& bytes) {
    std::ofstream out(path, std::ios::binary);
    out.write(reinterpret_cast<const char*>(bytes.data()), static_cast<std::streamsize>(bytes.size()));
    return static_cast<bool>(out);
}

void printColourIssues(const std::vector<SpriteColourIssue>& issues) {
    for (const auto& c : issues) {
        std::fprintf(stderr, "  #%02X%02X%02X alpha %3u  %7u px  nearest slot %u%s\n", c.r, c.g, c.b, c.a,
                     c.pixels, c.nearestSlot, c.partialAlpha() ? "  (partial alpha)" : "");
    }
}

std::string numbers(const std::vector<uint16_t>& v) {
    std::string s;
    for (uint16_t n : v) s += (s.empty() ? "" : " ") + std::to_string(n);
    return "[" + s + "]";
}

/// --reimport: merge into @p existing, print the report.
int reimport(const std::string& existingPath, const std::vector<uint8_t>& source, const std::string& output,
             const SpriteImportOptions& io) {
    bool ok = false;
    const std::vector<uint8_t> existing = readAll(existingPath, ok);
    if (!ok) {
        std::fprintf(stderr, "error: cannot read %s\n", existingPath.c_str());
        return 2;
    }
    SpriteReimportOptions opts;
    opts.palette = io.palette;
    opts.limits = io.limits;
    opts.cellW = io.cellW;
    opts.cellH = io.cellH;
    const SpriteReimportResult r = reimportSprite(existing.data(), existing.size(), source.data(), source.size(), {}, opts);
    if (!r.ok()) {
        std::fprintf(stderr, "error (%s): %s\n", spriteImportStatusName(r.status), r.error.c_str());
        printColourIssues(r.colourIssues);
        return 1;
    }
    if (!writeAll(output, r.njn)) {
        std::fprintf(stderr, "error: cannot write %s\n", output.c_str());
        return 2;
    }
    std::printf("Re-imported: %s  (%zu bytes, .njn v2 %s)\n", output.c_str(), r.njn.size(),
                r.kind == SpriteKind::Layered ? "layered" : "sheet");
    std::printf("  frames : %u -> %u%s\n", r.framesBefore, r.framesAfter, r.touchesNoClip ? "  (touches no clip)" : "");
    for (const auto& c : r.clips) {
        std::string flags;
        if (c.added) flags += " added";
        if (c.tagRemoved) flags += " tag-removed";
        if (c.emptied) flags += " emptied";
        if (!c.clashTag.empty()) flags += " tag '" + c.clashTag + "' not taken over";
        if (!c.removed.empty()) flags += " removed " + std::to_string(c.removed.size()) + " clip frame(s)";
        std::printf("  clip   : %s %s -> %s%s\n", c.name.c_str(), numbers(c.before).c_str(), numbers(c.after).c_str(),
                    flags.c_str());
    }
    return 0;
}

const char* modeName(NjnLoopMode m) {
    const auto i = static_cast<size_t>(m);
    return i < std::size(kLoopModeNames) ? kLoopModeNames[i] : "?";
}

} // namespace

int main(int argc, char** argv) {
    std::string input, output, existing, palette = "default";
    bool kindGiven = false;
    SpriteImportOptions opts;
    for (int i = 1; i < argc; ++i) {
        const std::string a = argv[i];
        if (a == "-o" && i + 1 < argc) {
            output = argv[++i];
        } else if (a == "--layered") {
            opts.kind = SpriteKind::Layered;
            kindGiven = true;
        } else if (a == "--sheet") {
            opts.kind = SpriteKind::Sheet;
            kindGiven = true;
        } else if (a == "--reimport" && i + 1 < argc) {
            existing = argv[++i];
        } else if (a == "--palette" && i + 1 < argc) {
            palette = argv[++i];
        } else if (a == "--cell" && i + 1 < argc) {
            const std::string cell = argv[++i];
            const size_t split = cell.find('x');
            unsigned w = 0, h = 0;
            if (split == std::string::npos) { usage(); return 2; }
            const auto rw = std::from_chars(cell.data(), cell.data() + split, w);
            const auto rh = std::from_chars(cell.data() + split + 1, cell.data() + cell.size(), h);
            if (rw.ec != std::errc{} || rh.ec != std::errc{} || rw.ptr != cell.data() + split ||
                rh.ptr != cell.data() + cell.size() || !w || !h || w > 255 || h > 255) { usage(); return 2; }
            opts.cellW = uint16_t(w); opts.cellH = uint16_t(h);
        } else if (a == "-h" || a == "--help") {
            usage();
            return 0;
        } else if (input.empty() && !a.empty() && a[0] != '-') {
            input = a;
        } else {
            usage();
            return 2;
        }
    }
    if (!existing.empty() && output.empty()) output = existing;
    if (input.empty() || output.empty() || (!existing.empty() && kindGiven)) {
        usage();
        return 2;
    }
    Palette p;
    if (!p.loadPreset(palette.c_str()) || p.getSize() != PALETTE_MAX_ENTRIES) {
        std::fprintf(stderr, "error: unknown or short palette preset '%s'\n", palette.c_str());
        return 2;
    }
    std::copy(std::begin(p.colors), std::end(p.colors), opts.palette.begin());

    bool readOk = false;
    const std::vector<uint8_t> bytes = readAll(input, readOk);
    if (!readOk) {
        std::fprintf(stderr, "error: cannot read %s\n", input.c_str());
        return 2;
    }
    if (!existing.empty()) return reimport(existing, bytes, output, opts);

    const SpriteImportResult r = importSprite(bytes.data(), bytes.size(), opts);
    if (!r.ok()) {
        std::fprintf(stderr, "error (%s): %s\n", spriteImportStatusName(r.status), r.error.c_str());
        printColourIssues(r.colourIssues);
        return 1;
    }

    if (!writeAll(output, r.njn)) {
        std::fprintf(stderr, "error: cannot write %s\n", output.c_str());
        return 2;
    }
    std::printf("Written: %s  (%zu bytes, .njn v2 %s)\n", output.c_str(), r.njn.size(),
                r.kind == SpriteKind::Layered ? "layered" : "sheet");
    std::printf("  canvas : %ux%u, %u frames\n", r.canvasW, r.canvasH, r.frames);
    if (r.kind == SpriteKind::Layered) {
        std::string parts;
        for (const auto& n : r.parts) parts += (parts.empty() ? "" : ", ") + n;
        std::printf("  parts  : %s (%u images)\n", parts.c_str(), r.images);
    }
    for (const auto& c : r.clips) std::printf("  clip   : %s (%u frames, %s)\n", c.name.c_str(), c.frames, modeName(c.loopMode));
    for (const auto& h : r.hiddenLayers) std::printf("  hidden : %s (ignored)\n", h.c_str());
    return 0;
}
