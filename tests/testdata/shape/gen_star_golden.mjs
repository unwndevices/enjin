// Generates tests/shape_star_golden.hpp: the prototype Star's fill masks at
// rest, as the prototype's own 4×4 majority rasterizer draws them (Tomodachi
// #329). Run from libs/enjin:  node tests/testdata/shape/gen_star_golden.mjs
//
// The SVG, the path flattener and rasterFill() are copied verbatim from
// docs/design/pet/prototype/look-and-feel.html (Tomodachi branch
// prototype/pet-look-and-feel @ 3eaac8b), options cov = 'majority',
// model at rest (every Shape is its SVG rings + HOME), quant = exact.
//
// The flattened points are rounded to Q8.8 (what a Shape pack stores), and
// the golden mask is the prototype's rasterFill() on those Q8.8 points
// (exact in a double), so the C++ fill must match it pixel for pixel. The
// script also reports how many pixels Q8.8 rounding itself moves against the
// unrounded prototype points, for the record.

import { writeFileSync } from 'node:fs';

const W = 160, H = 160, HOME = [80, 90];
const o = { cov: 'majority' };

const STAR = `<svg data-look="purple">
  <g data-feature="antenna" data-kind="sway" data-anchor="0,-20">
    <path d="M0 -20 L0 -27" data-role="none" data-stroke="body"/>
    <path d="M0 -26 Q-7 -31 -6 -38 Q-5 -45 0 -45 Q5 -45 6 -38 Q7 -31 0 -26 Z" data-role="body" data-dither="8"/>
  </g>
  <g data-feature="foot-l" data-kind="sway" data-anchor="-6,20"><ellipse cx="-6" cy="23" rx="4.5" ry="3.5" data-role="body" data-dither="8"/></g>
  <g data-feature="foot-r" data-kind="sway" data-anchor="6,20"><ellipse cx="6" cy="23" rx="4.5" ry="3.5" data-role="body" data-dither="8"/></g>
  <path id="body" d="M -19 -29 L 0 -21 L 19 -29 L 20 -16 L 36 -13 L 26 0 L 35 13 L 20 16 L 19 28 L 0 21 L -19 28 L -20 16 L -35 13 L -26 0 L -36 -13 L -20 -16 Z" data-role="body"/>
  <g data-feature="cheeks"><circle cx="-16" cy="-11" r="3" data-role="accent"/><circle cx="16" cy="-11" r="3" data-role="accent"/></g>
  <g data-feature="mouth">
    <path id="mouth-hole" d="M -18 -3 Q -18 -6 -14 -6 L 14 -6 Q 18 -6 18 -3 Q 17 14 0 14 Q -17 14 -18 -3 Z" data-role="ink"/>
    <ellipse cx="-8" cy="-5" rx="5" ry="4" data-role="accent"/>
    <ellipse cx="8" cy="-5" rx="5" ry="4" data-role="accent"/>
    <ellipse cx="-6" cy="10" rx="5" ry="4.5" data-role="accent" data-clip="mouth-hole"/>
    <ellipse cx="6" cy="10" rx="5" ry="4.5" data-role="accent" data-clip="mouth-hole"/>
  </g>
  <g data-feature="eyes" data-kind="eyes" data-anchor="0,-13">
    <rect x="-6" y="-15" width="2" height="3" data-role="ink"/><rect x="4" y="-15" width="2" height="3" data-role="ink"/>
  </g>
</svg>`;

// The Chomper's even-odd Body (a zig-zag mouth hole), for the fill rule.
const CHOMPER_BODY = `<svg>
  <path id="chomper-body" fill-rule="evenodd" data-role="body" d="M 28 -2 C 28 13.46 15.46 26 0 26 C -15.46 26 -28 13.46 -28 -2 C -28 -17.46 -15.46 -30 0 -30 C 15.46 -30 28 -17.46 28 -2 Z
     M -8 -6 L -3 2 L 2 -6 L 7 2 L 12 -6 L 17 2 L 24 -6 L 24 1 L 17 9 L 12 1 L 7 9 L 2 1 L -3 9 L -8 1 Z"/>
</svg>`;

