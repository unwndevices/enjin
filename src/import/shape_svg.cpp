/**
 * @file shape_svg.cpp
 * @brief Kit SVG reader: XML, CSS classes, the SVG subset, transforms, flattening (Tomodachi #330)
 */
#include "shape_svg.hpp"

#include <algorithm>
#include <cctype>
#include <cmath>
#include <cstdlib>
#include <cstring>
#include <utility>

namespace enjin2::shape_svg {

namespace {

constexpr double kPi = 3.14159265358979323846;
constexpr uint32_t kMaxDocPoints = 1u << 18;
constexpr int kMaxCurveSegments = 256;

// ---------------------------------------------------------------------------
// XML
// ---------------------------------------------------------------------------

struct XmlElem {
    std::string name;
    std::vector<std::pair<std::string, std::string>> attrs;
    std::vector<XmlElem> kids;
    std::string text;

    const std::string* attr(const char* key) const {
        for (const auto& a : attrs)
            if (a.first == key) return &a.second;
        return nullptr;
    }
};

bool isWs(char c) { return c == ' ' || c == '\t' || c == '\n' || c == '\r'; }

class XmlReader {
public:
    XmlReader(const char* data, size_t size, uint32_t maxDepth, uint32_t maxElements)
        : p_(data), end_(data + size), maxDepth_(maxDepth), maxElements_(maxElements) {}

    bool document(XmlElem& root, std::string& err, bool& tooLarge) {
        if (end_ - p_ >= 3 && static_cast<unsigned char>(p_[0]) == 0xEF && static_cast<unsigned char>(p_[1]) == 0xBB &&
            static_cast<unsigned char>(p_[2]) == 0xBF)
            p_ += 3;
        bool ok = misc(true) && p_ < end_ && *p_ == '<' && element(root, 0) && misc(false) && p_ == end_;
        if (ok) return true;
        if (err_.empty()) err_ = p_ >= end_ ? "unexpected end of file" : "unexpected content";
        err = err_;
        tooLarge = tooLarge_;
        return false;
    }

private:
    const char* p_;
    const char* end_;
    uint32_t    maxDepth_, maxElements_;
    uint32_t    elements_ = 0;
    std::string err_;
    bool        tooLarge_ = false;

    bool fail(const char* m) {
        if (err_.empty()) err_ = m;
        return false;
    }
    bool starts(const char* s) const {
        const size_t n = std::strlen(s);
        return static_cast<size_t>(end_ - p_) >= n && std::memcmp(p_, s, n) == 0;
    }
    void ws() {
        while (p_ < end_ && isWs(*p_)) ++p_;
    }
    bool skipPast(const char* term) {
        const size_t n = std::strlen(term);
        while (static_cast<size_t>(end_ - p_) >= n) {
            if (std::memcmp(p_, term, n) == 0) {
                p_ += n;
                return true;
            }
            ++p_;
        }
        return fail("unterminated markup");
    }
    bool doctype() {
        p_ += 9;  // "<!DOCTYPE"
        char quote = 0;
        int bracket = 0;
        while (p_ < end_) {
            const char c = *p_++;
            if (quote) {
                if (c == quote) quote = 0;
            } else if (c == '"' || c == '\'') {
                quote = c;
            } else if (c == '[') {
                ++bracket;
            } else if (c == ']') {
                --bracket;
            } else if (c == '>' && bracket <= 0) {
                return true;
            }
        }
        return fail("unterminated DOCTYPE");
    }
    /// Prolog / epilog: whitespace, comments, PIs (and a DOCTYPE before the root).
    bool misc(bool prolog) {
        for (;;) {
            ws();
            if (starts("<?")) {
                if (!skipPast("?>")) return false;
            } else if (starts("<!--")) {
                if (!skipPast("-->")) return false;
            } else if (prolog && starts("<!DOCTYPE")) {
                if (!doctype()) return false;
            } else {
                return true;
            }
        }
    }

    static void utf8(std::string& out, uint32_t c) {
        if (c == 0 || c > 0x10FFFF) return;
        if (c < 0x80) {
            out += static_cast<char>(c);
        } else if (c < 0x800) {
            out += static_cast<char>(0xC0 | (c >> 6));
            out += static_cast<char>(0x80 | (c & 0x3F));
        } else if (c < 0x10000) {
            out += static_cast<char>(0xE0 | (c >> 12));
            out += static_cast<char>(0x80 | ((c >> 6) & 0x3F));
            out += static_cast<char>(0x80 | (c & 0x3F));
        } else {
            out += static_cast<char>(0xF0 | (c >> 18));
            out += static_cast<char>(0x80 | ((c >> 12) & 0x3F));
            out += static_cast<char>(0x80 | ((c >> 6) & 0x3F));
            out += static_cast<char>(0x80 | (c & 0x3F));
        }
    }

    /// Append @p s[0..n) with entities decoded.  Unknown entities stay verbatim.
    static void decode(std::string& out, const char* s, size_t n) {
        for (size_t i = 0; i < n; ++i) {
            if (s[i] != '&') {
                out += s[i];
                continue;
            }
            size_t semi = i + 1;
            while (semi < n && semi < i + 12 && s[semi] != ';') ++semi;
            if (semi >= n || s[semi] != ';') {
                out += '&';
                continue;
            }
            const std::string ent(s + i + 1, semi - i - 1);
            if (ent == "lt") out += '<';
            else if (ent == "gt") out += '>';
            else if (ent == "amp") out += '&';
            else if (ent == "quot") out += '"';
            else if (ent == "apos") out += '\'';
            else if (ent.size() > 1 && ent[0] == '#') {
                const bool hex = ent[1] == 'x' || ent[1] == 'X';
                const char* digits = ent.c_str() + (hex ? 2 : 1);
                char* stop = nullptr;
                const unsigned long v = std::strtoul(digits, &stop, hex ? 16 : 10);
                if (*digits && stop && *stop == 0) utf8(out, static_cast<uint32_t>(v > 0x10FFFF ? 0 : v));
                else out.append(s + i, semi - i + 1);
            } else {
                out.append(s + i, semi - i + 1);
            }
            i = semi;
        }
    }

    bool name(std::string& out) {
        const char* s = p_;
        while (p_ < end_ && !isWs(*p_) && *p_ != '/' && *p_ != '>' && *p_ != '=' && *p_ != '<') ++p_;
        if (p_ == s) return fail("expected a name");
        out.assign(s, static_cast<size_t>(p_ - s));
        return true;
    }

