"""Bank Respublika logo + "Retail Loan Team" as 2D outlines (mm) for the LCD enclosure. Helper for make_enclosure_lcd.py.

The logo comes from brand/BR_digital_logo.svg (supplied by the user, 14 outline paths, no font name inside).
The team name is set in Outfit SemiBold (SIL OFL, brand/Outfit_600SemiBold.ttf), chosen BY EYE as the closest free font to the
logo lettering (single-storey 'a', round geometric forms). It is NOT the bank's official brand font.
All shapes are returned in (u, v) mm: u = to the right, v = up, centred on (0, 0).
"""
import pathlib
import re

from fontTools.pens.basePen import BasePen
from fontTools.ttLib import TTFont
from manifold3d import CrossSection, FillRule
from svgpathtools import svg2paths

HERE = pathlib.Path(__file__).resolve().parent / 'brand'
SEG = 10  # samples per curve


def _polys_from_svg(svg):
    paths, _ = svg2paths(str(svg))
    out = []
    for p in paths:
        for sub in p.continuous_subpaths():
            pts = []
            for seg in sub:
                n = 1 if seg.__class__.__name__ == 'Line' else SEG
                for i in range(n):
                    z = seg.point(i / n)
                    pts.append((z.real, -z.imag))        # SVG y is down
            if len(pts) >= 3:
                out.append(pts)
    return out


class _Flat(BasePen):
    def __init__(self, gs):
        super().__init__(gs)
        self.contours, self.cur = [], []

    def _moveTo(self, p):
        self.cur = [p]

    def _lineTo(self, p):
        self.cur.append(p)

    def _curveToOne(self, a, b, c):
        p0 = self.cur[-1]
        for i in range(1, SEG + 1):
            t = i / SEG
            m = 1 - t
            self.cur.append((m**3 * p0[0] + 3 * m * m * t * a[0] + 3 * m * t * t * b[0] + t**3 * c[0],
                             m**3 * p0[1] + 3 * m * m * t * a[1] + 3 * m * t * t * b[1] + t**3 * c[1]))

    def _qCurveToOne(self, a, b):
        p0 = self.cur[-1]
        for i in range(1, SEG + 1):
            t = i / SEG
            m = 1 - t
            self.cur.append((m * m * p0[0] + 2 * m * t * a[0] + t * t * b[0], m * m * p0[1] + 2 * m * t * a[1] + t * t * b[1]))

    def _closePath(self):
        if len(self.cur) >= 3:
            self.contours.append(self.cur)
        self.cur = []

    _endPath = _closePath


def _polys_from_text(text, ttf, tracking=0.0):
    f = TTFont(str(ttf))
    gs, cmap, hmtx = f.getGlyphSet(), f.getBestCmap(), f['hmtx']
    upm = f['head'].unitsPerEm
    x, out = 0.0, []
    for ch in text:
        name = cmap[ord(ch)]
        pen = _Flat(gs)
        gs[name].draw(pen)
        for c in pen.contours:
            out.append([(px + x, py) for px, py in c])
        x += hmtx[name][0] + tracking * upm
    return [[(px / upm, py / upm) for px, py in c] for c in out]   # units of em


def _cs(polys):
    return CrossSection(polys, FillRule.NonZero)


def _fit(cs, width=None, cap=None, scale=None):
    x0, y0, x1, y1 = cs.bounds()
    s = scale if scale else width / (x1 - x0)
    cs = cs.scale([s, s])
    x0, y0, x1, y1 = cs.bounds()
    return cs.translate([-(x0 + x1) / 2, -(y0 + y1) / 2])


def logo(width=39.4):
    """Bank / Respublika, centred, `width` mm wide."""
    return _fit(_cs(_polys_from_svg(HERE / 'BR_digital_logo.svg')), width=width)


def team_text(width=48.0, text='Retail Loan Team'):
    """Single line, centred, `width` mm wide (Outfit SemiBold)."""
    return _fit(_cs(_polys_from_text(text, HERE / 'Outfit_600SemiBold.ttf', tracking=0.01)), width=width)


if __name__ == '__main__':
    for n, c in (('logo', logo()), ('text', team_text())):
        print(n, [round(v, 2) for v in c.bounds()], 'area', round(c.area(), 1))
