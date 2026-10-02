/**
 * @file main.cpp
 * @brief enjin_sprite_import — `.aseprite` → `.njn` v2 command line (Tomodachi #291)
 *
 *   enjin_sprite_import <input.aseprite> -o <out.njn> [--layered|--sheet] [--palette NAME]
 *
 * Runs enjin2::importSprite(), the function the Studio's asset-tools worker
 * calls.  --palette picks the RGBA target palette by preset name (default: the
 * system palette).  Exit status: 0 ok, 1 import failed, 2 usage or I/O error.
 */
#include <enjin2/import/sprite_import.hpp>

#include <algorithm>
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
                 "usage: enjin_sprite_import <input.aseprite> -o <out.njn> [--layered|--sheet] [--palette NAME]\n"
                 "  --layered / --sheet  override the kind (default: layered when >1 layer is visible)\n"
                 "  --palette NAME       RGBA target palette preset (default: default, the system palette)\n");
}

const char* modeName(NjnLoopMode m) {
    const auto i = static_cast<size_t>(m);
    return i < std::size(kLoopModeNames) ? kLoopModeNames[i] : "?";
}

} // namespace

int main(int argc, char** argv) {
    std::string input, output, palette = "default";
    SpriteImportOptions opts;
    for (int i = 1; i < argc; ++i) {
        const std::string a = argv[i];
        if (a == "-o" && i + 1 < argc) {
            output = argv[++i];
        } else if (a == "--layered") {
            opts.kind = SpriteKind::Layered;
        } else if (a == "--sheet") {
            opts.kind = SpriteKind::Sheet;
        } else if (a == "--palette" && i + 1 < argc) {
            palette = argv[++i];
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
    if (input.empty() || output.empty()) {
        usage();
        return 2;
    }
    Palette p;
    if (!p.loadPreset(palette.c_str()) || p.getSize() != PALETTE_MAX_ENTRIES) {
        std::fprintf(stderr, "error: unknown or short palette preset '%s'\n", palette.c_str());
        return 2;
    }
    std::copy(std::begin(p.colors), std::end(p.colors), opts.palette.begin());

    std::ifstream in(input, std::ios::binary);
    if (!in) {
        std::fprintf(stderr, "error: cannot read %s\n", input.c_str());
        return 2;
    }
    const std::vector<uint8_t> bytes((std::istreambuf_iterator<char>(in)), std::istreambuf_iterator<char>());

    const SpriteImportResult r = importSprite(bytes.data(), bytes.size(), opts);
    if (!r.ok()) {
        std::fprintf(stderr, "error (%s): %s\n", spriteImportStatusName(r.status), r.error.c_str());
        for (const auto& c : r.colourIssues) {
            std::fprintf(stderr, "  #%02X%02X%02X alpha %3u  %7u px  nearest slot %u%s\n", c.r, c.g, c.b, c.a,
                         c.pixels, c.nearestSlot, c.partialAlpha() ? "  (partial alpha)" : "");
        }
        return 1;
    }

    std::ofstream out(output, std::ios::binary);
    out.write(reinterpret_cast<const char*>(r.njn.data()), static_cast<std::streamsize>(r.njn.size()));
    if (!out) {
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