    bool element(XmlElem& el, uint32_t depth) {
        if (depth >= maxDepth_) {
            tooLarge_ = true;
            return fail("elements nested too deeply");
        }
        if (++elements_ > maxElements_) {
            tooLarge_ = true;
            return fail("too many elements");
        }
        ++p_;  // '<'
        if (!name(el.name)) return false;
        for (;;) {
            ws();
            if (p_ >= end_) return fail("unterminated tag");
            if (*p_ == '/') {
                ++p_;
                if (p_ >= end_ || *p_ != '>') return fail("expected '>'");
                ++p_;
                return true;
            }
            if (*p_ == '>') {
                ++p_;
                break;
            }
            std::string key;
            if (!name(key)) return false;
            ws();
            if (p_ >= end_ || *p_ != '=') return fail("expected '=' after an attribute name");
            ++p_;
            ws();
            if (p_ >= end_ || (*p_ != '"' && *p_ != '\'')) return fail("expected a quoted attribute value");
            const char q = *p_++;
            const char* s = p_;
            while (p_ < end_ && *p_ != q) ++p_;
            if (p_ >= end_) return fail("unterminated attribute value");
            std::string value;
            decode(value, s, static_cast<size_t>(p_ - s));
            ++p_;
            for (const auto& a : el.attrs)
                if (a.first == key) return fail("duplicate attribute");
            el.attrs.emplace_back(std::move(key), std::move(value));
        }
        // Content.
        for (;;) {
            if (p_ >= end_) return fail("unterminated element");
            if (starts("</")) {
                p_ += 2;
                std::string closing;
                if (!name(closing)) return false;
                ws();
                if (p_ >= end_ || *p_ != '>') return fail("expected '>'");
                ++p_;
                if (closing != el.name) return fail("mismatched closing tag");
                return true;
            }
            if (starts("<!--")) {
                if (!skipPast("-->")) return false;
            } else if (starts("<![CDATA[")) {
                p_ += 9;
                const char* s = p_;
                if (!skipPast("]]>")) return false;
                el.text.append(s, static_cast<size_t>(p_ - 3 - s));
            } else if (starts("<?")) {
                if (!skipPast("?>")) return false;
            } else if (starts("<!")) {
                return fail("unexpected markup declaration");
            } else if (*p_ == '<') {
                el.kids.emplace_back();
                if (!element(el.kids.back(), depth + 1)) return false;
            } else {
                const char* s = p_;
                while (p_ < end_ && *p_ != '<') ++p_;
                decode(el.text, s, static_cast<size_t>(p_ - s));
            }
        }
    }
};

// ---------------------------------------------------------------------------
// Numbers, colours, transforms
// ---------------------------------------------------------------------------

void skipWsComma(const char*& p, const char* end) {
    while (p < end && (isWs(*p) || *p == ',')) ++p;
}

/// SVG number at @p p (compact forms: "-1-2", ".5.5", "1e-3").  Skips leading ws/commas.
bool number(const char*& p, const char* end, double& out) {
    skipWsComma(p, end);
    const char* s = p;
    if (p < end && (*p == '+' || *p == '-')) ++p;
    bool digits = false;
    while (p < end && *p >= '0' && *p <= '9') ++p, digits = true;
    if (p < end && *p == '.') {
        ++p;
        while (p < end && *p >= '0' && *p <= '9') ++p, digits = true;
    }
    if (!digits) {
        p = s;
        return false;
    }
    if (p < end && (*p == 'e' || *p == 'E')) {
        const char* e = p + 1;
        if (e < end && (*e == '+' || *e == '-')) ++e;
        if (e < end && *e >= '0' && *e <= '9') {
            p = e;
            while (p < end && *p >= '0' && *p <= '9') ++p;
        }
    }
    if (p - s > 63) return false;
    char buf[64];
    std::memcpy(buf, s, static_cast<size_t>(p - s));
    buf[p - s] = 0;
    out = std::strtod(buf, nullptr);
    return std::isfinite(out);
}

bool number(const std::string& s, double& out) {
    const char* p = s.c_str();
    return number(p, p + s.size(), out);
}

/// An arc flag: one '0' or '1', no separator needed after it.
bool flag(const char*& p, const char* end, bool& out) {
    skipWsComma(p, end);
    if (p >= end || (*p != '0' && *p != '1')) return false;
    out = *p++ == '1';
    return true;
}

std::string trim(const std::string& s) {
    size_t a = 0, b = s.size();
    while (a < b && isWs(s[a])) ++a;
    while (b > a && isWs(s[b - 1])) --b;
    return s.substr(a, b - a);
}

std::string lower(std::string s) {
    for (char& c : s)
        if (c >= 'A' && c <= 'Z') c = static_cast<char>(c - 'A' + 'a');
    return s;
}

int hexDigit(char c) {
    if (c >= '0' && c <= '9') return c - '0';
    if (c >= 'a' && c <= 'f') return c - 'a' + 10;
    if (c >= 'A' && c <= 'F') return c - 'A' + 10;
    return -1;
}

/// Parse a CSS colour.  false when it is not one this reader knows.
bool colour(const std::string& raw, uint32_t& rgb) {
    const std::string v = lower(trim(raw));
    if (v.size() == 7 && v[0] == '#') {
        rgb = 0;
        for (int i = 1; i < 7; ++i) {
            const int d = hexDigit(v[i]);
            if (d < 0) return false;
            rgb = (rgb << 4) | static_cast<uint32_t>(d);
        }
        return true;
    }
    if (v.size() == 4 && v[0] == '#') {
        rgb = 0;
        for (int i = 1; i < 4; ++i) {
            const int d = hexDigit(v[i]);
            if (d < 0) return false;
            rgb = (rgb << 8) | static_cast<uint32_t>(d * 17);
        }
        return true;
    }
    if (v.compare(0, 4, "rgb(") == 0 && v.back() == ')') {
        const char* p = v.c_str() + 4;
        const char* end = v.c_str() + v.size() - 1;
        rgb = 0;
        for (int i = 0; i < 3; ++i) {
            double c = 0;
            if (!number(p, end, c)) return false;
            skipWsComma(p, end);
            if (p < end && *p == '%') {
                c = c * 255.0 / 100.0;
                ++p;
            }
            const long ci = std::lround(c < 0 ? 0 : c > 255 ? 255 : c);
            rgb = (rgb << 8) | static_cast<uint32_t>(ci);
        }
        skipWsComma(p, end);
        return p == end;
    }
    static const struct { const char* name; uint32_t rgb; } named[] = {
        {"black", 0x000000}, {"white", 0xFFFFFF}, {"red", 0xFF0000}, {"lime", 0x00FF00},
        {"green", 0x008000}, {"blue", 0x0000FF}, {"yellow", 0xFFFF00}, {"gray", 0x808080},
        {"grey", 0x808080},
    };
    for (const auto& n : named)
        if (v == n.name) {
            rgb = n.rgb;
            return true;
        }
    return false;
}

struct Matrix {
    double a = 1, b = 0, c = 0, d = 1, e = 0, f = 0;
    Pt apply(Pt p) const { return {a * p.x + c * p.y + e, b * p.x + d * p.y + f}; }
    Matrix operator*(const Matrix& o) const {  // this ∘ o
        return {a * o.a + c * o.b, b * o.a + d * o.b, a * o.c + c * o.d,
                b * o.c + d * o.d, a * o.e + c * o.f + e, b * o.e + d * o.f + f};
    }
};

/// Parse a `transform` list.  false on a malformed one.
bool transform(const std::string& s, Matrix& out) {
    out = Matrix{};
    const char* p = s.c_str();
    const char* end = p + s.size();
    for (;;) {
        skipWsComma(p, end);
        if (p >= end) return true;
        const char* n = p;
        while (p < end && ((*p >= 'a' && *p <= 'z') || (*p >= 'A' && *p <= 'Z'))) ++p;
        const std::string fn(n, static_cast<size_t>(p - n));
        while (p < end && isWs(*p)) ++p;
        if (p >= end || *p != '(') return false;
        ++p;
        double v[6];
        int k = 0;
        for (;;) {
            skipWsComma(p, end);
            if (p < end && *p == ')') {
                ++p;
                break;
            }
            if (k == 6 || !number(p, end, v[k])) return false;
            ++k;
        }
        Matrix m;
        if (fn == "matrix" && k == 6) {
            m = {v[0], v[1], v[2], v[3], v[4], v[5]};
        } else if (fn == "translate" && (k == 1 || k == 2)) {
            m.e = v[0];
            m.f = k == 2 ? v[1] : 0;
        } else if (fn == "scale" && (k == 1 || k == 2)) {
            m.a = v[0];
            m.d = k == 2 ? v[1] : v[0];
        } else if (fn == "rotate" && (k == 1 || k == 3)) {
            const double r = v[0] * kPi / 180.0, cs = std::cos(r), sn = std::sin(r);
            m = {cs, sn, -sn, cs, 0, 0};
            if (k == 3) {
                Matrix t1, t2;
                t1.e = v[1];
                t1.f = v[2];
                t2.e = -v[1];
                t2.f = -v[2];
                m = t1 * m * t2;
            }
        } else if (fn == "skewX" && k == 1) {
            m.c = std::tan(v[0] * kPi / 180.0);
        } else if (fn == "skewY" && k == 1) {
            m.b = std::tan(v[0] * kPi / 180.0);
        } else {
            return false;
        }
        out = out * m;
    }
}

// ---------------------------------------------------------------------------
// Geometry: transform, then flatten (Wang's formula) into rings
// ---------------------------------------------------------------------------

class Builder {
public:
    Builder(const Matrix& m, std::vector<Ring>& out, uint32_t& budget) : m_(m), out_(out), budget_(budget) {}

