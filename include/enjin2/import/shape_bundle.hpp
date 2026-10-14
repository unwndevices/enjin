/**
 * @file shape_bundle.hpp
 * @brief Shape bundle: Kit SVGs + shape sidecars + id ledger → one Shape pack (ADR-0017, Tomodachi #330)
 *
 * bundleShapes() turns every pet source into the one Shape pack an applet
 * ships (`graphics/shape_pack.hpp`).  The native CLI (`tools/shape_bundle`)
 * and, later, the Studio's asset-tools worker call the same function, so it
 * does no I/O: sources, sidecars and the ledger come in as bytes; the pack,
 * the updated ledger and the issues come out.
 *
 * ## Source (a Kit: one 80×80 artboard, Illustrator 29.x "Save a Copy › SVG")
 *
 * The `viewBox` must be 80×80; its centre is the Kit origin (1 unit = 1 px).
 * Top-level layers named after a slot (`body eyes cheeks mouth top arms legs`)
 * hold the pieces; other top-level layers are ignored with a lint.  Names come
 * from `data-name`, else `id` (`_xHH_` escapes decoded, a legacy `_N_`
 * duplicate suffix stripped).
 *
 * A Feature's slot layer holds pose sublayers named from the slot's fixed list
 * (shapePoseName()); with none, its contents are the required pose.  Inside a
 * pose, each named child (group or shape) is a **member**; unnamed shapes form
 * one implicit member.  An object named `anchor` is the anchor of the member
 * it sits in (its bbox centre; without one, the pose-0 anchor of the same
 * member, else the member's bbox centre) and is never drawn.
 *
 * - Each pose-0 member puts a **socket** on the Kit's Body at its anchor, side
 *   by the anchor's x (|x| < 0.5 px is Centre).  A slot holding only anchors
 *   declares sockets without a Feature.
 * - A member named `…_l` / `…_r` is a Left / Right member.  Any other member is
 *   Mirrored: stored right-facing (a member drawn left of centre is flipped)
 *   and mirrored back on left sockets.  When every Mirrored member is the same
 *   drawing, one is kept.
 * - A Feature whose slot layer comes before `body` is drawn behind it.
 *
 * Rings are flattened at 0.0625 px (0.125 px at 2×) and stored Q8.8: a Feature
 * member's relative to its anchor, the Body's relative to the Kit origin.
 *
 * ## Shape sidecar (`<source>.shape.json`, optional)
 *
 * ```json
 * { "version": 1,
 *   "roles": { "#b18cf9": "body", "#1c1c1c": "ink" },
 *   "features": { "top": { "binding": "field" }, "arms": { "sway": { "hz": 3, "zeta": 0.3 } } } }
 * ```
 * Each fill colour's role comes from `roles` (an unmapped colour is a lint
 * and paints as `body`).  `features` overrides a slot layer's Binding and sway;
 * defaults: eyes rigid, mouth and cheeks field, top/arms/legs sway 2.4 Hz ζ 0.27.
 *
 * ## Id ledger (JSON, empty = new)
 *
 * ```json
 * { "version": 1, "next": { "body": 2, "eyes": 3 },
 *   "pieces": [ { "source": "star.svg", "slot": "eyes", "layer": "eyes", "id": 1 } ],
 *   "retired": [ { "source": "old.svg", "slot": "eyes", "layer": "eyes", "id": 2 } ] }
 * ```
 * A piece is keyed by source name + slot + layer name.  A new piece takes its
 * slot's next id (from 1); a piece no source has any more is retired, and a
 * retired id is never handed out again.  The output is sorted, so the same
 * inputs give the same bytes.
 *
 * ## Issues
 *
 * Lints never stop the bundle.  Any error does (outside ShapeBundleOptions::preview):
 * the result then has no pack and returns the input ledger unchanged.  Errors: wrong artboard, unparsable
 * SVG / sidecar / ledger, forbidden element (`text image linearGradient
 * radialGradient pattern mask filter symbol foreignObject`, `use` outside a
 * clip path), a Body without `eyes` and `mouth` sockets, a slot layer twice in
 * one source, a Feature without its required pose, and size caps.
 *
 * ## Paint (Tomodachi #332)
 *
 * - **Dither:** `opacity` × `fill-opacity` snaps to 25 / 50 / 75 / 100 % (the
 *   record's `dither`, in quarters); an off-level value is the
 *   `off-level-opacity` lint.  `opacity` on a group is the `group-opacity` error.
 * - **Stroke:** a `stroke` colour gets a role like a fill (stroke colours are
 *   source colours too); `stroke-width` (default 1, scaled by the transform)
 *   rounds to whole px (`fractional-stroke` lint when it isn't one; 0 strokes
 *   nothing).  On closed rings it is the inner stroke width, on open ones the
 *   line width.
 * - **Holes:** a compound path is one Shape of several rings, under its
 *   fill-rule (even-odd, or non-zero with the hole wound the other way).
 * - **Clip:** `clip-path: url(#id)` on a shape or group names a `<clipPath>`
 *   (its shapes, or the legacy `<use href>` of one; geometry in the clipped
 *   element's space).  A clip within 0.25 px of the Kit's Body (both ways,
 *   outline to outline) binds to the **live Body** (`ShapeClip::LiveBody`);
 *   otherwise it becomes a Shape of role `none` after every span, relative to
 *   the member's anchor (`ShapeClip::Own` + `clipShape`), and within 2 px of the
 *   Body it is the `near-body-clip` lint.  A clip-path naming nothing is the
 *   `missing-clip` lint; a clip inside a clip keeps the inner one (`nested-clip`).
 *   Shapes inside `<clipPath>` are never drawn.
 */
