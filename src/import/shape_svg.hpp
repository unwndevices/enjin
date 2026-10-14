/**
 * @file shape_svg.hpp
 * @brief Kit SVG → layer tree of flattened Shapes (Tomodachi #330)
 *
 * Internal to enjin2_import.  A hand-rolled XML reader (entities, comments,
 * CDATA, PIs, DOCTYPE skipped, depth- and element-capped) feeds the SVG
 * subset: `g`, `path`, `circle`, `ellipse`, `rect`, `polygon`, `polyline`,
 * `line`; `transform` baked; fill / fill-rule from presentation attributes,
 * `.class` rules in `<style>` and `style=""` (in that precedence order).
 * Every curve is flattened in Kit coordinates (origin at the artboard centre)
 * so the tolerance holds after transforms.
 */
#pragma once

#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

#include <enjin2/import/shape_bundle.hpp>

namespace enjin2::shape_svg {

/// Max distance from a flattened chord to its curve, in logical px (0.125 px at 2×).
static constexpr double kFlattenTolerance = 0.0625;

struct Pt { double x = 0, y = 0; };

struct Ring {
    std::vector<Pt> pts;
    bool closed = false;
};

/// What the bundle needs of a Shape's computed style.
struct Paint {
    enum class Fill : uint8_t { None, Colour, Unsupported } fill = Fill::Colour;
    uint32_t rgb = 0;            ///< 0xRRGGBB when fill == Colour (SVG's default fill is black).
    bool     evenOdd = false;
    bool     stroke = false;     ///< A stroke other than `none` (Tomodachi #332).
    bool     opacity = false;    ///< opacity / fill-opacity below 1 (Tomodachi #332).
    bool     clip = false;       ///< clip-path on it or an ancestor (Tomodachi #332).
};

/// One node of the layer tree: a group or a drawable.
struct Node {
    bool              group = false;
    std::string       name;     ///< Decoded layer name, "" when unnamed.
    std::string       element;  ///< The SVG element name.
    std::vector<Node> children; ///< Groups only.
    std::vector<Ring> rings;    ///< Drawables only, Kit coordinates.
    Paint             paint;
};

/// Read a Kit.  Appends issues (with @p source) and returns false on an error.
/// @p root is a group holding the `<svg>` element's drawable descendants.
bool readSvg(const uint8_t* data, size_t size, const std::string& source, const ShapeBundleLimits& limits,
             Node& root, std::vector<ImportIssue>& issues);

/// Decode an Illustrator layer name: `_xHH_` escapes, legacy `_N_` suffix.
std::string decodeLayerName(const std::string& raw);

}  // namespace enjin2::shape_svg