    bool overflow() const { return overflow_; }
    bool open() const { return open_; }
    Pt   current() const { return cur_; }

    void moveTo(Pt p) {
        finish();
        cur_ = start_ = p;
        ring_ = Ring{};
        open_ = true;
        push(m_.apply(p));
    }
    void lineTo(Pt p) {
        ensureOpen();
        cur_ = p;
        push(m_.apply(p));
    }
    void cubicTo(Pt c1, Pt c2, Pt p) {
        ensureOpen();
        const Pt a = m_.apply(cur_), b = m_.apply(c1), c = m_.apply(c2), d = m_.apply(p);
        const double ddx1 = a.x - 2 * b.x + c.x, ddy1 = a.y - 2 * b.y + c.y;
        const double ddx2 = b.x - 2 * c.x + d.x, ddy2 = b.y - 2 * c.y + d.y;
        const double mag = std::sqrt(std::fmax(ddx1 * ddx1 + ddy1 * ddy1, ddx2 * ddx2 + ddy2 * ddy2));
        const int n = segments(std::sqrt(mag * 6.0 / (8.0 * kFlattenTolerance)));
        for (int i = 1; i < n; ++i) {
            const double t = static_cast<double>(i) / n, u = 1 - t;
            const double w0 = u * u * u, w1 = 3 * u * u * t, w2 = 3 * u * t * t, w3 = t * t * t;
            push({w0 * a.x + w1 * b.x + w2 * c.x + w3 * d.x, w0 * a.y + w1 * b.y + w2 * c.y + w3 * d.y});
        }
        push(d);
        cur_ = p;
    }
    void quadTo(Pt c1, Pt p) {
        ensureOpen();
        const Pt a = m_.apply(cur_), b = m_.apply(c1), d = m_.apply(p);
        const double ddx = a.x - 2 * b.x + d.x, ddy = a.y - 2 * b.y + d.y;
        const int n = segments(std::sqrt(std::sqrt(ddx * ddx + ddy * ddy) * 2.0 / (8.0 * kFlattenTolerance)));
        for (int i = 1; i < n; ++i) {
            const double t = static_cast<double>(i) / n, u = 1 - t;
            push({u * u * a.x + 2 * u * t * b.x + t * t * d.x, u * u * a.y + 2 * u * t * b.y + t * t * d.y});
        }
        push(d);
        cur_ = p;
    }
    /// Elliptical arc of the ellipse (centre @p c, radii, x-axis rotation @p phi) from angle a0 to a1,
    /// as ≤ 45° cubics.  The current point must already be at angle a0.
    void ellipseArc(Pt c, double rx, double ry, double phi, double a0, double a1, const Pt* endExact = nullptr) {
        const double cs = std::cos(phi), sn = std::sin(phi);
        auto map = [&](double ux, double uy) { return Pt{c.x + rx * cs * ux - ry * sn * uy, c.y + rx * sn * ux + ry * cs * uy}; };
        const double span = a1 - a0;
        const int n = std::max(1, static_cast<int>(std::ceil(std::fabs(span) / (kPi / 4) - 1e-9)));
        const double delta = span / n;
        const double k = 4.0 / 3.0 * std::tan(delta / 4);
        for (int i = 0; i < n; ++i) {
            const double t0 = a0 + delta * i, t1 = a0 + delta * (i + 1);
            const double c0 = std::cos(t0), s0 = std::sin(t0), c1 = std::cos(t1), s1 = std::sin(t1);
            const Pt q1 = map(c0 - k * s0, s0 + k * c0);
            const Pt q2 = map(c1 + k * s1, s1 - k * c1);
            Pt q3 = map(c1, s1);
            if (i == n - 1 && endExact) q3 = *endExact;
            cubicTo(q1, q2, q3);
        }
    }
    /// SVG `A`: endpoint parameterisation (SVG 1.1 F.6.5).
    void arcTo(double rx, double ry, double phiDeg, bool large, bool sweep, Pt p) {
        ensureOpen();
        const Pt p1 = cur_;
        if (p1.x == p.x && p1.y == p.y) return;
        rx = std::fabs(rx);
        ry = std::fabs(ry);
        if (rx == 0 || ry == 0) {
            lineTo(p);
            return;
        }
        const double phi = std::fmod(phiDeg, 360.0) * kPi / 180.0, cs = std::cos(phi), sn = std::sin(phi);
        const double dx2 = (p1.x - p.x) / 2, dy2 = (p1.y - p.y) / 2;
        const double x1p = cs * dx2 + sn * dy2, y1p = -sn * dx2 + cs * dy2;
        const double lambda = (x1p * x1p) / (rx * rx) + (y1p * y1p) / (ry * ry);
        if (lambda > 1) {
            const double s = std::sqrt(lambda);
            rx *= s;
            ry *= s;
        }
        const double num = rx * rx * ry * ry - rx * rx * y1p * y1p - ry * ry * x1p * x1p;
        const double den = rx * rx * y1p * y1p + ry * ry * x1p * x1p;
        double coef = den > 0 ? std::sqrt(std::fmax(0.0, num / den)) : 0;
        if (large == sweep) coef = -coef;
        const double cxp = coef * rx * y1p / ry, cyp = -coef * ry * x1p / rx;
        const Pt c = {cs * cxp - sn * cyp + (p1.x + p.x) / 2, sn * cxp + cs * cyp + (p1.y + p.y) / 2};
        auto angle = [](double ux, double uy, double vx, double vy) { return std::atan2(ux * vy - uy * vx, ux * vx + uy * vy); };
        const double ux = (x1p - cxp) / rx, uy = (y1p - cyp) / ry, vx = (-x1p - cxp) / rx, vy = (-y1p - cyp) / ry;
        const double t1 = angle(1, 0, ux, uy);
        double dt = angle(ux, uy, vx, vy);
        if (!sweep && dt > 0) dt -= 2 * kPi;
        if (sweep && dt < 0) dt += 2 * kPi;
        ellipseArc(c, rx, ry, phi, t1, t1 + dt, &p);
    }
    void close() {
        if (!open_) return;
        ring_.closed = true;
        finish();
        cur_ = start_;
    }
    void finish() {
        if (open_ && !ring_.pts.empty()) out_.push_back(std::move(ring_));
        ring_ = Ring{};
        open_ = false;
    }

private:
    Matrix             m_;
    std::vector<Ring>& out_;
    uint32_t&          budget_;
    Ring ring_;
    Pt   cur_, start_;
    bool open_ = false, overflow_ = false;

