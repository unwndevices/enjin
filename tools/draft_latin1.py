#!/usr/bin/env python3
"""draft_latin1.py — a ONE-SHOT helper that drafted the Latin-1 glyphs (U+00A0..U+00FF)
Tomodachi's three pixel faces lack (Tomodachi #476, ADR-0029 "One format, three faces").

It is not part of the build. It ran once to write `tools/faces/<face>.latin1.txt`; from
then on those text files are the source of truth, hand-edited by the owner, and
`ttf2enjin.py --glyphs` reads them. Re-running this script OVERWRITES the owner's edits,
so do not, unless you mean to throw them away (it refuses without --force when a file
exists).

How a glyph is drafted, always from the face's own letters (the face's ASCII raster,
from ttf2enjin.rasterize):

  * accented letters = the base letter + a diacritic drawn in the face's stroke style;
    capital accents sit above the cap height with a 1 px gap, moved down (losing the
    gap) only when the line's ascent leaves no room; lowercase accents sit at the
    face's ascender height, 1 px above the x-height. Dotless i under i's accents.
  * Æ æ from the face's A/a and E/e overlapped; Ð = D + a crossbar; Þ þ from P/p and
    b's ascender; µ = u + a descender; ¿ = ? turned; ± × ÷ ¬ · ¯ ­ from + - x . _;
    the rest (ß ð ª º ¹ ² ³ ¼ ½ ¾ « » ® ° ¶ ...) as small per-face pixel art.
  * glyphs the TTF already has are never drafted (ttf2enjin rasterises those).

Usage (from the Tomodachi repo root):
    python3 libs/enjin/tools/draft_latin1.py [--face monogram|body|display] [--force]
"""

import argparse
import math
import os
import sys

HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, HERE)

import ttf2enjin  # noqa: E402

try:
    from fontTools.ttLib import TTFont
except ImportError:  # pragma: no cover - environment guard
    sys.stderr.write("draft_latin1: fontTools is required — pip install fonttools\n")
    raise

FACES = {
    "monogram": dict(ttf="monogram.ttf", size=16, ascent=11, monospace=6),
    "body": dict(ttf="1980v23P01.ttf", size=11, ascent=8, monospace=None),
    "display": dict(ttf="KarenFat.ttf", size=22, ascent=17, monospace=None),
}


# ---------------------------------------------------------------- bitmaps ------

class Bm:
    """Ink pixels in pen coordinates (x right, y down, baseline at y = 0) + advance."""

    def __init__(self, px=(), adv=0):
        self.px = set(px)
        self.adv = adv

    def copy(self):
        return Bm(self.px, self.adv)

    @property
    def minx(self):
        return min(x for x, _ in self.px)

    @property
    def maxx(self):
        return max(x for x, _ in self.px)

    @property
    def miny(self):
        return min(y for _, y in self.px)

    @property
    def maxy(self):
        return max(y for _, y in self.px)

    def shifted(self, dx, dy):
        return Bm({(x + dx, y + dy) for x, y in self.px}, self.adv)

    def rows_where(self, pred):
        return Bm({(x, y) for x, y in self.px if pred(y)}, self.adv)


def art(rows, x0=0, y0=0):
    """Pixel set from rows of '.'/'#' (a list, or one string split on whitespace)."""
    if isinstance(rows, str):
        rows = rows.split()
    return {(x0 + c, y0 + r) for r, row in enumerate(rows) for c, ch in enumerate(row)
            if ch == "#"}


# display's small figures (superscripts, ª º, fractions): 2 px strokes, 5 px wide.
SMALL = {
    "1": "..##. .###. ..##. ..##. ..##. ..##. .####",
    "2": ".###. ##.## ...## ..##. .##.. ##... #####",
    "3": "####. ...## ..##. ...## ...## ##.## .###.",
    "4": "...## ..### .#.## ##.## ##### ...## ...##",
    "a": ".###. ...## .#### ##.## .####",
    "o": ".###. ##.## ##.## ##.## .###.",
}


def small(ch):
    return Bm(art(SMALL[ch]))


def line(x0, y0, x1, y1):
    """Pixels of a 1 px line (DDA), endpoints included."""
    n = max(abs(x1 - x0), abs(y1 - y0))
    return {(int(math.floor(x0 + (x1 - x0) * t / n + 0.5)),
             int(math.floor(y0 + (y1 - y0) * t / n + 0.5))) for t in range(n + 1)}


