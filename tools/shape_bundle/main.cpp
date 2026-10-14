/**
 * @file main.cpp
 * @brief enjin_shape_bundle — Kit SVGs → one Shape pack (Tomodachi #330, ADR-0017)
 *
 *   enjin_shape_bundle <source.svg>... --ledger <ledger.json> -o <pack.njn>
 *
 * Runs enjin2::bundleShapes(), the function the Studio's asset-tools worker
 * will call.  Each source's shape sidecar is `<source>.shape.json` next to it
 * (optional).  Sources are keyed in the ledger by their path relative to the
 * ledger's folder.  A missing ledger starts a new one; the updated ledger is
 * written back only when it changed.  Issues go to stderr; on any error
 * nothing is written.  Exit status: 0 ok, 1 refused, 2 usage or I/O error.
 */
#include <enjin2/graphics/shape_pack.hpp>
#include <enjin2/import/shape_bundle.hpp>

#include <cstdio>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <string>
#include <vector>

using namespace enjin2;
namespace fs = std::filesystem;

namespace {

void usage() {
    std::fprintf(stderr,
                 "usage: enjin_shape_bundle <source.svg>... --ledger <ledger.json> -o <pack.njn>\n"
                 "  Each source's shape sidecar is <source>.shape.json (optional).\n"
                 "  The ledger is created when missing and updated in place.\n");
}

bool readAll(const fs::path& path, std::vector<uint8_t>& out) {
    std::ifstream in(path, std::ios::binary);
    if (!in) return false;
    out.assign(std::istreambuf_iterator<char>(in), std::istreambuf_iterator<char>());
    return !in.bad();
}

bool writeAll(const fs::path& path, const void* data, size_t size) {
    std::ofstream out(path, std::ios::binary | std::ios::trunc);
    out.write(static_cast<const char*>(data), static_cast<std::streamsize>(size));
    return static_cast<bool>(out);
}

}  // namespace

int main(int argc, char** argv) {
    std::vector<std::string> inputs;
    std::string ledgerPath, output;
    for (int i = 1; i < argc; ++i) {
        const std::string a = argv[i];
        if ((a == "--ledger" || a == "-o") && i + 1 < argc) {
            (a == "-o" ? output : ledgerPath) = argv[++i];
        } else if (a == "-h" || a == "--help") {
            usage();
            return 0;
        } else if (!a.empty() && a[0] == '-') {
            std::fprintf(stderr, "error: unknown option %s\n", a.c_str());
            usage();
            return 2;
        } else {
            inputs.push_back(a);
        }
    }
    if (inputs.empty() || ledgerPath.empty() || output.empty()) {
        usage();
        return 2;
    }

    std::error_code ec;
    const fs::path ledgerFile = fs::absolute(ledgerPath, ec);
    const fs::path ledgerDir = fs::weakly_canonical(ledgerFile.parent_path(), ec);
    std::vector<ShapeSource> sources;
    for (const auto& in : inputs) {
        ShapeSource s;
        if (!readAll(in, s.svg)) {
            std::fprintf(stderr, "error: cannot read %s\n", in.c_str());
            return 2;
        }
        const fs::path sidecar = in + ".shape.json";
        if (fs::exists(sidecar, ec) && !readAll(sidecar, s.sidecar)) {
            std::fprintf(stderr, "error: cannot read %s\n", sidecar.string().c_str());
            return 2;
        }
        const fs::path full = fs::weakly_canonical(fs::absolute(in, ec), ec);
        fs::path rel = full.lexically_relative(ledgerDir);
        s.name = rel.empty() ? full.generic_string() : rel.generic_string();
        sources.push_back(std::move(s));
    }

    std::string ledger;
    if (fs::exists(ledgerFile, ec)) {
        std::vector<uint8_t> bytes;
        if (!readAll(ledgerFile, bytes)) {
            std::fprintf(stderr, "error: cannot read %s\n", ledgerPath.c_str());
            return 2;
        }
        ledger.assign(bytes.begin(), bytes.end());
    }

    const ShapeBundleResult r = bundleShapes(sources, ledger);
    size_t errors = 0;
    for (const auto& i : r.issues) {
        if (i.severity == ImportIssueSeverity::Error) ++errors;
        std::string where = i.source;
        if (!i.layer.empty()) where += (where.empty() ? "" : ": ") + i.layer;
        std::fprintf(stderr, "%s %s: %s%s%s%s\n", importIssueSeverityName(i.severity), i.code.c_str(), i.message.c_str(),
                     where.empty() ? "" : " (", where.c_str(), where.empty() ? "" : ")");
    }
    if (!r.ok()) {
        std::fprintf(stderr, "error: Shape pack not written (%zu error%s)\n", errors, errors == 1 ? "" : "s");
        return 1;
    }
    if (!writeAll(output, r.pack.data(), r.pack.size())) {
        std::fprintf(stderr, "error: cannot write %s\n", output.c_str());
        return 2;
    }
    if (r.ledger != ledger && !writeAll(ledgerFile, r.ledger.data(), r.ledger.size())) {
        std::fprintf(stderr, "error: cannot write %s\n", ledgerPath.c_str());
        return 2;
    }

    ShapePackMemory mem(r.pack.data(), r.pack.size());
    ShapePackIndex index;
    index.open(mem);
    std::printf("Shape pack: %s  (%zu bytes, %zu pieces)%s\n", output.c_str(), r.pack.size(), index.pieces().size(),
                r.ledger != ledger ? "  ledger updated" : "");
    for (const auto& e : index.pieces())
        std::printf("  %-6s %3u  %-16s %6u bytes\n", shapeSlotName(e.slot), e.id, e.label.c_str(), e.size);
    return 0;
}