    static int segments(double n) {
        if (!std::isfinite(n) || n > kMaxCurveSegments) return kMaxCurveSegments;
        return std::max(1, static_cast<int>(std::ceil(n)));
    }
    void ensureOpen() {
        if (!open_) {
            const Pt here = cur_;
            moveTo(here);
            start_ = here;
        }
    }
    void push(Pt p) {
        if (budget_ == 0) {
            overflow_ = true;
            return;
        }
        --budget_;
        ring_.pts.push_back(p);
    }
};

/// Path data → rings.  false (with @p err) on malformed data.
bool pathData(const std::string& d, Builder& b, uint32_t& commandBudget, std::string& err) {
    const char* p = d.c_str();
    const char* end = p + d.size();
    char cmd = 0;
    Pt lastCtrl;
    char lastKind = 0;  // 'C' or 'Q' when the previous segment was one
    for (;;) {
        skipWsComma(p, end);
        if (p >= end) break;
        if ((*p >= 'A' && *p <= 'Z') || (*p >= 'a' && *p <= 'z')) {
            cmd = *p++;
            if (!std::strchr("MmLlHhVvCcSsQqTtAaZz", cmd)) {
                err = std::string("unknown path command '") + cmd + "'";
                return false;
            }
        } else if (!cmd) {
            err = "path data must start with a command";
            return false;
        } else if (cmd == 'Z' || cmd == 'z') {
            err = "numbers after Z";
            return false;
        }
        if (commandBudget == 0) {
            err = "too many path commands";
            return false;
        }
        --commandBudget;
        const bool rel = cmd >= 'a';
        const Pt c = b.current();
        auto at = [&](double x, double y) { return rel ? Pt{c.x + x, c.y + y} : Pt{x, y}; };
        double v[7];
        auto nums = [&](int n) {
            for (int i = 0; i < n; ++i)
                if (!number(p, end, v[i])) return false;
            return true;
        };
        char kind = 0;
        switch (cmd) {
            case 'M': case 'm':
                if (!nums(2)) goto bad;
                b.moveTo(at(v[0], v[1]));
                cmd = rel ? 'l' : 'L';
                break;
            case 'L': case 'l':
                if (!nums(2)) goto bad;
                b.lineTo(at(v[0], v[1]));
                break;
            case 'H': case 'h':
                if (!nums(1)) goto bad;
                b.lineTo({rel ? c.x + v[0] : v[0], c.y});
                break;
            case 'V': case 'v':
                if (!nums(1)) goto bad;
                b.lineTo({c.x, rel ? c.y + v[0] : v[0]});
                break;
            case 'C': case 'c': {
                if (!nums(6)) goto bad;
                const Pt c2 = at(v[2], v[3]);
                b.cubicTo(at(v[0], v[1]), c2, at(v[4], v[5]));
                lastCtrl = c2;
                kind = 'C';
                break;
            }
            case 'S': case 's': {
                if (!nums(4)) goto bad;
                const Pt c1 = lastKind == 'C' ? Pt{2 * c.x - lastCtrl.x, 2 * c.y - lastCtrl.y} : c;
                const Pt c2 = at(v[0], v[1]);
                b.cubicTo(c1, c2, at(v[2], v[3]));
                lastCtrl = c2;
                kind = 'C';
                break;
            }
            case 'Q': case 'q': {
                if (!nums(4)) goto bad;
                const Pt c1 = at(v[0], v[1]);
                b.quadTo(c1, at(v[2], v[3]));
                lastCtrl = c1;
                kind = 'Q';
                break;
            }
            case 'T': case 't': {
                if (!nums(2)) goto bad;
                const Pt c1 = lastKind == 'Q' ? Pt{2 * c.x - lastCtrl.x, 2 * c.y - lastCtrl.y} : c;
                b.quadTo(c1, at(v[0], v[1]));
                lastCtrl = c1;
                kind = 'Q';
                break;
            }
            case 'A': case 'a': {
                bool large = false, sweep = false;
                if (!number(p, end, v[0]) || !number(p, end, v[1]) || !number(p, end, v[2]) ||
                    !flag(p, end, large) || !flag(p, end, sweep) || !number(p, end, v[3]) || !number(p, end, v[4]))
                    goto bad;
                b.arcTo(v[0], v[1], v[2], large, sweep, at(v[3], v[4]));
                break;
            }
            case 'Z': case 'z':
                b.close();
                break;
        }
        lastKind = kind;
        continue;
    bad:
        err = std::string("bad arguments to path command '") + cmd + "'";
        return false;
    }
    b.finish();
    return true;
}

bool pointList(const std::string& s, std::vector<Pt>& out) {
    const char* p = s.c_str();
    const char* end = p + s.size();
    out.clear();
    for (;;) {
        Pt pt;
        skipWsComma(p, end);
        if (p >= end) return true;
        if (!number(p, end, pt.x)) return false;
        if (!number(p, end, pt.y)) return true;  // odd count: SVG drops the last coordinate
        out.push_back(pt);
    }
}

// ---------------------------------------------------------------------------
// CSS (class rules only) and computed style
// ---------------------------------------------------------------------------

using Decls = std::vector<std::pair<std::string, std::string>>;

void declarations(const std::string& body, Decls& out) {
    size_t i = 0;
    while (i < body.size()) {
        size_t semi = body.find(';', i);
        if (semi == std::string::npos) semi = body.size();
        const std::string decl = body.substr(i, semi - i);
        const size_t colon = decl.find(':');
        if (colon != std::string::npos) {
            const std::string key = lower(trim(decl.substr(0, colon)));
            std::string value = trim(decl.substr(colon + 1));
            const size_t bang = value.find("!important");
            if (bang != std::string::npos) value = trim(value.substr(0, bang));
            if (!key.empty()) out.emplace_back(key, value);
        }
        i = semi + 1;
    }
}

struct ClassRule {
    std::string cls;
    Decls       decls;
};

void stylesheet(const std::string& css, std::vector<ClassRule>& rules) {
    // Strip comments.
    std::string s;
    for (size_t i = 0; i < css.size(); ++i) {
        if (css.compare(i, 2, "/*") == 0) {
            const size_t e = css.find("*/", i + 2);
            if (e == std::string::npos) break;
            i = e + 1;
            continue;
        }
        s += css[i];
    }
    size_t i = 0;
    while (i < s.size()) {
        const size_t open = s.find('{', i);
        if (open == std::string::npos) break;
        const std::string sel = trim(s.substr(i, open - i));
        // Find the matching '}' (skipping nested at-rule blocks wholesale).
        int depth = 0;
        size_t close = open;
        for (; close < s.size(); ++close) {
            if (s[close] == '{') ++depth;
            else if (s[close] == '}' && --depth == 0) break;
        }
        if (close >= s.size()) break;
        if (!sel.empty() && sel[0] != '@') {
            Decls decls;
            declarations(s.substr(open + 1, close - open - 1), decls);
            size_t a = 0;
            while (a <= sel.size()) {
                size_t comma = sel.find(',', a);
                if (comma == std::string::npos) comma = sel.size();
                const std::string one = trim(sel.substr(a, comma - a));
                bool plainClass = one.size() > 1 && one[0] == '.';
                for (size_t k = 1; plainClass && k < one.size(); ++k)
                    if (!(std::isalnum(static_cast<unsigned char>(one[k])) || one[k] == '-' || one[k] == '_')) plainClass = false;
                if (plainClass) rules.push_back({one.substr(1), decls});
                a = comma + 1;
            }
        }
        i = close + 1;
    }
}

void collectStyles(const XmlElem& e, std::string& css) {
    if (e.name == "style") css += e.text + "\n";
    for (const auto& k : e.kids) collectStyles(k, css);
}

/// Inherited part of the computed style.
struct Inherited {
    Paint::Fill fill = Paint::Fill::Colour;
    uint32_t    rgb = 0;
    bool        evenOdd = false;
    Paint::Fill stroke = Paint::Fill::None;
    uint32_t    strokeRgb = 0;
    double      strokeWidth = 1;
    double      fillOpacity = 1;
    int         clip = -1;  // not inherited in CSS, but it applies to the subtree
};

/// The id in `url(#id)` (quotes allowed), or "".
std::string urlId(const std::string& v) {
    const std::string t = trim(v);
    if (t.size() < 5 || t.compare(0, 4, "url(") != 0 || t.back() != ')') return "";
    std::string in = trim(t.substr(4, t.size() - 5));
    if (in.size() >= 2 && (in[0] == '\'' || in[0] == '"') && in.back() == in[0]) in = in.substr(1, in.size() - 2);
    return in.size() > 1 && in[0] == '#' ? in.substr(1) : "";
}

double scaleOf(const Matrix& m) { return std::sqrt(std::fabs(m.a * m.d - m.b * m.c)); }

// ---------------------------------------------------------------------------
// The walk: SVG elements → layer tree
// ---------------------------------------------------------------------------

std::string localName(const std::string& n) {
    const size_t c = n.find(':');
    return c == std::string::npos ? n : n.substr(c + 1);
}

class Walker {
public:
    Walker(const std::string& source, const ShapeBundleLimits& limits, std::vector<ImportIssue>& issues)
        : source_(source), limits_(limits), issues_(issues), budget_(kMaxDocPoints), commands_(limits.maxPathCommands) {}