# ------------------------------------------------------------------ faces ------

class Face:
    def __init__(self, name, fonts_dir):
        cfg = FACES[name]
        self.name = name
        self.path = os.path.join(fonts_dir, cfg["ttf"])
        self.size = cfg["size"]
        self.ascent = cfg["ascent"]
        self.monospace = cfg["monospace"]
        self.cmap = set(TTFont(self.path).getBestCmap())
        self._cache = {}
        self.cap_top = self.g("H").miny
        self.x_top = self.g("x").miny
        self.cap_acc_bottom = self.cap_top - 2  # 1 px gap above the cap height
        self.lc_acc_bottom = self.x_top - 2     # 1 px gap above the x-height
        self.bar = self.g("-").maxy - self.g("-").miny + 1  # horizontal stroke weight

    def g(self, ch):
        """The TTF's own glyph for `ch`, as a Bm."""
        if ch not in self._cache:
            gl = ttf2enjin.rasterize(self.path, self.size, ord(ch), ord(ch))[0][0]
            px = {(gl.x_offset + c, gl.y_offset + r)
                  for r, row in enumerate(gl.bits) for c, b in enumerate(row) if b}
            self._cache[ch] = Bm(px, gl.x_advance)
        return self._cache[ch].copy()

    def has(self, code):
        return code in self.cmap


# Diacritics per face: name -> (rows, anchor), where anchor is the accent column (may be
# .5) that sits over the base letter's centre. "cap" / "lc" variants where they differ.
ACCENTS = {
    "monogram": {
        "grave": ("#. .#", 1), "acute": (".# #.", 0), "circumflex": (".#. #.#", 1),
        "tilde": (".#..# #.##.", 2),           # the face's own '~'
        "diaeresis": ("#.#", 1),                # the face's own '¨'
        "ring": (".#. #.# .#.", 1),
        "cedilla": (".# ##", 1),
    },
    "body": {  # lowercase accents are the TTF's own (à á â ã ü), caps 1 row where ü's is
        "grave": ("##. .##", 1), "acute": ("..# .##", 1), "circumflex": ("##. #..", 1),
        "tilde": ("### ###", 1),
        "diaeresis": ("#.# #.#", 1), "diaeresis_cap": ("#.#", 1),
        "ring": (".#. #.# .#.", 1), "ring_cap": (".#. #.#", 1),
        "acute_cap": (".# #.", 0.5),
        "cedilla": (".##. ##..", 1.5),          # the TTF's own ç tail
    },
    "display": {
        "grave": ("####.... ..####.. ....####", 3.5),
        "acute": ("....#### ..####.. ####....", 3.5),
        "circumflex": ("...####... .########. ####..####", 4.5),
        "tilde": (".#####..### ########### ###..#####.", 5),
        "diaeresis": ("####..#### ####..#### ####..####", 4.5),  # the face's own '¨'
        "ring": (".####. ##..## .####.", 2.5),
        "cedilla": (".####. ...### ...### #####.", 2.5),
    },
}


def accent(face, name, cap):
    table = ACCENTS[face.name]
    key = name + ("_cap" if cap else "")
    rows, anchor = table.get(key, table[name])
    return art(rows), anchor


def centre_of(bm):
    return (bm.minx + bm.maxx) / 2.0


def finish(face, bm, base_adv, base_maxx):
    """Shift ink right of x = 0 if it went negative, and size the advance."""
    rsb = max(0, base_adv - (base_maxx + 1))
    shift = -bm.minx if bm.minx < 0 else 0
    out = bm.shifted(shift, 0)
    out.adv = max(base_adv + shift, out.maxx + 1 + rsb)
    if face.monospace:
        if out.maxx >= face.monospace:
            raise SystemExit("%s: glyph too wide for the monospace advance" % face.name)
        out.adv = face.monospace
    return out