#pragma once

#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

namespace enjin2 {

/// One source file and its shape sidecar (empty bytes: no sidecar).
struct ShapeSource {
    std::string          name;  ///< Ledger key (the CLI: path relative to the ledger).
    std::vector<uint8_t> svg;
    std::vector<uint8_t> sidecar;
};

enum class ImportIssueSeverity : uint8_t { Lint, Error };

/// One problem found while importing, pointing at the source layer it is about.
struct ImportIssue {
    ImportIssueSeverity severity = ImportIssueSeverity::Lint;
    std::string code;     ///< Stable kebab-case id, e.g. `forbidden-element`.
    std::string message;  ///< One human sentence.
    std::string source;   ///< ShapeSource::name ("" for the ledger).
    std::string layer;    ///< Layer path, e.g. `eyes/open/eye_l` ("" for the file).
};

/// Size caps, checked before the work they bound.
struct ShapeBundleLimits {
    size_t   maxSourceBytes  = 4u << 20;  ///< Per SVG.
    size_t   maxJsonBytes    = 1u << 20;  ///< Per sidecar and the ledger.
    uint32_t maxSources      = 256;
    uint32_t maxXmlDepth     = 64;
    uint32_t maxElements     = 20000;      ///< Per SVG.
    uint32_t maxPathCommands = 50000;      ///< Per SVG.
    uint32_t maxPiecePoints  = 32768;      ///< Per Body or Feature.
};

struct ShapeBundleOptions {
    ShapeBundleLimits limits;
    /// The Studio's preview (Tomodachi #340): an error drops only what it is about instead of
    /// the whole bundle.  A piece-level error drops its piece (a Body that loses a socket with
    /// it goes too); a file-level one (unparsable SVG or sidecar, wrong artboard, a slot layer
    /// twice) drops its source; a bad ledger is read as empty; a source listed twice is kept
    /// once.  The rest still packs and gets ids.  The issues are the same, so ok() stays false.
    bool preview = false;
};

/// The fill and stroke colours one source draws with: distinct 0xRRGGBB, in first-use order
/// (Features in layer order, then the Body), whether the shape sidecar maps them or not.
struct ShapeSourceColours {
    std::string           source;
    std::vector<uint32_t> rgb;
};

struct ShapeBundleResult {
    std::vector<uint8_t>     pack;    ///< Empty on an error (in preview: only when nothing packs).
    std::string              ledger;  ///< The updated ledger (the input one when there is no pack).
    std::vector<ImportIssue> issues;
    /// One entry per source read, in source order: what the Studio offers roles for.
    std::vector<ShapeSourceColours> colours;

    bool ok() const {
        for (const auto& i : issues)
            if (i.severity == ImportIssueSeverity::Error) return false;
        return !pack.empty();
    }
};

ShapeBundleResult bundleShapes(const std::vector<ShapeSource>& sources, const std::string& ledgerJson,
                               const ShapeBundleOptions& opts = {});

/// `lint` / `error`.
const char* importIssueSeverityName(ImportIssueSeverity s);

}  // namespace enjin2