    std::vector<ClassRule> rules;
    std::vector<std::pair<std::string, const XmlElem*>> ids;  ///< Every element's raw id.
    std::vector<std::vector<Ring>>* clips = nullptr;           ///< The root's clipPaths.
    bool errors = false;

    void collectIds(const XmlElem& e) {
        if (const std::string* id = e.attr("id")) ids.emplace_back(*id, &e);
        for (const auto& k : e.kids) collectIds(k);
    }
    const XmlElem* byId(const std::string& id) const {
        for (const auto& [k, v] : ids)
            if (k == id) return v;
        return nullptr;
    }

    void issue(ImportIssueSeverity sev, const char* code, const std::string& msg, const std::string& layer) {
        if (sev == ImportIssueSeverity::Error) errors = true;
        issues_.push_back({sev, code, msg, source_, layer});
    }
    void lintOnce(const char* code, const std::string& msg, const std::string& layer) {
        for (const auto& i : issues_)
            if (i.code == code && i.source == source_) return;
        issue(ImportIssueSeverity::Lint, code, msg, layer);
    }

    /// Children of @p e into @p parent.  @p mode: 0 draw, 1 defs (no drawing), 2 clip path.
    void children(const XmlElem& e, const Matrix& m, const Inherited& in, Node& parent, const std::string& path, int mode) {
        for (const auto& k : e.kids) element(k, m, in, parent, path, mode);
    }