def compose(face, base_ch, acc_name):
    base = face.g(base_ch)
    if base_ch == "i":  # dotless i: drop everything above the x-height
        base = base.rows_where(lambda y: y >= face.x_top)
    cap = base.miny < face.x_top  # taller than the x-height -> a capital
    px, anchor = accent(face, acc_name, cap)
    acc = Bm(px)
    aw_h = acc.maxy + 1
    x = int(math.floor(centre_of(base) - anchor + 0.5))
    if acc_name == "cedilla":
        y = base.maxy + 1
    else:
        bottom = face.cap_acc_bottom if cap else face.lc_acc_bottom
        top = bottom - aw_h + 1
        if top < -face.ascent:  # no room for the gap: move it down, never above the line
            top = -face.ascent
        y = top
    glyph = Bm(base.px | acc.shifted(x, y).px)
    return finish(face, glyph, base.adv, base.maxx)


ACCENTED = {}
for _code, _base, _acc in [
        (0xC0, "A", "grave"), (0xC1, "A", "acute"), (0xC2, "A", "circumflex"),
        (0xC3, "A", "tilde"), (0xC4, "A", "diaeresis"), (0xC5, "A", "ring"),
        (0xC7, "C", "cedilla"),
        (0xC8, "E", "grave"), (0xC9, "E", "acute"), (0xCA, "E", "circumflex"),
        (0xCB, "E", "diaeresis"),
        (0xCC, "I", "grave"), (0xCD, "I", "acute"), (0xCE, "I", "circumflex"),
        (0xCF, "I", "diaeresis"),
        (0xD1, "N", "tilde"),
        (0xD2, "O", "grave"), (0xD3, "O", "acute"), (0xD4, "O", "circumflex"),
        (0xD5, "O", "tilde"), (0xD6, "O", "diaeresis"),
        (0xD9, "U", "grave"), (0xDA, "U", "acute"), (0xDB, "U", "circumflex"),
        (0xDC, "U", "diaeresis"), (0xDD, "Y", "acute"),
        (0xFD, "y", "acute"), (0xFF, "y", "diaeresis")]:
    ACCENTED[_code] = (_base, _acc)
for _code, (_base, _acc) in list(ACCENTED.items()):
    if 0xC0 <= _code <= 0xDC:  # the lowercase row mirrors the capital row
        ACCENTED[_code + 0x20] = ("i" if _base == "I" else _base.lower(), _acc)

# Base letter shown next to each glyph on the review sheet.
BASE_LETTER = {code: b for code, (b, _a) in ACCENTED.items()}
BASE_LETTER.update({0xC6: "A", 0xE6: "a", 0xD0: "D", 0xF0: "o", 0xD8: "O", 0xF8: "o",
                    0xDE: "P", 0xFE: "p", 0xDF: "B", 0xB5: "u", 0xBF: "?", 0xA1: "!",
                    0xAA: "a", 0xBA: "o", 0xB9: "1", 0xB2: "2", 0xB3: "3", 0xBC: "4",
                    0xBD: "2", 0xBE: "3", 0xAB: "<", 0xBB: ">", 0xAD: "-", 0xAF: "-",
                    0xAC: "-", 0xB1: "+", 0xD7: "x", 0xF7: "-", 0xB7: ".", 0xB4: "'",
                    0xA8: "'", 0xB8: ",", 0xA0: " ", 0xA9: "C", 0xAE: "R", 0xB0: "o",
                    0xB6: "P", 0xA2: "c", 0xA3: "L", 0xA4: "o", 0xA5: "Y", 0xA6: "|",
                    0xA7: "S"})