// ---- verbatim from the prototype ------------------------------------------
const TOL = 0.2;
function distLine(p, a, b) {
  const dx = b[0] - a[0], dy = b[1] - a[1], L = Math.hypot(dx, dy);
  if (L < 1e-9) return Math.hypot(p[0] - a[0], p[1] - a[1]);
  return Math.abs((p[0] - a[0]) * dy - (p[1] - a[1]) * dx) / L;
}
const mid = (a, b) => [(a[0] + b[0]) / 2, (a[1] + b[1]) / 2];
function flatC(out, p0, p1, p2, p3, d = 0) {
  if (d > 12 || Math.max(distLine(p1, p0, p3), distLine(p2, p0, p3)) < TOL) { out.push(p3.slice()); return; }
  const p01 = mid(p0, p1), p12 = mid(p1, p2), p23 = mid(p2, p3), p012 = mid(p01, p12), p123 = mid(p12, p23), m = mid(p012, p123);
  flatC(out, p0, p01, p012, m, d + 1); flatC(out, m, p123, p23, p3, d + 1);
}
function flatQ(out, p0, p1, p2) {
  flatC(out, p0, [p0[0] + 2 / 3 * (p1[0] - p0[0]), p0[1] + 2 / 3 * (p1[1] - p0[1])],
                 [p2[0] + 2 / 3 * (p1[0] - p2[0]), p2[1] + 2 / 3 * (p1[1] - p2[1])], p2);
}
function parsePath(d) {
  const toks = d.match(/[a-zA-Z]|-?\d*\.?\d+(?:e[-+]?\d+)?/g);
  let i = 0, cmd = '', cx = 0, cy = 0, sx = 0, sy = 0, cur = null;
  const subs = [], num = () => parseFloat(toks[i++]);
  while (i < toks.length) {
    if (/[a-zA-Z]/.test(toks[i])) cmd = toks[i++];
    const rel = cmd === cmd.toLowerCase(), C = cmd.toUpperCase(), ox = rel ? cx : 0, oy = rel ? cy : 0;
    if (C === 'M') { const x = num() + ox, y = num() + oy; cur = { pts: [[x, y]], closed: false }; subs.push(cur); cx = sx = x; cy = sy = y; cmd = rel ? 'l' : 'L'; }
    else if (C === 'L') { const x = num() + ox, y = num() + oy; cur.pts.push([x, y]); cx = x; cy = y; }
    else if (C === 'H') { const x = num() + ox; cur.pts.push([x, cy]); cx = x; }
    else if (C === 'V') { const y = num() + oy; cur.pts.push([cx, y]); cy = y; }
    else if (C === 'Q') { const x1 = num() + ox, y1 = num() + oy, x = num() + ox, y = num() + oy; flatQ(cur.pts, [cx, cy], [x1, y1], [x, y]); cx = x; cy = y; }
    else if (C === 'C') { const x1 = num() + ox, y1 = num() + oy, x2 = num() + ox, y2 = num() + oy, x = num() + ox, y = num() + oy; flatC(cur.pts, [cx, cy], [x1, y1], [x2, y2], [x, y]); cx = x; cy = y; }
    else if (C === 'Z') {
      cur.closed = true; cx = sx; cy = sy;
      const p = cur.pts, a = p[0], b = p[p.length - 1];
      if (p.length > 1 && Math.abs(a[0] - b[0]) < 1e-6 && Math.abs(a[1] - b[1]) < 1e-6) p.pop();
    }
    else throw new Error('unsupported path command ' + cmd);
  }
  return subs;
}
function elemToD(el) {
  const t = el.tagName.toLowerCase(), g = n => parseFloat(el.getAttribute(n) || 0);
  if (t === 'path') return el.getAttribute('d');
  if (t === 'circle' || t === 'ellipse') {
    const cx = g('cx'), cy = g('cy'), rx = t === 'circle' ? g('r') : g('rx'), ry = t === 'circle' ? g('r') : g('ry'), k = 0.5523;
    return `M ${cx + rx} ${cy} C ${cx + rx} ${cy + ry * k} ${cx + rx * k} ${cy + ry} ${cx} ${cy + ry} C ${cx - rx * k} ${cy + ry} ${cx - rx} ${cy + ry * k} ${cx - rx} ${cy} ` +
           `C ${cx - rx} ${cy - ry * k} ${cx - rx * k} ${cy - ry} ${cx} ${cy - ry} C ${cx + rx * k} ${cy - ry} ${cx + rx} ${cy - ry * k} ${cx + rx} ${cy} Z`;
  }
  if (t === 'rect') {
    const x = g('x'), y = g('y'), w = g('width'), h = g('height'), r = Math.min(g('rx'), w / 2, h / 2);
    if (!r) return `M ${x} ${y} L ${x + w} ${y} L ${x + w} ${y + h} L ${x} ${y + h} Z`;
    const k = 0.5523 * r;
    return `M ${x + r} ${y} L ${x + w - r} ${y} C ${x + w - r + k} ${y} ${x + w} ${y + r - k} ${x + w} ${y + r} L ${x + w} ${y + h - r} ` +
           `C ${x + w} ${y + h - r + k} ${x + w - r + k} ${y + h} ${x + w - r} ${y + h} L ${x + r} ${y + h} C ${x + r - k} ${y + h} ${x} ${y + h - r + k} ${x} ${y + h - r} ` +
           `L ${x} ${y + r} C ${x} ${y + r - k} ${x + r - k} ${y} ${x + r} ${y} Z`;
  }
  throw new Error('unsupported element ' + t);
}
function rasterFill(rings, rule) {
  const m = new Uint8Array(W * H);
  const ss = o.cov === 'centre' ? 1 : 4, thr = o.cov === 'majority' ? 8 : 1;
  const cnt = ss > 1 ? new Uint8Array(W * H) : m;
  const edges = [];
  let ymin = 1e9, ymax = -1e9;
  for (const r of rings) {
    if (r.length < 3) continue;
    for (let i = 0; i < r.length; i++) {
      const a = r[i], b = r[(i + 1) % r.length];
      if (a[1] === b[1]) continue;
      const down = a[1] < b[1], t = down ? a : b, u = down ? b : a;
      edges.push([t[1], u[1], t[0], (u[0] - t[0]) / (u[1] - t[1]), down ? 1 : -1]);
      ymin = Math.min(ymin, t[1]); ymax = Math.max(ymax, u[1]);
    }
  }
  const sy0 = Math.max(0, Math.floor(ymin * ss)), sy1 = Math.min(H * ss, Math.ceil(ymax * ss));
  const xs = [];
  for (let sy = sy0; sy < sy1; sy++) {
    const yc = (sy + 0.5) / ss; xs.length = 0;
    for (const e of edges) if (e[0] <= yc && e[1] > yc) xs.push([e[2] + (yc - e[0]) * e[3], e[4]]);
    xs.sort((p, q) => p[0] - q[0]);
    let wind = 0;
    for (let k = 0; k < xs.length - 1; k++) {
      wind += xs[k][1];
      const inside = rule === 'evenodd' ? (wind & 1) !== 0 : wind !== 0;
      if (!inside) continue;
      // pixel-centre sampling, top-left rule: a centre exactly on a left edge is in, on a right edge out
      const a = Math.max(0, Math.ceil(xs[k][0] * ss - 0.5)), b = Math.min(W * ss - 1, Math.ceil(xs[k + 1][0] * ss - 0.5) - 1);
      const row = Math.floor(sy / ss) * W;
      for (let sx = a; sx <= b; sx++) cnt[row + Math.floor(sx / ss)]++;
    }
  }
  if (ss > 1) for (let i = 0; i < W * H; i++) m[i] = cnt[i] >= thr ? 1 : 0;
  else for (let i = 0; i < W * H; i++) m[i] = m[i] ? 1 : 0;
  return m;
}
// ---- end verbatim ---------------------------------------------------------