    void element(const XmlElem& e, const Matrix& parentM, const Inherited& parentIn, Node& parent,
                 const std::string& path, int mode) {
        if (errors && (budget_ == 0 || commands_ == 0)) return;
        if (e.name.find(':') != std::string::npos) return;  // editor metadata (i:pgf, sodipodi:…)
        const std::string& n = e.name;
        static const char* const forbidden[] = {"text", "tspan", "textPath", "image", "linearGradient",
                                                "radialGradient", "pattern", "mask", "filter", "symbol",
                                                "foreignObject", "video", "audio", "iframe", "canvas"};
        for (const char* f : forbidden)
            if (n == f) {
                issue(ImportIssueSeverity::Error, "forbidden-element", "<" + n + "> is not allowed in a pet source", path);
                return;
            }
        if (n == "use") {
            if (mode != 2) issue(ImportIssueSeverity::Error, "forbidden-element", "<use> is only allowed in a clip path", path);
            return;
        }
        if (n == "style" || n == "title" || n == "desc" || n == "metadata") return;
        if (n == "defs") {
            Node scratch;
            children(e, parentM, parentIn, scratch, path, 1);
            return;
        }
        if (n == "clipPath") {
            Node scratch;
            children(e, parentM, parentIn, scratch, path, 2);
            return;
        }

        const bool isGroup = n == "g" || n == "a";
        static const char* const drawables[] = {"path", "circle", "ellipse", "rect", "polygon", "polyline", "line"};
        bool isDrawable = false;
        for (const char* d : drawables) isDrawable = isDrawable || n == d;
        if (!isGroup && !isDrawable) {
            issue(ImportIssueSeverity::Lint, "unknown-element", "<" + n + "> is not read", path);
            return;
        }

        Node node;
        node.group = isGroup;
        node.element = n;
        if (const std::string* dn = e.attr("data-name")) node.name = *dn;
        else if (const std::string* id = e.attr("id")) node.name = decodeLayerName(*id);
        const std::string here = node.name.empty() ? path : (path.empty() ? node.name : path + "/" + node.name);

        Matrix m = parentM;
        if (const std::string* t = e.attr("transform")) {
            Matrix local;
            if (!transform(*t, local)) {
                issue(ImportIssueSeverity::Error, "unparsable", "bad transform \"" + *t + "\"", here);
                return;
            }
            m = parentM * local;
        }
        Inherited in = parentIn;
        if (!style(e, isGroup, m, in, node.paint, here)) return;

        if (isGroup) {
            children(e, m, in, node, here, mode);
            if (mode == 0) parent.children.push_back(std::move(node));
            return;
        }
        if (mode != 0) return;  // geometry in defs / clip paths is not drawn (Tomodachi #332)
        Builder b(m, node.rings, budget_);
        if (!geometry(e, b, here)) return;
        b.finish();
        if (b.overflow()) {
            issue(ImportIssueSeverity::Error, "too-large", "the source has too many points", here);
            return;
        }
        parent.children.push_back(std::move(node));
    }

private:
    const std::string&        source_;
    const ShapeBundleLimits&  limits_;
    std::vector<ImportIssue>& issues_;
    uint32_t budget_;
    uint32_t commands_;

    double attrNum(const XmlElem& e, const char* key, double fallback, bool& bad) const {
        const std::string* v = e.attr(key);
        if (!v) return fallback;
        double out = 0;
        if (!number(*v, out)) {
            bad = true;
            return fallback;
        }
        return out;
    }