# Per-face pixel art: code -> (yOffset of the top row, xOffset, rows, xAdvance or None).
ART = {
    "monogram": {
        0xAA: (-7, 1, ".## #.# .## ... ###", None),
        0xBA: (-7, 1, ".#. #.# .#. ... ###", None),
        0xAB: (-5, 0, ".#..# #..#. .#..#", None),
        0xBB: (-5, 0, "#..#. .#..# #..#.", None),
        0xAE: (-7, 0, ".###. #..## #.#.# #..## #.#.# #.#.# .###.", None),
        0xB0: (-7, 1, ".#. #.# .#.", None),
        0xB2: (-7, 1, "##. ..# .#. #.. ###", None),
        0xB3: (-7, 1, "##. ..# .#. ..# ##.", None),
        0xB9: (-7, 1, ".#. ##. .#. .#. ###", None),
        0xB6: (-7, 0, ".#### ###.# ###.# .##.# ..#.# ..#.# ..#.#", None),
        0xBC: (-7, 0, "#...# #..#. #.#.. .#... #.#.# ..### ....#", None),
        0xBD: (-7, 0, "#...# #..#. #.#.. .#.## #...# ...#. ..###", None),
        0xBE: (-7, 0, "##..# .#.#. ##.#. ..#.. #.#.# ..### ....#", None),
        0xC6: (-7, 0, ".#### #.#.. #.#.. ##### #.#.. #.#.. #.###", None),
        0xE6: (-5, 0, ".#### #.#.# #.### #.#.. .####", None),
        0xD0: (-7, 0, ".###. .#..# .#..# ###.# .#..# .#..# .###.", None),
        0xD8: (-7, 0, ".###. #...# #..## #.#.# ##..# #...# .###.", None),
        0xF8: (-5, 0, ".###. #..## #.#.# ##..# .###.", None),
        0xDF: (-7, 0, ".##.. #..#. #.#.. #..#. #...# #...# #.##.", None),
        0xF0: (-7, 0, "..#.# ...#. ..#.# .#### #...# #...# .###.", None),
    },
    "body": {
        0xAA: (-6, 0, ".## #.# .## ... ###", None),
        0xBA: (-6, 0, "### #.# ### ... ###", None),
        0xAB: (-4, 0, ".#..# #..#. .#..#", 6),
        0xBB: (-4, 0, "#..#. .#..# #..#.", 6),
        0xAE: (-6, 0, ".###. #..## #.#.# #..## #.#.# #.#.# .###.", 6),
        0xB0: (-6, 0, "### #.# ###", 4),
        0xB2: (-6, 0, "##. ..# .#. ###", 4),
        0xB3: (-6, 0, "##. .## ..# ##.", 4),
        0xB9: (-6, 0, ".# ## .# .#", 3),
        0xB6: (-6, 0, "#### ##.# .#.# .#.# .#.# .#.#", 5),
        0xBC: (-6, 0, "#....#. #...#.. #..#... ..#.#.# .#..### #.....#", 8),
        0xBD: (-6, 0, "#....#. #...#.. #..#... ..#.##. .#...#. #....##", 8),
        0xBE: (-6, 0, "###...#. .##..#.. ###.#... ...#.#.# ..#..### .#.....#", 9),
        0xD8: (-6, 0, "#### #..# #.## ##.# #..# ####", 5),
        0xF8: (-4, 0, "#### #.## ##.# ####", 5),
        0xDF: (-6, 0, "###. #.#. #.## #..# #..# #.##", 5),
        0xF0: (-6, 0, "##. .## ### #.# #.# ###", 4),
    },
    "display": {
        0xAB: (-8, 1, "..###..### .###..###. ###..###.. ###..###.. .###..###. ..###..###", 12),
        0xBB: (-8, 1, "###..###.. .###..###. ..###..### ..###..### .###..###. ###..###..", 12),
        0xAE: (-12, 0, ".###########. ############# ###......#### ###..###..### ###..###..### "
                       "###......#### ###..##..#### ###..###..### ###..###..### "
                       "###..###..### ############# .###########.", 14),
        0xB0: (-12, 1, ".####. ###### ##..## ##..## ###### .####.", 8),
        0xB6: (-12, 0, "..######### .########## ########### #######.### #######.### "
                       ".######.### ...####.### ...####.### ...####.### ...####.### "
                       "...####.### ...####.###", 12),
        0xD7: (-11, 1, "###...### .###.###. ..#####.. ...###... ..#####.. .###.###. ###...###",
               11),
        0xDF: (-15, 0, "..#######.. .#########. ####...#### ####...#### ####..####. "
                       "####.####.. ####.####.. ####..####. ####...#### ####...#### "
                       "####...#### ####..##### ####.###### ####.#####. ####.####..", 12),
        0xF0: (-15, 0, "..#####.... ...######## .....###### .......#### ..########. "
                       "########### ########### ####...#### ####...#### ####...#### "
                       "####...#### ####...#### ########### ..########. ..########.", 12),
    },
}


def from_art(face, code):
    yo, xo, rows, adv = ART[face.name][code]
    bm = Bm(art(rows, xo, yo))
    bm.adv = adv if adv is not None else (face.monospace or bm.maxx + 2)
    if face.monospace:
        bm.adv = face.monospace
    return bm


