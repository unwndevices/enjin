/**
 * @file shape_bundle.cpp
 * @brief Kit SVGs → one Shape pack: pull-apart, shape sidecar, id ledger (Tomodachi #330)
 */
#include <enjin2/import/shape_bundle.hpp>

#include <enjin2/graphics/shape_pack.hpp>

#include <algorithm>
#include <cctype>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <map>
#include <tuple>

#include "shape_json.hpp"
#include "shape_svg.hpp"

namespace enjin2 {

const char* importIssueSeverityName(ImportIssueSeverity s) {
    return s == ImportIssueSeverity::Error ? "error" : "lint";
}

namespace {

using shape_svg::Node;
using shape_svg::Pt;
using shape_svg::Ring;
using json = shape_json::Value;

constexpr double kCentreBand = 0.5;  ///< |x| below this is on the centre line.
constexpr int kSameDrawing = 2;
constexpr double kDefaultSwayHz = 2.4, kDefaultSwayZeta = 0.27;      ///< Q8.8 units two members' points may differ by and still be one drawing.

// ---------------------------------------------------------------------------
// Issues
// ---------------------------------------------------------------------------

struct Sink {
    std::vector<ImportIssue>& out;
    bool errors = false;

    void add(ImportIssueSeverity sev, const char* code, const std::string& msg, const std::string& source,
             const std::string& layer) {
        if (sev == ImportIssueSeverity::Error) errors = true;
        out.push_back({sev, code, msg, source, layer});
    }
    void error(const char* code, const std::string& msg, const std::string& source, const std::string& layer = "") {
        add(ImportIssueSeverity::Error, code, msg, source, layer);
    }
    void lint(const char* code, const std::string& msg, const std::string& source, const std::string& layer = "") {
        add(ImportIssueSeverity::Lint, code, msg, source, layer);
    }
};

std::string join(const std::string& a, const std::string& b) { return a.empty() ? b : b.empty() ? a : a + "/" + b; }

bool slotByName(const std::string& n, ShapeSlot& out) {
    for (uint8_t i = 0; i < SHAPE_SLOT_COUNT; ++i)
        if (n == shapeSlotName(static_cast<ShapeSlot>(i))) {
            out = static_cast<ShapeSlot>(i);
            return true;
        }
    return false;
}

int poseByName(ShapeSlot slot, const std::string& n) {
    for (uint8_t i = 0; i < shapePoseCount(slot); ++i)
        if (n == shapePoseName(slot, i)) return i;
    return -1;
}

/// `[a-z][a-z0-9-]*`, optionally ending `_l` / `_r`.
bool goodName(const std::string& n) {
    std::string base = n;
    if (base.size() > 2 && base[base.size() - 2] == '_' && (base.back() == 'l' || base.back() == 'r'))
        base.resize(base.size() - 2);
    if (base.empty() || base[0] < 'a' || base[0] > 'z') return false;
    for (char c : base)
        if (!((c >= 'a' && c <= 'z') || (c >= '0' && c <= '9') || c == '-')) return false;
    return true;
}

// ---------------------------------------------------------------------------
// Shape sidecar
// ---------------------------------------------------------------------------

struct FeatureOverride {
    bool         hasBinding = false;
    ShapeBinding binding = ShapeBinding::Field;
    bool         hasSway = false;
    double       hz = 0, zeta = 0;
};

struct Sidecar {
    std::map<uint32_t, ShapeRole>          roles;
    std::map<std::string, FeatureOverride> features;
};

bool parseSidecar(const std::vector<uint8_t>& bytes, const std::string& source, const ShapeBundleLimits& lim,
                  Sidecar& out, Sink& sink) {
    if (bytes.empty()) return true;
    auto bad = [&](const std::string& m) {
        sink.error("bad-sidecar", "shape sidecar: " + m, source);
        return false;
    };
    if (bytes.size() > lim.maxJsonBytes) return bad("larger than the size cap");
    json doc;
    std::string err;
    if (!shape_json::parse(reinterpret_cast<const char*>(bytes.data()), bytes.size(), doc, err)) return bad(err);
    if (!doc.isObject()) return bad("not a JSON object");
    if (const json* v = doc.get("version"); v && (!v->isNumber() || v->n != 1)) return bad("unsupported version");
    if (const json* roles = doc.get("roles")) {
        if (!roles->isObject()) return bad("\"roles\" is not an object");
        for (const auto& [hex, role] : roles->members) {
            uint32_t rgb = 0;
            bool ok = hex.size() == 7 && hex[0] == '#';
            for (size_t i = 1; ok && i < 7; ++i) {
                const char c = static_cast<char>(std::tolower(static_cast<unsigned char>(hex[i])));
                const int d = c >= '0' && c <= '9' ? c - '0' : c >= 'a' && c <= 'f' ? c - 'a' + 10 : -1;
                ok = d >= 0;
                rgb = (rgb << 4) | static_cast<uint32_t>(d < 0 ? 0 : d);
            }
            if (!ok) return bad("role key \"" + hex + "\" is not #rrggbb");
            static const char* const names[] = {"body", "accent", "ink", "none"};
            int r = -1;
            for (int i = 0; i < 4; ++i)
                if (role.isString() && role.s == names[i]) r = i;
            if (r < 0) return bad("role of " + hex + " is not body, accent, ink or none");
            out.roles[rgb] = static_cast<ShapeRole>(r);
        }
    }
    if (const json* feats = doc.get("features")) {
        if (!feats->isObject()) return bad("\"features\" is not an object");
        for (const auto& [layer, f] : feats->members) {
            if (!f.isObject()) return bad("feature \"" + layer + "\" is not an object");
            FeatureOverride o;
            if (const json* b = f.get("binding")) {
                static const char* const names[] = {"field", "rigid", "sway"};
                int k = -1;
                for (int i = 0; i < 3; ++i)
                    if (b->isString() && b->s == names[i]) k = i;
                if (k < 0) return bad("binding of \"" + layer + "\" is not field, rigid or sway");
                o.hasBinding = true;
                o.binding = static_cast<ShapeBinding>(k);
            }
            if (const json* s = f.get("sway")) {
                const json* hz = s->get("hz");
                const json* zeta = s->get("zeta");
                if (!s->isObject() || !hz || !zeta || !hz->isNumber() || !zeta->isNumber() || hz->n <= 0 ||
                    hz->n >= 16 || zeta->n < 0 || zeta->n >= 16)
                    return bad("sway of \"" + layer + "\" needs hz and zeta in [0, 16)");
                o.hasSway = true;
                o.hz = hz->n;
                o.zeta = zeta->n;
            }
            out.features[layer] = o;
        }
    }
    return true;
}

// ---------------------------------------------------------------------------
// Id ledger
// ---------------------------------------------------------------------------

struct LedgerEntry {
    std::string source, layer;
    ShapeSlot   slot = ShapeSlot::Body;
    uint16_t    id = 0;
    auto key() const { return std::make_tuple(source, static_cast<uint8_t>(slot), layer); }
};

struct Ledger {
    uint32_t next[SHAPE_SLOT_COUNT];
    std::vector<LedgerEntry> pieces, retired;
    Ledger() {
        for (auto& n : next) n = 1;
    }
};

bool parseLedger(const std::string& text, const ShapeBundleLimits& lim, Ledger& out, Sink& sink) {
    auto bad = [&](const std::string& m) {
        sink.error("bad-ledger", "id ledger: " + m, "");
        return false;
    };
    bool blank = true;
    for (char c : text) blank = blank && (c == ' ' || c == '\n' || c == '\r' || c == '\t');
    if (blank) return true;
    if (text.size() > lim.maxJsonBytes) return bad("larger than the size cap");
    json doc;
    std::string err;
    if (!shape_json::parse(text.data(), text.size(), doc, err)) return bad(err);
    if (!doc.isObject()) return bad("not a JSON object");
    const json* v = doc.get("version");
    if (!v || !v->isNumber() || v->n != 1) return bad("unsupported version");
    if (const json* next = doc.get("next")) {
        if (!next->isObject()) return bad("\"next\" is not an object");
        for (const auto& [name, n] : next->members) {
            ShapeSlot s;
            if (!slotByName(name, s)) return bad("unknown slot \"" + name + "\" in \"next\"");
            if (!n.isNumber() || n.n < 1 || n.n > 65536 || n.n != std::floor(n.n)) return bad("bad next id for " + name);
            out.next[static_cast<uint8_t>(s)] = static_cast<uint32_t>(n.n);
        }
    }
    auto entries = [&](const char* key, std::vector<LedgerEntry>& list) {
        const json* arr = doc.get(key);
        if (!arr) return true;
        if (!arr->isArray()) return bad(std::string("\"") + key + "\" is not an array");
        for (const json& e : arr->items) {
            const json* src = e.get("source");
            const json* slot = e.get("slot");
            const json* layer = e.get("layer");
            const json* id = e.get("id");
            LedgerEntry le;
            if (!src || !src->isString() || !slot || !slot->isString() || !layer || !layer->isString() || !id ||
                !id->isNumber() || !slotByName(slot->s, le.slot) || id->n < 1 || id->n > 65535 || id->n != std::floor(id->n))
                return bad(std::string("bad entry in \"") + key + "\"");
            le.source = src->s;
            le.layer = layer->s;
            le.id = static_cast<uint16_t>(id->n);
            list.push_back(std::move(le));
        }
        return true;
    };
    if (!entries("pieces", out.pieces) || !entries("retired", out.retired)) return false;
    // Ids are unique per slot across pieces and retired; keys unique among pieces.
    std::vector<std::pair<uint8_t, uint16_t>> ids;
    for (const auto* list : {&out.pieces, &out.retired})
        for (const auto& e : *list) ids.emplace_back(static_cast<uint8_t>(e.slot), e.id);
    std::sort(ids.begin(), ids.end());
    if (std::adjacent_find(ids.begin(), ids.end()) != ids.end()) return bad("an id is used twice");
    std::vector<std::tuple<std::string, uint8_t, std::string>> keys;
    for (const auto& e : out.pieces) keys.push_back(e.key());
    std::sort(keys.begin(), keys.end());
    if (std::adjacent_find(keys.begin(), keys.end()) != keys.end()) return bad("a piece is listed twice");
    return true;
}

std::string writeLedger(const Ledger& l) {
    auto entry = [](const LedgerEntry& e) {
        return "    { \"source\": " + shape_json::quote(e.source) + ", \"slot\": \"" + shapeSlotName(e.slot) +
               "\", \"layer\": " + shape_json::quote(e.layer) + ", \"id\": " + std::to_string(e.id) + " }";
    };
    std::string s = "{\n  \"version\": 1,\n  \"next\": {";
    for (uint8_t i = 0; i < SHAPE_SLOT_COUNT; ++i)
        s += std::string(i ? ", " : " ") + "\"" + shapeSlotName(static_cast<ShapeSlot>(i)) + "\": " + std::to_string(l.next[i]);
    s += " },\n  \"pieces\": [";
    for (size_t i = 0; i < l.pieces.size(); ++i) s += (i ? ",\n" : "\n") + entry(l.pieces[i]);
    s += l.pieces.empty() ? "],\n" : "\n  ],\n";
    s += "  \"retired\": [";
    for (size_t i = 0; i < l.retired.size(); ++i) s += (i ? ",\n" : "\n") + entry(l.retired[i]);
    s += l.retired.empty() ? "]\n}\n" : "\n  ]\n}\n";
    return s;
}

// ---------------------------------------------------------------------------
// Pull a Kit apart
// ---------------------------------------------------------------------------

struct Box {
    double x0 = 1e300, y0 = 1e300, x1 = -1e300, y1 = -1e300;
    bool empty() const { return x0 > x1; }
    void add(const Node& n) {
        for (const auto& r : n.rings)
            for (const auto& p : r.pts) {
                x0 = std::min(x0, p.x);
                y0 = std::min(y0, p.y);
                x1 = std::max(x1, p.x);
                y1 = std::max(y1, p.y);
            }
        for (const auto& k : n.children) add(k);
    }
    Pt centre() const { return {(x0 + x1) / 2, (y0 + y1) / 2}; }
};

/// One member as drawn in one pose.
struct MemberDraw {
    std::string              name;  ///< "" for the implicit member.
    std::vector<const Node*> shapes;
    bool                     hasAnchor = false;
    Pt                       anchor;
    std::string              path;
};

/// Drawables under @p n (anchors excluded), in document order.  @p anchor gets the first `anchor`.
void collectShapes(const Node& n, std::vector<const Node*>& shapes, const Node*& anchor) {
    for (const auto& k : n.children) {
        if (k.name == "anchor") {
            if (!anchor) anchor = &k;
            continue;
        }
        if (k.group) collectShapes(k, shapes, anchor);
        else shapes.push_back(&k);
    }
}

Pt centreOf(const Node& n) {
    Box b;
    b.add(n);
    return b.empty() ? Pt{} : b.centre();
}

/// The members of one pose layer.  Named children are members; unnamed groups are transparent.
void collectMembers(const Node& pose, const std::string& path, std::vector<MemberDraw>& members,
                    MemberDraw& implicit, std::vector<const Node*>& anchors, bool& implicitSeen, size_t& implicitAt) {
    for (const auto& k : pose.children) {
        if (k.name == "anchor") {
            anchors.push_back(&k);
            continue;
        }
        if (k.name.empty()) {
            if (k.group) {
                collectMembers(k, path, members, implicit, anchors, implicitSeen, implicitAt);
            } else {
                if (!implicitSeen) implicitAt = members.size();
                implicitSeen = true;
                implicit.shapes.push_back(&k);
            }
            continue;
        }
        MemberDraw m;
        m.name = k.name;
        m.path = join(path, k.name);
        const Node* anchor = nullptr;
        if (k.group) collectShapes(k, m.shapes, anchor);
        else m.shapes.push_back(&k);
        if (anchor) {
            m.hasAnchor = true;
            m.anchor = centreOf(*anchor);
        }
        members.push_back(std::move(m));
    }
}

/// Members of a pose, in document order, with the implicit member and stray anchors resolved.
std::vector<MemberDraw> poseMembers(const Node& pose, const std::string& path, const std::string& source, Sink& sink) {
    std::vector<MemberDraw> members;
    MemberDraw implicit;
    implicit.path = path;
    std::vector<const Node*> anchors;
    bool implicitSeen = false;
    size_t implicitAt = 0;
    collectMembers(pose, path, members, implicit, anchors, implicitSeen, implicitAt);
    if (implicitSeen) {
        if (!anchors.empty()) {
            implicit.hasAnchor = true;
            implicit.anchor = centreOf(*anchors[0]);
            if (anchors.size() > 1) sink.lint("stray-anchor", "more than one anchor for one member; the first is used", source, path);
        }
        members.insert(members.begin() + static_cast<std::ptrdiff_t>(implicitAt), std::move(implicit));
    } else if (!anchors.empty()) {
        if (members.empty()) {
            // Anchor-only: one socket per anchor.
            for (const Node* a : anchors) {
                MemberDraw m;
                m.hasAnchor = true;
                m.anchor = centreOf(*a);
                m.path = join(path, "anchor");
                members.push_back(std::move(m));
            }
        } else {
            sink.lint("stray-anchor", "an anchor outside any member is ignored", source, path);
        }
    }
    for (const auto& m : members)
        if (!m.name.empty() && !goodName(m.name))
            sink.lint("bad-name", "member name \"" + m.name + "\" is not [a-z][a-z0-9-]*", source, m.path);
    return members;
}

/// An own-space clip waiting for its Shape: appended after every span of the piece.
struct PendingClip {
    size_t                               shape;  ///< The clipped Shape.
    std::vector<std::vector<ShapePoint>> rings;  ///< The clip, relative to the member's anchor.
};

struct Context {
    const std::string&       source;
    const Sidecar&           sidecar;
    const ShapeBundleLimits& limits;
    Sink&                    sink;
    std::vector<uint32_t>&   colours;   ///< Every fill and stroke colour read, first use first.
    std::vector<uint32_t>    unmapped;  ///< Colours already linted.
    const std::vector<std::vector<Ring>>* clips = nullptr;  ///< The source's clip paths (Kit coordinates).
    std::vector<Ring>        bodyRings;  ///< The Kit's Body as drawn (Kit coordinates), for live-Body clips.
    bool                     inBody = false;  ///< Building the Body piece (its clips are its own).
    std::vector<PendingClip> pending;
    bool lintedRange = false;
};

/// Clip within this of the Body (px, both ways) binds to the live Body; within kNearBodyClip is a lint.
constexpr double kLiveBodyClip = 0.25, kNearBodyClip = 2.0;

int16_t q88(double v, Context& cx, const std::string& path) {
    const double s = std::round(v * 256);
    if (s > 32767 || s < -32768) {
        if (!cx.lintedRange) cx.sink.lint("out-of-range", "geometry more than 127 px from its anchor is clamped", cx.source, path);
        cx.lintedRange = true;
        return static_cast<int16_t>(s > 0 ? 32767 : -32768);
    }
    return static_cast<int16_t>(s);
}

ShapePoint qpt(Pt p, Context& cx, const std::string& path) { return {q88(p.x, cx, path), q88(p.y, cx, path)}; }

ShapeRole roleOf(shape_svg::Paint::Fill kind, uint32_t rgb, Context& cx, const std::string& path) {
    if (kind != shape_svg::Paint::Fill::Colour) return ShapeRole::None;
    if (std::find(cx.colours.begin(), cx.colours.end(), rgb) == cx.colours.end()) cx.colours.push_back(rgb);
    const auto it = cx.sidecar.roles.find(rgb);
    if (it != cx.sidecar.roles.end()) return it->second;
    if (std::find(cx.unmapped.begin(), cx.unmapped.end(), rgb) == cx.unmapped.end()) {
        cx.unmapped.push_back(rgb);
        char hex[8];
        std::snprintf(hex, sizeof hex, "#%06x", static_cast<unsigned>(rgb));
        cx.sink.lint("unmapped-colour", std::string("colour ") + hex + " has no role in the shape sidecar; it paints as body",
                     cx.source, path);
    }
    return ShapeRole::Body;
}

// Distance from a point to the closed rings' outline.
double outlineDist(Pt p, const std::vector<Ring>& rings) {
    double best = 1e300;
    for (const Ring& r : rings) {
        const size_t n = r.pts.size();
        for (size_t i = 0; i < n; ++i) {
            const Pt a = r.pts[i], b = r.pts[(i + 1) % n];
            const double dx = b.x - a.x, dy = b.y - a.y, l2 = dx * dx + dy * dy;
            double t = l2 > 0 ? ((p.x - a.x) * dx + (p.y - a.y) * dy) / l2 : 0;
            t = std::clamp(t, 0.0, 1.0);
            best = std::min(best, std::hypot(p.x - (a.x + t * dx), p.y - (a.y + t * dy)));
        }
    }
    return best;
}

/// Symmetric Hausdorff distance between two ring sets' outlines (vertex-sampled).
double outlineDistance(const std::vector<Ring>& a, const std::vector<Ring>& b) {
    if (a.empty() || b.empty()) return 1e300;
    double d = 0;
    for (const Ring& r : a)
        for (const Pt& p : r.pts) d = std::max(d, outlineDist(p, b));
    for (const Ring& r : b)
        for (const Pt& p : r.pts) d = std::max(d, outlineDist(p, a));
    return d;
}

/// The paint of @p n into @p f: roles, rule, dither, stroke (lints as it goes).
void paintOf(const Node& n, ShapeFill& f, Context& cx, const std::string& path) {
    const shape_svg::Paint& paint = n.paint;
    f.role = roleOf(paint.fill, paint.rgb, cx, path);
    f.flags = paint.evenOdd ? SHAPE_FILL_EVENODD : 0;

    // Dither: opacity snapped to quarters of coverage.
    const double op = paint.opacity;
    const int quarters = std::clamp(static_cast<int>(std::lround(op * 4)), 1, 4);
    f.dither = static_cast<uint8_t>(quarters);
    if (std::fabs(op - quarters / 4.0) > 0.005) {
        char m[120];
        std::snprintf(m, sizeof m, "opacity %.0f %% is not 25, 50, 75 or 100 %%; it dithers at %d %%", op * 100, quarters * 25);
        cx.sink.lint("off-level-opacity", m, cx.source, path);
    }

    // Stroke: the inner width (closed rings) or the line width (open), in whole px.
    if (paint.stroke == shape_svg::Paint::Fill::Colour) {
        const double w = paint.strokeWidth;
        const long px = std::lround(w);
        if (std::fabs(w - static_cast<double>(px)) > 0.01) {
            char m[120];
            std::snprintf(m, sizeof m, "stroke-width %.2f is not a whole number of px; it strokes %ld px", w, px);
            cx.sink.lint("fractional-stroke", m, cx.source, path);
        }
        if (px > 0) {
            f.strokeWidth = static_cast<uint8_t>(std::min(px, 255L));
            f.strokeRole = roleOf(paint.stroke, paint.strokeRgb, cx, path);
        }
    }
}

/// Append @p shapes (Kit coordinates) to @p piece relative to @p anchor, mirrored when @p flip.
/// Returns the number of Shapes added.  Own-space clips go to cx.pending (see appendClips()).
uint16_t addShapes(ShapePiece& piece, const std::vector<const Node*>& shapes, Pt anchor, bool flip, Context& cx,
                   const std::string& path) {
    uint16_t added = 0;
    auto local = [&](const Pt& p) { return qpt({flip ? anchor.x - p.x : p.x - anchor.x, p.y - anchor.y}, cx, path); };
    for (const Node* n : shapes) {
        const std::string here = join(path, n->name);
        ShapeFill f;
        paintOf(*n, f, cx, here);
        f.firstRing = static_cast<uint16_t>(piece.rings.size());
        for (const Ring& r : n->rings) {
            ShapeRing ring;
            ring.firstPoint = static_cast<uint16_t>(piece.points.size());
            ring.flags = r.closed ? SHAPE_RING_CLOSED : 0;
            std::vector<ShapePoint> pts;
            for (const Pt& p : r.pts) {
                const ShapePoint s = local(p);
                if (pts.empty() || pts.back().x != s.x || pts.back().y != s.y) pts.push_back(s);
            }
            if (r.closed && pts.size() > 1 && pts.back().x == pts.front().x && pts.back().y == pts.front().y) pts.pop_back();
            if (pts.size() < (r.closed ? 3u : 2u)) continue;
            if (piece.points.size() + pts.size() > cx.limits.maxPiecePoints) {
                cx.sink.error("too-large", "a piece has more points than the cap", cx.source, path);
                return added;
            }
            piece.points.insert(piece.points.end(), pts.begin(), pts.end());
            ring.pointCount = static_cast<uint16_t>(pts.size());
            piece.rings.push_back(ring);
        }
        f.ringCount = static_cast<uint16_t>(piece.rings.size() - f.firstRing);
        if (f.ringCount == 0) {
            cx.sink.lint("empty-shape", "a shape with no area is dropped", cx.source, here);
            continue;
        }

        // Clip: the live Body when it matches the Kit's Body, else the Feature's own space.
        if (n->paint.clip >= 0 && cx.clips && static_cast<size_t>(n->paint.clip) < cx.clips->size()) {
            const std::vector<Ring>& clip = (*cx.clips)[static_cast<size_t>(n->paint.clip)];
            const double d = cx.inBody ? 1e300 : outlineDistance(clip, cx.bodyRings);
            if (d <= kLiveBodyClip) {
                f.clip = ShapeClip::LiveBody;
            } else {
                if (d <= kNearBodyClip) {
                    char m[140];
                    std::snprintf(m, sizeof m, "the clip is %.2f px off the Body; it clips in the Feature's own space "
                                  "(redraw it from the Body to follow the Body)", d);
                    cx.sink.lint("near-body-clip", m, cx.source, here);
                }
                PendingClip pc;
                pc.shape = piece.shapes.size();
                for (const Ring& r : clip) {
                    std::vector<ShapePoint> pts;
                    for (const Pt& p : r.pts) {
                        const ShapePoint q = local(p);
                        if (pts.empty() || pts.back().x != q.x || pts.back().y != q.y) pts.push_back(q);
                    }
                    if (pts.size() > 1 && pts.back().x == pts.front().x && pts.back().y == pts.front().y) pts.pop_back();
                    if (pts.size() >= 3) pc.rings.push_back(std::move(pts));
                }
                f.clip = ShapeClip::Own;
                cx.pending.push_back(std::move(pc));
            }
        }
        piece.shapes.push_back(f);
        ++added;
    }
    return added;
}

/// Append the pending own-space clips as Shapes of role None after every span (one per distinct
/// geometry) and point their clipped Shapes at them.
void appendClips(ShapePiece& piece, Context& cx) {
    std::vector<std::pair<const PendingClip*, uint16_t>> made;
    for (const PendingClip& pc : cx.pending) {
        int found = -1;
        for (const auto& [other, index] : made)
            if (other->rings.size() == pc.rings.size()) {
                bool same = true;
                for (size_t r = 0; same && r < pc.rings.size(); ++r) {
                    const auto& x = pc.rings[r];
                    const auto& y = other->rings[r];
                    same = x.size() == y.size();
                    for (size_t k = 0; same && k < x.size(); ++k) same = x[k].x == y[k].x && x[k].y == y[k].y;
                }
                if (same) found = index;
            }
        if (found < 0) {
            ShapeFill c;
            c.role = ShapeRole::None;
            c.firstRing = static_cast<uint16_t>(piece.rings.size());
            for (const auto& pts : pc.rings) {
                if (piece.points.size() + pts.size() > cx.limits.maxPiecePoints) {
                    cx.sink.error("too-large", "a piece has more points than the cap", cx.source, "");
                    return;
                }
                piece.rings.push_back({static_cast<uint16_t>(piece.points.size()), static_cast<uint16_t>(pts.size()),
                                       SHAPE_RING_CLOSED});
                piece.points.insert(piece.points.end(), pts.begin(), pts.end());
            }
            c.ringCount = static_cast<uint16_t>(piece.rings.size() - c.firstRing);
            found = static_cast<int>(piece.shapes.size());
            piece.shapes.push_back(c);
            made.emplace_back(&pc, static_cast<uint16_t>(found));
        }
        piece.shapes[pc.shape].clipShape = static_cast<uint16_t>(found);
    }
    cx.pending.clear();
}

struct Built {
    ShapePiece  piece;
    std::string layer;
    bool        hasShapes = false;
};

struct KitSocket {
    ShapeSlot slot;
    Pt        at;
};

/// Are Shapes @p ia and @p ib the same drawing (paint, rings, own clip)?
bool sameShape(const ShapePiece& p, size_t ia, size_t ib, bool clip = true) {
            const ShapeFill& fa = p.shapes[ia];
            const ShapeFill& fb = p.shapes[ib];
            if (fa.role != fb.role || fa.flags != fb.flags || fa.ringCount != fb.ringCount || fa.dither != fb.dither ||
                fa.strokeWidth != fb.strokeWidth || fa.strokeRole != fb.strokeRole || fa.clip != fb.clip)
                return false;
            if (clip && fa.clip == ShapeClip::Own && !sameShape(p, fa.clipShape, fb.clipShape, false)) return false;
            for (uint16_t r = 0; r < fa.ringCount; ++r) {
                const ShapeRing& ra = p.rings[fa.firstRing + r];
                const ShapeRing& rb = p.rings[fb.firstRing + r];
                if (ra.pointCount != rb.pointCount || ra.flags != rb.flags) return false;
                // Order-insensitive (a mirrored copy winds the other way): each point has a twin.
                auto covered = [&](const ShapeRing& x, const ShapeRing& y) {
                    for (uint16_t k = 0; k < x.pointCount; ++k) {
                        const ShapePoint& px = p.points[x.firstPoint + k];
                        bool twin = false;
                        for (uint16_t j = 0; j < y.pointCount && !twin; ++j) {
                            const ShapePoint& py = p.points[y.firstPoint + j];
                            twin = std::abs(px.x - py.x) <= kSameDrawing && std::abs(px.y - py.y) <= kSameDrawing;
                        }
                        if (!twin) return false;
                    }
                    return true;
                };
                if (!covered(ra, rb) || !covered(rb, ra)) return false;
            }
            return true;
}

/// Are members @p a and @p b the same drawing in every pose?
bool sameDrawing(const ShapePiece& p, size_t a, size_t b) {
    for (size_t pose = 0; pose < p.poses.size(); ++pose) {
        const ShapeSpan& sa = p.span(pose, a);
        const ShapeSpan& sb = p.span(pose, b);
        if (sa.shapeCount != sb.shapeCount) return false;
        for (uint16_t i = 0; i < sa.shapeCount; ++i)
            if (!sameShape(p, sa.firstShape + i, sb.firstShape + i)) return false;
    }
    return true;
}

/// Keep only member 0 (its spans), leaving the Shapes of the others unreferenced, then compact.
void keepFirstMember(ShapePiece& p) {
    ShapePiece out = p;
    out.members.resize(1);
    out.spans.clear();
    out.shapes.clear();
    out.rings.clear();
    out.points.clear();
    auto copy = [&](size_t i) {
        ShapeFill f = p.shapes[i];
        const uint16_t firstRing = static_cast<uint16_t>(out.rings.size());
        for (uint16_t r = 0; r < f.ringCount; ++r) {
            ShapeRing ring = p.rings[f.firstRing + r];
            const uint16_t firstPoint = static_cast<uint16_t>(out.points.size());
            for (uint16_t k = 0; k < ring.pointCount; ++k) out.points.push_back(p.points[ring.firstPoint + k]);
            ring.firstPoint = firstPoint;
            out.rings.push_back(ring);
        }
        f.firstRing = firstRing;
        out.shapes.push_back(f);
    };
    for (size_t pose = 0; pose < p.poses.size(); ++pose) {
        const ShapeSpan s = p.span(pose, 0);
        ShapeSpan ns{static_cast<uint16_t>(out.shapes.size()), s.shapeCount};
        for (uint16_t i = 0; i < s.shapeCount; ++i) copy(s.firstShape + i);
        out.spans.push_back(ns);
    }
    // Own clips after every span, each once.
    std::map<uint16_t, uint16_t> clipAt;
    const size_t drawn = out.shapes.size();
    for (size_t i = 0; i < drawn; ++i) {
        ShapeFill& f = out.shapes[i];
        if (f.clip != ShapeClip::Own) continue;
        auto it = clipAt.find(f.clipShape);
        if (it == clipAt.end()) {
            const uint16_t old = f.clipShape;
            copy(old);  // may reallocate out.shapes
            it = clipAt.emplace(old, static_cast<uint16_t>(out.shapes.size() - 1)).first;
        }
        out.shapes[i].clipShape = it->second;
    }
    p = std::move(out);
}

void defaults(ShapeSlot slot, ShapeBinding& binding, double& hz, double& zeta) {
    hz = zeta = 0;
    switch (slot) {
        case ShapeSlot::Eyes: binding = ShapeBinding::Rigid; break;
        case ShapeSlot::Top:
        case ShapeSlot::Arms:
        case ShapeSlot::Legs:
            binding = ShapeBinding::Sway;
            hz = kDefaultSwayHz;
            zeta = kDefaultSwayZeta;
            break;
        default: binding = ShapeBinding::Field; break;
    }
}

uint16_t q412(double v) { return static_cast<uint16_t>(std::lround(std::clamp(v, 0.0, 65535.0 / 4096.0) * 4096)); }

/// One Feature slot layer → a piece (if it draws anything) + its sockets.
bool buildFeature(const Node& layer, ShapeSlot slot, bool behind, Context& cx, Built& out,
                  std::vector<KitSocket>& sockets) {
    const std::string& src = cx.source;
    // Pose layers.
    std::vector<std::pair<int, const Node*>> poses;
    for (const auto& k : layer.children) {
        const int pose = k.group ? poseByName(slot, k.name) : -1;
        if (pose >= 0) {
            for (const auto& p : poses)
                if (p.first == pose) {
                    cx.sink.error("duplicate-piece", "pose \"" + k.name + "\" appears twice", src, join(layer.name, k.name));
                    return false;
                }
            poses.emplace_back(pose, &k);
        }
    }
    if (poses.empty()) {
        poses.emplace_back(0, &layer);
    } else {
        for (const auto& k : layer.children)
            if (!(k.group && poseByName(slot, k.name) >= 0))
                cx.sink.lint("unknown-pose", "\"" + (k.name.empty() ? k.element : k.name) + "\" is not a pose of " +
                             shapeSlotName(slot) + "; it is ignored", src, join(layer.name, k.name));
    }
    std::sort(poses.begin(), poses.end(), [](const auto& a, const auto& b) { return a.first < b.first; });
    if (poses[0].first != 0) {
        cx.sink.error("missing-pose", std::string("the required pose \"") + shapePoseName(slot, 0) + "\" is missing", src,
                      layer.name);
        return false;
    }
    auto posePath = [&](size_t i) {
        return poses[i].second == &layer ? layer.name : join(layer.name, poses[i].second->name);
    };

    std::vector<std::vector<MemberDraw>> draws;
    for (size_t i = 0; i < poses.size(); ++i) draws.push_back(poseMembers(*poses[i].second, posePath(i), src, cx.sink));
    std::vector<MemberDraw>& base = draws[0];
    if (base.empty()) {
        cx.sink.lint("empty-shape", "the slot layer draws nothing", src, layer.name);
        return true;
    }

    // Anchors: own, else pose 0's of the same member, else bbox centre.
    std::vector<Pt> anchor0(base.size());
    for (size_t m = 0; m < base.size(); ++m) {
        if (base[m].hasAnchor) {
            anchor0[m] = base[m].anchor;
        } else {
            Box b;
            for (const Node* n : base[m].shapes) b.add(*n);
            anchor0[m] = b.empty() ? Pt{} : b.centre();
        }
        sockets.push_back({slot, anchor0[m]});
    }

    ShapePiece& p = out.piece;
    p.slot = slot;
    p.order = behind ? ShapeOrder::Behind : ShapeOrder::Front;
    ShapeBinding binding;
    double hz, zeta;
    defaults(slot, binding, hz, zeta);
    const auto ov = cx.sidecar.features.find(layer.name);
    if (ov != cx.sidecar.features.end()) {
        if (ov->second.hasBinding) binding = ov->second.binding;
        if (ov->second.hasSway) {
            hz = ov->second.hz;
            zeta = ov->second.zeta;
        }
    }
    if (binding != ShapeBinding::Sway) {
        hz = zeta = 0;
    } else if (hz == 0) {  // sway by override on a slot that doesn't sway by default
        hz = kDefaultSwayHz;
        zeta = kDefaultSwayZeta;
    }
    p.binding = binding;
    p.swayHz = q412(hz);
    p.swayZeta = q412(zeta);

    cx.pending.clear();
    cx.inBody = false;
    std::vector<bool> flip(base.size());
    for (size_t m = 0; m < base.size(); ++m) {
        const std::string& n = base[m].name;
        MemberSide side = MemberSide::Mirrored;
        if (n.size() > 2 && n[n.size() - 2] == '_' && n.back() == 'l') side = MemberSide::Left;
        if (n.size() > 2 && n[n.size() - 2] == '_' && n.back() == 'r') side = MemberSide::Right;
        flip[m] = side == MemberSide::Mirrored && anchor0[m].x < -kCentreBand;
        ShapeMember mem;
        mem.side = side;
        mem.home = qpt(anchor0[m], cx, layer.name);
        p.members.push_back(mem);
    }

    for (size_t i = 0; i < poses.size(); ++i) {
        p.poses.push_back(static_cast<uint8_t>(poses[i].first));
        std::vector<bool> used(draws[i].size());
        for (size_t m = 0; m < base.size(); ++m) {
            // Match by name; the implicit member matches the implicit member; else by position.
            const MemberDraw* d = nullptr;
            if (i == 0) {
                d = &base[m];
            } else {
                for (size_t k = 0; k < draws[i].size() && !d; ++k)
                    if (!used[k] && draws[i][k].name == base[m].name) {
                        used[k] = true;
                        d = &draws[i][k];
                    }
            }
            ShapeSpan span{static_cast<uint16_t>(p.shapes.size()), 0};
            if (d) {
                const Pt a = d->hasAnchor ? d->anchor : anchor0[m];
                span.shapeCount = addShapes(p, d->shapes, a, flip[m], cx, d->path);
                if (span.shapeCount) out.hasShapes = true;
            }
            p.spans.push_back(span);
        }
        if (i > 0)
            for (size_t k = 0; k < draws[i].size(); ++k)
                if (!used[k])
                    cx.sink.lint("unknown-member", "member \"" + draws[i][k].name + "\" is not in the required pose; it is ignored",
                                 src, draws[i][k].path);
    }
    appendClips(p, cx);
    if (cx.sink.errors) return false;

    // One drawing for every Mirrored member: keep one.
    bool allMirrored = p.members.size() > 1;
    for (const auto& m : p.members) allMirrored = allMirrored && m.side == MemberSide::Mirrored;
    bool same = allMirrored;
    for (size_t m = 1; same && m < p.members.size(); ++m) same = sameDrawing(p, 0, m);
    if (same) keepFirstMember(p);
    out.layer = layer.name;
    return true;
}

bool buildBody(const Node& layer, Context& cx, Built& out) {
    ShapePiece& p = out.piece;
    p.slot = ShapeSlot::Body;
    p.binding = ShapeBinding::Field;
    p.poses = {0};
    p.members = {ShapeMember{}};
    std::vector<const Node*> shapes;
    const Node* anchor = nullptr;
    collectShapes(layer, shapes, anchor);
    cx.pending.clear();
    cx.inBody = true;
    const uint16_t n = addShapes(p, shapes, {0, 0}, false, cx, layer.name);
    p.spans = {{0, n}};
    appendClips(p, cx);
    cx.inBody = false;
    out.hasShapes = n > 0;
    out.layer = layer.name;
    if (!out.hasShapes) cx.sink.lint("empty-shape", "the body layer draws nothing", cx.source, layer.name);
    return !cx.sink.errors;
}

std::string labelOf(const std::string& source) {
    std::string stem = source;
    const size_t slash = stem.find_last_of("/\\");
    if (slash != std::string::npos) stem = stem.substr(slash + 1);
    const size_t dot = stem.find('.');
    if (dot != std::string::npos && dot > 0) stem.resize(dot);
    std::string out;
    for (char c : stem)
        if (c >= 0x20 && c <= 0x7E && out.size() < SHAPE_PACK_MAX_LABEL) out += c;
    return out;
}

struct SourcePiece {
    std::string source, layer, label;
    ShapePiece  piece;
};

bool finishBody(Built& b, const std::vector<KitSocket>& sockets, const Node& bodyNode, const ShapeSource& src, Context& cx);

/// One source → its pieces.  In @p preview a piece-level error drops only its piece; a false
/// return (a file-level error, or any error outside preview) drops the whole source.
bool pullApart(const ShapeSource& src, const ShapeBundleLimits& lim, bool preview, Sink& sink,
               std::vector<uint32_t>& colours, std::vector<SourcePiece>& out) {
    Sidecar sidecar;
    if (!parseSidecar(src.sidecar, src.name, lim, sidecar, sink)) return false;
    Node root;
    if (!shape_svg::readSvg(src.svg.data(), src.svg.size(), src.name, lim, root, sink.out)) {
        sink.errors = true;
        return false;
    }
    Context cx{src.name, sidecar, lim, sink, colours, {}, &root.clipPaths, {}, false, {}, false};
    // Runs one piece's build with the error flag scoped to it: did it fail?
    auto failed = [&](auto&& build) {
        const bool before = sink.errors;
        sink.errors = false;
        const bool bad = !build() || sink.errors;
        sink.errors = before || sink.errors;
        return bad;
    };

    // Slot layers: top-level named nodes (a layer of one object may be that object).
    struct SlotLayer { ShapeSlot slot; const Node* node; size_t index; };
    std::vector<SlotLayer> layers;
    std::vector<Node> wrapped;
    wrapped.reserve(root.children.size());
    for (size_t i = 0; i < root.children.size(); ++i) {
        const Node& k = root.children[i];
        ShapeSlot slot;
        if (!slotByName(k.name, slot)) {
            sink.lint("unknown-layer", "top-level layer \"" + (k.name.empty() ? k.element : k.name) +
                      "\" is not a slot; it is ignored", src.name, k.name);
            continue;
        }
        for (const auto& l : layers)
            if (l.slot == slot) {
                sink.error("duplicate-piece", "slot layer \"" + k.name + "\" appears twice", src.name, k.name);
                return false;
            }
        const Node* node = &k;
        if (!k.group) {
            Node g;
            g.group = true;
            g.name = k.name;
            g.element = "g";
            Node child = k;
            child.name.clear();
            g.children.push_back(std::move(child));
            wrapped.push_back(std::move(g));
            node = &wrapped.back();
        }
        layers.push_back({slot, node, i});
    }

    const SlotLayer* body = nullptr;
    for (const auto& l : layers)
        if (l.slot == ShapeSlot::Body) body = &l;
    if (body) {
        // The Body as drawn: what a live-Body clip must match.
        std::vector<const Node*> shapes;
        const Node* anchor = nullptr;
        collectShapes(*body->node, shapes, anchor);
        for (const Node* n : shapes)
            for (const Ring& r : n->rings)
                if (r.closed) cx.bodyRings.push_back(r);
    }

    std::vector<KitSocket> sockets;
    std::vector<Built> built;
    for (const auto& l : layers) {
        if (l.slot == ShapeSlot::Body) continue;
        Built b;
        if (failed([&] { return buildFeature(*l.node, l.slot, body && l.index < body->index, cx, b, sockets); })) {
            if (!preview) return false;
            continue;
        }
        if (b.hasShapes) built.push_back(std::move(b));
    }
    Built b;
    if (body && failed([&] { return buildBody(*body->node, cx, b) && finishBody(b, sockets, *body->node, src, cx); })) {
        if (!preview) return false;
        body = nullptr;
    }
    if (body) built.insert(built.begin(), std::move(b));
    if (sink.errors && !preview) return false;
    const std::string label = labelOf(src.name);
    for (auto& piece : built) out.push_back({src.name, piece.layer, label, std::move(piece.piece)});
    return true;
}

/// The Body's sockets, one per Feature member drawn (or anchor) in the source; false if the
/// Body lacks the eyes and mouth ones.
bool finishBody(Built& b, const std::vector<KitSocket>& sockets, const Node& bodyNode, const ShapeSource& src, Context& cx) {
    Sink& sink = cx.sink;
    const Node* const body = &bodyNode;
    {
        for (const auto& s : sockets) {
            ShapeSocket so;
            so.slot = s.slot;
            so.side = s.at.x < -kCentreBand ? SocketSide::Left : s.at.x > kCentreBand ? SocketSide::Right : SocketSide::Centre;
            so.at = qpt(s.at, cx, body->name);
            b.piece.sockets.push_back(so);
        }
        std::stable_sort(b.piece.sockets.begin(), b.piece.sockets.end(),
                         [](const ShapeSocket& a, const ShapeSocket& c) { return a.slot < c.slot; });
        bool eyes = false, mouth = false;
        for (const auto& s : b.piece.sockets) {
            eyes = eyes || s.slot == ShapeSlot::Eyes;
            mouth = mouth || s.slot == ShapeSlot::Mouth;
        }
        if (!eyes || !mouth) {
            sink.error("missing-socket", std::string("the Body has no ") + (!eyes ? "eyes" : "mouth") +
                       " socket: draw that Feature or put an anchor in its slot layer", src.name, body->name);
            return false;
        }
        if (sockets.size() > 255) {
            sink.error("too-large", "the Body has more than 255 sockets", src.name, body->name);
            return false;
        }
    }
    return true;
}

}  // namespace

ShapeBundleResult bundleShapes(const std::vector<ShapeSource>& sources, const std::string& ledgerJson,
                               const ShapeBundleOptions& opts) {
    ShapeBundleResult r;
    r.ledger = ledgerJson;
    Sink sink{r.issues};
    const ShapeBundleLimits& lim = opts.limits;
    if (sources.size() > lim.maxSources) {
        sink.error("too-large", "more sources than the cap", "");
        return r;
    }
    const bool preview = opts.preview;
    Ledger ledger;
    if (!parseLedger(ledgerJson, lim, ledger, sink)) {
        if (!preview) return r;
        ledger = Ledger{};  // every piece is new: provisional ids
    }
    std::vector<bool> listed(sources.size(), true);
    for (size_t i = 0; i < sources.size(); ++i)
        for (size_t k = 0; k < i && listed[i]; ++k)
            if (sources[i].name == sources[k].name) {
                sink.error("duplicate-piece", "the same source is listed twice", sources[i].name);
                if (!preview) return r;
                listed[i] = false;
            }

    std::vector<SourcePiece> pieces;
    for (size_t i = 0; i < sources.size(); ++i) {
        if (!listed[i]) continue;
        const ShapeSource& s = sources[i];
        r.colours.push_back({s.name, {}});
        std::vector<SourcePiece> mine;
        if (pullApart(s, lim, preview, sink, r.colours.back().rgb, mine)) {
            for (auto& p : mine) pieces.push_back(std::move(p));
        }
    }
    if (sink.errors && !preview) return r;

    // Ids: keep the ledger's, give new pieces their slot's next free id, retire the vanished.
    std::vector<std::vector<bool>> taken(SHAPE_SLOT_COUNT);
    auto take = [&](ShapeSlot s, uint16_t id) {
        auto& t = taken[static_cast<uint8_t>(s)];
        if (t.size() <= id) t.resize(id + 1u);
        t[id] = true;
    };
    auto isTaken = [&](ShapeSlot s, uint32_t id) {
        const auto& t = taken[static_cast<uint8_t>(s)];
        return id < t.size() && t[id];
    };
    for (const auto* list : {&ledger.pieces, &ledger.retired})
        for (const auto& e : *list) take(e.slot, e.id);
    Ledger next;
    for (uint8_t i = 0; i < SHAPE_SLOT_COUNT; ++i) next.next[i] = ledger.next[i];
    next.retired = ledger.retired;
    std::vector<bool> seen(ledger.pieces.size());
    std::vector<ShapePackPieceIn> packIn;
    // Assign in a stable order (source, slot, layer) so new ids don't depend on argument order.
    std::vector<size_t> order(pieces.size());
    for (size_t i = 0; i < order.size(); ++i) order[i] = i;
    std::sort(order.begin(), order.end(), [&](size_t a, size_t b) {
        return std::make_tuple(pieces[a].source, static_cast<uint8_t>(pieces[a].piece.slot), pieces[a].layer) <
               std::make_tuple(pieces[b].source, static_cast<uint8_t>(pieces[b].piece.slot), pieces[b].layer);
    });
    for (size_t i : order) {
        SourcePiece& sp = pieces[i];
        const ShapeSlot slot = sp.piece.slot;
        uint32_t id = 0;
        for (size_t k = 0; k < ledger.pieces.size() && !id; ++k) {
            const LedgerEntry& e = ledger.pieces[k];
            if (e.source == sp.source && e.slot == slot && e.layer == sp.layer) {
                id = e.id;
                seen[k] = true;
            }
        }
        if (!id) {
            uint32_t& n = next.next[static_cast<uint8_t>(slot)];
            while (n <= 65535 && isTaken(slot, n)) ++n;
            if (n > 65535) {
                sink.error("too-large", std::string("no ids left in slot ") + shapeSlotName(slot), sp.source, sp.layer);
                return r;
            }
            id = n++;
            take(slot, static_cast<uint16_t>(id));
        }
        next.pieces.push_back({sp.source, sp.layer, slot, static_cast<uint16_t>(id)});
        packIn.push_back({slot, static_cast<uint16_t>(id), sp.label, std::move(sp.piece)});
    }
    for (size_t k = 0; k < ledger.pieces.size(); ++k)
        if (!seen[k]) next.retired.push_back(ledger.pieces[k]);
    for (const auto* list : {&next.pieces, &next.retired})
        for (const auto& e : *list) {
            uint32_t& n = next.next[static_cast<uint8_t>(e.slot)];
            n = std::max(n, static_cast<uint32_t>(e.id) + 1);
        }
    std::sort(next.pieces.begin(), next.pieces.end(), [](const LedgerEntry& a, const LedgerEntry& b) { return a.key() < b.key(); });
    std::sort(next.retired.begin(), next.retired.end(), [](const LedgerEntry& a, const LedgerEntry& b) {
        return std::make_tuple(static_cast<uint8_t>(a.slot), a.id) < std::make_tuple(static_cast<uint8_t>(b.slot), b.id);
    });

    const char* err = nullptr;
    std::vector<uint8_t> pack;
    if (!shapePackWrite(packIn, pack, &err)) {
        sink.error("too-large", std::string("cannot write the Shape pack: ") + (err ? err : "?"), "");
        return r;
    }
    if (packIn.empty()) {
        sink.error("unparsable", "no source has any Body or Feature", "");
        return r;
    }
    r.pack = std::move(pack);
    r.ledger = writeLedger(next);
    return r;
}

}  // namespace enjin2