    bool geometry(const XmlElem& e, Builder& b, const std::string& here) {
        const std::string& n = e.name;
        bool bad = false;
        if (n == "path") {
            const std::string* d = e.attr("d");
            if (!d) return true;
            std::string err;
            if (!pathData(*d, b, commands_, err)) {
                issue(ImportIssueSeverity::Error, commands_ == 0 ? "too-large" : "unparsable", "path: " + err, here);
                return false;
            }
            return true;
        }
        if (n == "circle" || n == "ellipse") {
            const double cx = attrNum(e, "cx", 0, bad), cy = attrNum(e, "cy", 0, bad);
            double rx, ry;
            if (n == "circle") {
                rx = ry = attrNum(e, "r", 0, bad);
            } else {
                rx = attrNum(e, "rx", -1, bad);
                ry = attrNum(e, "ry", -1, bad);
                if (rx < 0) rx = ry;
                if (ry < 0) ry = rx;
            }
            if (bad) return badAttr(here);
            if (rx <= 0 || ry <= 0) return true;
            b.moveTo({cx + rx, cy});
            const Pt start = {cx + rx, cy};
            b.ellipseArc({cx, cy}, rx, ry, 0, 0, 2 * kPi, &start);
            b.close();
            return true;
        }
        if (n == "rect") {
            const double x = attrNum(e, "x", 0, bad), y = attrNum(e, "y", 0, bad);
            const double w = attrNum(e, "width", 0, bad), h = attrNum(e, "height", 0, bad);
            double rx = attrNum(e, "rx", -1, bad), ry = attrNum(e, "ry", -1, bad);
            if (bad) return badAttr(here);
            if (w <= 0 || h <= 0) return true;
            if (rx < 0) rx = ry;
            if (ry < 0) ry = rx;
            if (rx < 0) rx = ry = 0;
            rx = std::fmin(rx, w / 2);
            ry = std::fmin(ry, h / 2);
            if (rx <= 0 || ry <= 0) {
                b.moveTo({x, y});
                b.lineTo({x + w, y});
                b.lineTo({x + w, y + h});
                b.lineTo({x, y + h});
            } else {
                b.moveTo({x + rx, y});
                b.lineTo({x + w - rx, y});
                b.ellipseArc({x + w - rx, y + ry}, rx, ry, 0, -kPi / 2, 0);
                b.lineTo({x + w, y + h - ry});
                b.ellipseArc({x + w - rx, y + h - ry}, rx, ry, 0, 0, kPi / 2);
                b.lineTo({x + rx, y + h});
                b.ellipseArc({x + rx, y + h - ry}, rx, ry, 0, kPi / 2, kPi);
                b.lineTo({x, y + ry});
                b.ellipseArc({x + rx, y + ry}, rx, ry, 0, kPi, kPi * 1.5);
            }
            b.close();
            return true;
        }
        if (n == "polygon" || n == "polyline") {
            std::vector<Pt> pts;
            const std::string* v = e.attr("points");
            if (v && !pointList(*v, pts)) return badAttr(here);
            if (pts.empty()) return true;
            b.moveTo(pts[0]);
            for (size_t i = 1; i < pts.size(); ++i) b.lineTo(pts[i]);
            if (n == "polygon") b.close();
            return true;
        }
        if (n == "line") {
            const double x1 = attrNum(e, "x1", 0, bad), y1 = attrNum(e, "y1", 0, bad);
            const double x2 = attrNum(e, "x2", 0, bad), y2 = attrNum(e, "y2", 0, bad);
            if (bad) return badAttr(here);
            b.moveTo({x1, y1});
            b.lineTo({x2, y2});
            return true;
        }
        return true;
    }

    bool badAttr(const std::string& here) {
        issue(ImportIssueSeverity::Error, "unparsable", "a geometry attribute is not a number", here);
        return false;
    }

    /// Computed style: presentation attributes < class rules (sheet order) < style="".
    /// false on an error (opacity on a group).
    bool style(const XmlElem& e, bool isGroup, const Matrix& m, Inherited& in, Paint& paint, const std::string& here) {
        Decls props;
        static const char* const presentation[] = {"fill", "fill-rule", "stroke", "stroke-width", "opacity",
                                                   "fill-opacity", "clip-path"};
        for (const char* key : presentation)
            if (const std::string* v = e.attr(key)) props.emplace_back(key, trim(*v));
        if (const std::string* cls = e.attr("class")) {
            std::vector<std::string> names;
            size_t i = 0;
            while (i < cls->size()) {
                while (i < cls->size() && isWs((*cls)[i])) ++i;
                size_t j = i;
                while (j < cls->size() && !isWs((*cls)[j])) ++j;
                if (j > i) names.push_back(cls->substr(i, j - i));
                i = j;
            }
            for (const auto& r : rules)
                for (const auto& nm : names)
                    if (r.cls == nm) props.insert(props.end(), r.decls.begin(), r.decls.end());
        }
        if (const std::string* st = e.attr("style")) declarations(*st, props);

        double opacity = 1;
        std::string clipRef;
        auto paintOf = [&](const std::string& value, Paint::Fill& kind, uint32_t& rgb, const char* what) {
            const std::string v = lower(value);
            uint32_t c = 0;
            if (v == "none" || v == "transparent") {
                kind = Paint::Fill::None;
            } else if (colour(value, c)) {
                kind = Paint::Fill::Colour;
                rgb = c;
            } else {
                kind = Paint::Fill::Unsupported;
                if (v.compare(0, 4, "url(") != 0)
                    issue(ImportIssueSeverity::Lint, "bad-colour", std::string(what) + " \"" + value + "\" is not a colour", here);
            }
        };
        for (const auto& [key, value] : props) {
            if (value == "inherit") continue;
            if (key == "fill") {
                paintOf(value, in.fill, in.rgb, "fill");
            } else if (key == "fill-rule") {
                in.evenOdd = lower(value) == "evenodd";
            } else if (key == "stroke") {
                paintOf(value, in.stroke, in.strokeRgb, "stroke");
            } else if (key == "stroke-width") {
                double w = 0;
                if (number(value, w) && w >= 0) in.strokeWidth = w;
            } else if (key == "opacity") {
                double o = 1;
                if (number(value, o)) opacity = std::clamp(o, 0.0, 1.0);
            } else if (key == "fill-opacity") {
                double o = 1;
                if (number(value, o)) in.fillOpacity = std::clamp(o, 0.0, 1.0);
            } else if (key == "clip-path") {
                clipRef = lower(value) == "none" ? "" : value;
            }
        }
        if (isGroup && opacity < 1) {
            issue(ImportIssueSeverity::Error, "group-opacity",
                  "opacity on a group is not allowed; set it on each shape (it dithers the fill)", here);
            return false;
        }
        if (!clipRef.empty()) {
            const int clip = resolveClip(urlId(clipRef), m, here);
            if (clip >= 0 && in.clip >= 0)
                issue(ImportIssueSeverity::Lint, "nested-clip", "a clip inside a clip: only the inner one is used", here);
            if (clip >= 0) in.clip = clip;
        }
        paint.fill = in.fill;
        paint.rgb = in.rgb;
        paint.evenOdd = in.evenOdd;
        paint.opacity = opacity * in.fillOpacity;
        paint.stroke = in.stroke;
        paint.strokeRgb = in.strokeRgb;
        paint.strokeWidth = in.strokeWidth * scaleOf(m);
        paint.clip = in.clip;
        return true;
    }