# ---------------------------------------------------------- derived glyphs ------

STEM = {"monogram": 1, "body": 1, "display": 4}  # vertical stroke weight
LIG_SHIFT = {"body": 2, "display": 7}             # E/e's stem over A/a's right stem


def spacing_row(face):
    """Bottom row of a spacing accent: where the TTF's own ¨ sits."""
    return face.g("¨").maxy if face.has(0xA8) else face.lc_acc_bottom


def place_spacing(face, px, bottom=None, top=None):
    """A spacing mark centred on the face's 'o', sized to its advance."""
    o = face.g("o")
    bm = Bm(px)
    dx = int(math.floor(centre_of(o) - centre_of(bm) + 0.5))
    dy = (bottom - bm.maxy) if bottom is not None else (top - bm.miny)
    out = bm.shifted(dx, dy)
    return finish(face, out, o.adv, o.maxx)


def bar_of(face, width):
    """A horizontal stroke `width` px wide, the face's '-' weight."""
    return {(x, y) for x in range(width) for y in range(face.bar)}


def derived(face, code):
    """Glyphs built from other glyphs of the face; None when not derivable here."""
    if code in ART[face.name]:
        return from_art(face, code)
    if code in ACCENTED:
        base, acc = ACCENTED[code]
        return compose(face, base, acc)
    if code == 0xA0:  # no-break space: the space's advance
        return Bm((), face.g(" ").adv)
    if code == 0xAD:  # soft hyphen: the face's hyphen
        return face.g("-")
    plus, minus = face.g("+"), face.g("-")
    if code == 0xAF:  # macron: the hyphen at the spacing-accent height
        return place_spacing(face, minus.px, bottom=spacing_row(face))
    if code == 0xB4:  # acute
        px, _ = accent(face, "acute", False)
        return place_spacing(face, px, bottom=spacing_row(face))
    if code == 0xB8:  # cedilla
        px, _ = accent(face, "cedilla", False)
        return place_spacing(face, px, top=0)
    if code == 0xB7:  # middle dot: '.' centred on the x-height
        dot = face.g(".")
        mid = (face.x_top - 1) / 2.0
        dy = int(math.floor(mid - (dot.miny + dot.maxy) / 2.0 + 0.5))
        out = dot.shifted(0, dy)
        out.adv = dot.adv
        return out
    if code == 0xAC:  # not sign: the plus's width as a bar, a hook at the right
        w = plus.maxx - plus.minx + 1
        stem = STEM[face.name]
        px = bar_of(face, w) | {(x, y) for x in range(w - stem, w)
                                for y in range(face.bar, 2 * face.bar)}
        out = Bm(px).shifted(plus.minx, minus.miny)
        return finish(face, out, plus.adv, plus.maxx)
    if code == 0xB1:  # plus-minus: the plus raised, a bar of its width at the baseline
        w = plus.maxx - plus.minx + 1
        bar = Bm(bar_of(face, w)).shifted(plus.minx, -face.bar)
        p = plus.shifted(0, (-face.bar - 2) - plus.maxy)
        return finish(face, Bm(p.px | bar.px), plus.adv, plus.maxx)
    if code == 0xD7:  # multiply: the face's x, centred on the plus
        x = face.g("x")
        dy = int(math.floor((plus.miny + plus.maxy) / 2.0 - (x.miny + x.maxy) / 2.0 + 0.5))
        dx = int(math.floor(centre_of(plus) - centre_of(x) + 0.5))
        return finish(face, x.shifted(dx, dy), plus.adv, plus.maxx)
    if code == 0xF7:  # divide: dot / bar / dot, centred on the hyphen
        dot = face.g(".")
        dw, dh = dot.maxx - dot.minx + 1, min(dot.maxy - dot.miny + 1, face.bar)
        w = plus.maxx - plus.minx + 1
        px = set()
        dxo = (w - dw) // 2
        px |= {(dxo + i, j) for i in range(dw) for j in range(dh)}
        px |= {(i, dh + 1 + j) for i in range(w) for j in range(face.bar)}
        px |= {(dxo + i, dh + 1 + face.bar + 1 + j) for i in range(dw) for j in range(dh)}
        bm = Bm(px)
        mid = (minus.miny + minus.maxy) / 2.0
        dy = int(math.floor(mid - bm.maxy / 2.0 + 0.5))
        out = bm.shifted(plus.minx, dy)
        if out.maxy > -1:
            out = out.shifted(0, -1 - out.maxy)
        return finish(face, out, plus.adv, plus.maxx)
    if code == 0xBF:  # inverted question mark: '?' turned 180°, hung like '¡'
        q = face.g("?")
        turned = {(q.minx + q.maxx - x, q.miny + q.maxy - y) for x, y in q.px}
        bm = Bm(turned)
        top = face.g("¡").miny if face.has(0xA1) else q.miny
        return finish(face, bm.shifted(0, top - bm.miny), q.adv, q.maxx)
    if code == 0xB5:  # micro: u with its left stem carried down to p's descender
        u, p = face.g("u"), face.g("p")
        top_row = [x for x, y in u.px if y == u.miny]
        stem = []
        for x in sorted(top_row):
            if not stem or x == stem[-1] + 1:
                stem.append(x)
            else:
                break
        px = set(u.px) | {(x, y) for x in stem for y in range(u.miny, p.maxy + 1)}
        return finish(face, Bm(px), u.adv, u.maxx)
    if code in (0xC6, 0xE6):  # Æ æ: A/a and E/e overlapped on the shared stem
        a, e = (face.g("A"), face.g("E")) if code == 0xC6 else (face.g("a"), face.g("e"))
        shift = LIG_SHIFT[face.name]
        bm = Bm(a.px | e.shifted(shift, 0).px)
        return finish(face, bm, e.adv + shift, e.maxx + shift)
    if code == 0xD0:  # Eth: D moved right, a crossbar through its stem
        d = face.g("D")
        stem_w = STEM[face.name]
        k = 1 if face.name != "display" else 2
        dd = d.shifted(k, 0)
        mid = (d.miny + d.maxy) // 2
        ys = range(mid - face.bar // 2, mid - face.bar // 2 + face.bar)
        right = d.minx + k + stem_w - 1 + (1 if face.name == "display" else 0)
        px = dd.px | {(x, y) for x in range(d.minx, right + 1) for y in ys}
        return finish(face, Bm(px), d.adv + k, d.maxx + k)
    if code in (0xD8, 0xF8):  # Ø ø: O/o slashed bottom-left to top-right
        o = face.g("O" if code == 0xD8 else "o")
        px = set(o.px)
        for t in range(3):
            px |= line(o.minx - 1 + t, o.maxy + 1, o.maxx - 1 + t, o.miny - 1)
        return finish(face, Bm(px), o.adv, o.maxx)
    if code == 0xDE:  # Thorn: P's bowl lowered on a full stem
        p = face.g("P")
        stem_w = STEM[face.name]
        drop = 1 if face.name != "display" else 2
        bowl = {(x, y) for x, y in p.px if x >= p.minx + stem_w}
        bowl_bottom = max(y for _, y in bowl)
        bowl = {(x, y + drop) for x, y in bowl}
        stem = {(x, y) for x in range(p.minx, p.minx + stem_w)
                for y in range(p.miny, p.maxy + 1)}
        assert bowl_bottom + drop <= p.maxy
        return finish(face, Bm(bowl | stem), p.adv, p.maxx)
    if code == 0xFE:  # thorn: p with b's ascender
        p, b = face.g("p"), face.g("b")
        asc = {(x, y) for x, y in b.px if y < p.miny}
        return finish(face, Bm(p.px | asc), p.adv, p.maxx)
    if code in (0xB9, 0xB2, 0xB3, 0xAA, 0xBA):  # display: small figures
        src = {0xB9: "1", 0xB2: "2", 0xB3: "3", 0xAA: "a", 0xBA: "o"}[code]
        small_ = small(src)
        top = face.g("1").miny
        if code in (0xAA, 0xBA):  # underlined
            w = small_.maxx + 1
            small_ = Bm(small_.px | {(x, y) for x in range(w)
                                     for y in range(small_.maxy + 2, small_.maxy + 4)})
        out = small_.shifted(1, top)
        out.adv = out.maxx + 3
        return out
    if code in (0xBC, 0xBD, 0xBE):  # display: small figures across a slash
        num = small("3" if code == 0xBE else "1")
        den = small({0xBC: "4", 0xBD: "2", 0xBE: "4"}[code])
        top = face.g("1").miny
        nw = num.maxx + 1
        dw, dh = den.maxx + 1, den.maxy + 1
        gap = 4
        W = nw + gap + dw
        px = {(x, y + top) for x, y in num.px}
        px |= {(x + nw + gap, y - dh) for x, y in den.px}
        slash = set()
        for t in range(2):
            slash |= line(t + 1, -1, W - 2 + t, top)
        clear = {(x + i, y + j) for x, y in px for i in (-1, 0, 1) for j in (-1, 0, 1)}
        px |= slash - clear
        out = Bm(px).shifted(1, 0)
        out.adv = out.maxx + 3
        return out
    return None


# --------------------------------------------------------------- writing ------

def glyph_block(code, bm):
    if not bm.px:
        return "U+%04X %s xAdvance=%d xOffset=0 yOffset=0" % (code, label_char(code), bm.adv)
    x0, y0 = bm.minx, bm.miny
    lines = ["U+%04X %s xAdvance=%d xOffset=%d yOffset=%d"
             % (code, label_char(code), bm.adv, x0, y0)]
    for y in range(y0, bm.maxy + 1):
        lines.append("".join("#" if (x, y) in bm.px else "." for x in range(x0, bm.maxx + 1)))
    return "\n".join(lines)


def label_char(code):
    if code in (0xA0, 0xAD):
        return {0xA0: "NBSP", 0xAD: "SHY"}[code]
    return chr(code)


def draft_face(face):
    out = {}
    for code in range(0xA0, 0x100):
        if face.has(code):
            continue
        bm = derived(face, code)
        if bm is None:
            raise SystemExit("%s: no recipe for U+%04X" % (face.name, code))
        if bm.px and bm.miny < -face.ascent:
            raise SystemExit("%s: U+%04X rises above the line top" % (face.name, code))
        out[code] = bm
    return out


def write_face(face, glyphs, path):
    from_ttf = [c for c in range(0xA0, 0x100) if face.has(c)]
    head = [
        "# DRAFT — awaiting the owner's review (#476)",
        "#",
        "# %s: the Latin-1 glyphs %s lacks, drafted from the face's own"
        % (face.name, os.path.basename(face.path)),
        "# letters by tools/draft_latin1.py (a one-shot helper). This file is now the source:",
        "# edit it by hand; ttf2enjin.py --glyphs merges it over the TTF's raster.",
        "#",
        "# One block per glyph, blank line between blocks:",
        "#   U+XXXX <char> xAdvance=N xOffset=N yOffset=N   (yOffset = top row vs the baseline)",
        "#   rows of '.'/'#' (width = row length, height = number of rows; none = no ink)",
        "# Line top = baseline - %d (ascent); nothing may sit above it. Cap height %d, x-height %d."
        % (face.ascent, -face.cap_top, -face.x_top),
        "# From the TTF (not here): %s" % " ".join("U+%04X" % c for c in from_ttf),
        "",
    ]
    body = "\n\n".join(glyph_block(c, glyphs[c]) for c in sorted(glyphs))
    with open(path, "w") as f:
        f.write("\n".join(head) + "\n" + body + "\n")


def main(argv=None):
    ap = argparse.ArgumentParser(description=__doc__.split("\n")[0])
    ap.add_argument("--face", choices=sorted(FACES), action="append")
    ap.add_argument("--fonts-dir", default="pixel_fonts")
    ap.add_argument("--out-dir", default=os.path.join(HERE, "faces"))
    ap.add_argument("--force", action="store_true",
                    help="overwrite an existing (possibly hand-edited) glyph file")
    args = ap.parse_args(argv)
    os.makedirs(args.out_dir, exist_ok=True)
    for name in args.face or sorted(FACES):
        face = Face(name, args.fonts_dir)
        path = os.path.join(args.out_dir, "%s.latin1.txt" % name)
        if os.path.exists(path) and not args.force:
            raise SystemExit("draft_latin1: %s exists (the owner's source); --force to "
                             "overwrite it" % path)
        glyphs = draft_face(face)
        write_face(face, glyphs, path)
        sys.stderr.write("draft_latin1: %s: %d drafted -> %s\n" % (name, len(glyphs), path))
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