// The SVG subset as {tagName, getAttribute} shims (no DOM in node).
const shapes = [];
let feature = null, member = 0;
for (const m of (STAR + CHOMPER_BODY).matchAll(/<g data-feature="([^"]*)"|<\/g>|<(path|ellipse|circle|rect)\b([^>]*?)\/>/g)) {
  if (m[1]) { feature = m[1]; member = 0; continue; }
  if (!m[2]) { feature = null; continue; }
  m.splice(1, 1);
  const attrs = {};
  for (const a of m[2].matchAll(/([\w-]+)="([^"]*)"/g)) attrs[a[1]] = a[2];
  const el = { tagName: m[1], getAttribute: n => (n in attrs ? attrs[n] : null) };
  const subs = parsePath(elemToD(el));
  if (!subs.some(s => s.closed)) continue;  // stroke-only (the antenna stalk)
  const name = attrs.id || `${feature}/${member} ${m[1]}`;
  member++;
  shapes.push({ name, rule: attrs['fill-rule'] || 'nonzero',
                rings: subs.filter(s => s.closed).map(s => s.pts.map(p => [p[0] + HOME[0], p[1] + HOME[1]])) });
}

const q88 = v => Math.round(v * 256);
let out = `// GENERATED by tests/testdata/shape/gen_star_golden.mjs (Tomodachi #329): do not edit.
// The prototype Star at rest (look-and-feel.html @ 3eaac8b, HOME = (80, 90))
// plus the Chomper's even-odd Body: each closed-ring Shape flattened as the
// prototype does, rounded to Q8.8, and the prototype's 4x4 majority
// rasterFill() of those Q8.8 rings.
#pragma once

#include <cstdint>

namespace star_golden {

struct GoldenShape {
    const char*           name;
    bool                  evenOdd;
    uint16_t              ringCount;
    const uint16_t*       ringSizes;   // points per ring
    const int16_t*        points;      // Q8.8 x, y pairs, ring after ring
    int16_t               x0, y0;      // golden picture origin (canvas px)
    uint16_t              w, h;
    const char* const*    rows;        // h rows of w chars, '#' = set
};

`;
const table = [];
let quantMoves = 0;
shapes.forEach((s, si) => {
  const qr = s.rings.map(r => r.map(p => [q88(p[0]), q88(p[1])]));
  const mask = rasterFill(qr.map(r => r.map(p => [p[0] / 256, p[1] / 256])), s.rule);
  const exact = rasterFill(s.rings, s.rule);
  for (let i = 0; i < W * H; i++) if (mask[i] !== exact[i]) quantMoves++;
  let x0 = W, y0 = H, x1 = -1, y1 = -1;
  for (let y = 0; y < H; y++) for (let x = 0; x < W; x++) if (mask[y * W + x]) {
    x0 = Math.min(x0, x); x1 = Math.max(x1, x); y0 = Math.min(y0, y); y1 = Math.max(y1, y);
  }
  // one pixel of margin so the picture also pins what is NOT set
  x0 -= 1; y0 -= 1; x1 += 1; y1 += 1;
  const rows = [];
  for (let y = y0; y <= y1; y++) {
    let r = '';
    for (let x = x0; x <= x1; x++) r += mask[y * W + x] ? '#' : '.';
    rows.push(r);
  }
  const id = `s${si}`;
  out += `// ${s.name} (${s.rule})\n`;
  out += `static const uint16_t ${id}_sizes[] = {${qr.map(r => r.length).join(', ')}};\n`;
  out += `static const int16_t ${id}_points[] = {\n`;
  for (const r of qr) {
    const flat = r.flatMap(p => p);
    for (let i = 0; i < flat.length; i += 12) out += '    ' + flat.slice(i, i + 12).join(', ') + ',\n';
  }
  out += `};\n`;
  out += `static const char* const ${id}_rows[] = {\n${rows.map(r => `    "${r}",`).join('\n')}\n};\n\n`;
  table.push(`    {"${s.name}", ${s.rule === 'evenodd'}, ${qr.length}, ${id}_sizes, ${id}_points, ${x0}, ${y0}, ${x1 - x0 + 1}, ${y1 - y0 + 1}, ${id}_rows},`);
});
out += `static const GoldenShape kShapes[] = {\n${table.join('\n')}\n};\n`;
out += `static const unsigned kShapeCount = ${shapes.length};\n\n} // namespace star_golden\n`;
writeFileSync(new URL('../../shape_star_golden.hpp', import.meta.url), out);
console.log(`${shapes.length} shapes; Q8.8 rounding moves ${quantMoves} pixel(s) vs the unrounded prototype points`);