    /// The geometry of clipPath `id` used by an element with transform @p m: a new clipPaths
    /// entry, or -1 (a lint) when there is no such clip path.
    int resolveClip(const std::string& id, const Matrix& m, const std::string& here) {
        const XmlElem* cp = id.empty() ? nullptr : byId(id);
        if (!cp || cp->name != "clipPath" || !clips) {
            issue(ImportIssueSeverity::Lint, "missing-clip",
                  "clip-path \"" + id + "\" names no <clipPath>; the shape is not clipped", here);
            return -1;
        }
        Matrix cm = m;
        if (const std::string* t = cp->attr("transform")) {
            Matrix local;
            if (transform(*t, local)) cm = m * local;
        }
        std::vector<Ring> rings;
        for (const auto& k : cp->kids) {
            const XmlElem* shape = &k;
            Matrix km = cm;
            if (const std::string* t = k.attr("transform")) {
                Matrix local;
                if (transform(*t, local)) km = cm * local;
            }
            if (k.name == "use") {
                const std::string* href = k.attr("href");
                if (!href) href = k.attr("xlink:href");
                shape = href && href->size() > 1 && (*href)[0] == '#' ? byId(href->substr(1)) : nullptr;
                if (!shape || shape->name == "clipPath" || shape->name == "use") {
                    issue(ImportIssueSeverity::Lint, "missing-clip", "a clip path's <use> names no shape", here);
                    continue;
                }
                bool bad = false;
                Matrix at;
                at.e = attrNum(k, "x", 0, bad);
                at.f = attrNum(k, "y", 0, bad);
                km = km * at;
                if (const std::string* t = shape->attr("transform")) {
                    Matrix local;
                    if (transform(*t, local)) km = km * local;
                }
            }
            Builder b(km, rings, budget_);
            if (!geometry(*shape, b, here)) continue;
            b.finish();
        }
        clips->push_back(std::move(rings));
        return static_cast<int>(clips->size() - 1);
    }
};

}  // namespace

std::string decodeLayerName(const std::string& raw) {
    std::string out;
    for (size_t i = 0; i < raw.size(); ++i) {
        // _xHH_ or _xHHHH_
        if (raw[i] == '_' && i + 1 < raw.size() && raw[i + 1] == 'x') {
            size_t j = i + 2;
            uint32_t v = 0;
            while (j < raw.size() && j < i + 8 && hexDigit(raw[j]) >= 0) v = (v << 4) | static_cast<uint32_t>(hexDigit(raw[j++]));
            const size_t digits = j - (i + 2);
            if ((digits == 2 || digits == 4) && j < raw.size() && raw[j] == '_') {
                if (v >= 0x20 && v < 0x7F) out += static_cast<char>(v);
                else if (v >= 0x80) {
                    if (v < 0x800) {
                        out += static_cast<char>(0xC0 | (v >> 6));
                        out += static_cast<char>(0x80 | (v & 0x3F));
                    } else {
                        out += static_cast<char>(0xE0 | (v >> 12));
                        out += static_cast<char>(0x80 | ((v >> 6) & 0x3F));
                        out += static_cast<char>(0x80 | (v & 0x3F));
                    }
                }
                i = j;
                continue;
            }
        }
        out += raw[i];
    }
    // Legacy duplicate suffix: name_12_
    if (out.size() >= 3 && out.back() == '_') {
        size_t k = out.size() - 1;
        while (k > 0 && out[k - 1] >= '0' && out[k - 1] <= '9') --k;
        if (k < out.size() - 1 && k >= 2 && out[k - 1] == '_') out.erase(k - 1);
    }
    return out;
}

bool readSvg(const uint8_t* data, size_t size, const std::string& source, const ShapeBundleLimits& limits,
             Node& root, std::vector<ImportIssue>& issues) {
    root = Node{};
    root.group = true;
    auto error = [&](const char* code, const std::string& msg) {
        issues.push_back({ImportIssueSeverity::Error, code, msg, source, ""});
        return false;
    };
    if (size > limits.maxSourceBytes) return error("too-large", "the SVG is larger than the size cap");
    if (!data || size == 0) return error("unparsable", "the SVG is empty");
    XmlElem doc;
    std::string err;
    bool tooLarge = false;
    XmlReader xml(reinterpret_cast<const char*>(data), size, limits.maxXmlDepth, limits.maxElements);
    if (!xml.document(doc, err, tooLarge)) return error(tooLarge ? "too-large" : "unparsable", "not readable XML: " + err);
    if (localName(doc.name) != "svg") return error("unparsable", "the root element is not <svg>");

    const std::string* vb = doc.attr("viewBox");
    double v[4] = {0, 0, 0, 0};
    bool vbOk = vb != nullptr;
    if (vbOk) {
        const char* p = vb->c_str();
        const char* end = p + vb->size();
        for (double& x : v) vbOk = vbOk && number(p, end, x);
        skipWsComma(p, end);
        vbOk = vbOk && p == end;
    }
    if (!vbOk || !(v[2] > 0) || !(v[3] > 0) || !std::isfinite(v[2]) || !std::isfinite(v[3]))
        return error("wrong-artboard", "the SVG needs a viewBox (one artboard)");
    root.viewW = v[2];
    root.viewH = v[3];

    Walker w(source, limits, issues);
    std::string css;
    collectStyles(doc, css);
    stylesheet(css, w.rules);
    Matrix origin;
    origin.e = -(v[0] + v[2] / 2);
    origin.f = -(v[1] + v[3] / 2);
    // The root walks as a group: its presentation attributes and transform apply to everything.
    XmlElem g;
    g.name = "g";
    for (const auto& a : doc.attrs)
        if (a.first != "id" && a.first != "data-name" && a.first != "viewBox") g.attrs.push_back(a);
    g.kids = std::move(doc.kids);
    w.collectIds(g);
    w.clips = &root.clipPaths;
    Node scratch;
    w.element(g, origin, Inherited{}, scratch, "", 0);
    if (!scratch.children.empty()) root.children = std::move(scratch.children.front().children);
    return !w.errors;
}

}  // namespace enjin2::shape_svg
