#!/usr/bin/env python3
# Draws res/808Metal.svg. VCV Rack can't draw SVG <text>, so every label is
# turned into outline paths with fontTools. Positions are in mm and must match
# src/Metal808.cpp (the numbers in the LAYOUT block are the same in both).
#   pip install fonttools
#   python3 res/make_panel.py
import os
from fontTools.ttLib import TTFont
from fontTools.pens.svgPathPen import SVGPathPen
from fontTools.pens.transformPen import TransformPen

HERE = os.path.dirname(os.path.abspath(__file__))
FONT_BOLD = os.environ.get("PANEL_FONT_BOLD", "/usr/share/fonts/opentype/inter/Inter-Bold.otf")
FONT_MED = os.environ.get("PANEL_FONT_MED", "/usr/share/fonts/opentype/inter/Inter-Medium.otf")

W, H = 121.92, 128.5  # 24 HP

BG = "#1F2023"
BG_DARK = "#16171A"
LINE = "#3A3B40"
INK = "#ECE6D8"
SOFT = "#9A968C"
BRASS = "#D9B45A"

# ---- LAYOUT (same numbers in src/Metal808.cpp) ----
COL = [11.0, 31.0, 51.0, 71.0, 91.0, 111.0]
ROW_OSC, ROW_FLT, ROW_ENV, ROW_CV, ROW_IO = 30.0, 51.0, 72.0, 94.5, 113.5

CONTROL = 3.2   # control label size (project standard)
JACK = 2.9      # jack label size (project standard)
SMALL = 2.4

fonts = {}


def text(s, x, y, size, font=FONT_BOLD, fill=INK, anchor="middle", spacing=0.04):
    """SVG path for string s; (x, y) is the baseline point in mm."""
    if font not in fonts:
        f = TTFont(font)
        fonts[font] = (f, f.getGlyphSet(), f.getBestCmap(), f["head"].unitsPerEm)
    f, gs, cmap, upm = fonts[font]
    scale = size / upm
    track = spacing * upm
    names = [cmap[ord(c)] for c in s]
    width = sum(gs[n].width + track for n in names) - track
    x0 = {"middle": x - width * scale / 2, "end": x - width * scale}.get(anchor, x)
    pen = SVGPathPen(gs)
    cx = 0.0
    for n in names:
        gs[n].draw(TransformPen(pen, (scale, 0, 0, -scale, x0 + cx * scale, y)))
        cx += gs[n].width + track
    text.width = width * scale
    return f'<path fill="{fill}" d="{pen.getCommands()}"/>'


parts = []
add = parts.append
add(f'<rect width="{W}" height="{H}" fill="{BG}"/>')

# title + six little square waves (the six oscillators)
add(text("808 METAL", 6.0, 11.6, 5.0, FONT_BOLD, INK, anchor="start", spacing=0.08))
x = 41.5
for i, (w, h) in enumerate([(1.4, 2.4), (1.8, 2.4), (1.2, 2.4), (2.2, 2.4), (1.0, 2.4), (1.6, 2.4)]):
    d = f"M{x} 10.6 v-{h} h{w} v{h} h{w} v-{h} h{w} v{h}"
    add(f'<path d="{d}" stroke="{BRASS}" stroke-width="0.45" fill="none" stroke-linejoin="round"/>')
    x += 3 * w + 1.6
add(text("TR-808 cymbal model", W - 6.0, 11.2, SMALL, FONT_MED, SOFT, anchor="end"))


def section(name, y):
    """Small heading with a rule, sitting above a row whose knobs are at y."""
    hy = y - 12.6
    add(text(name, 4.0, hy, SMALL, FONT_BOLD, BRASS, anchor="start", spacing=0.1))
    add(f'<line x1="{4.0 + text.width + 2.0:.2f}" y1="{hy - 0.85}" x2="{W - 4.0}" y2="{hy - 0.85}" stroke="{LINE}" stroke-width="0.3"/>')


LBL = 7.8   # label baseline above a knob/jack centre

section("OSCILLATORS", ROW_OSC)
for x, s in zip(COL, ["TUNE", "TRIM A", "TRIM B", "SPREAD", "WIDTH", "DRIFT"]):
    add(text(s, x, ROW_OSC - LBL, CONTROL))

section("FILTERS", ROW_FLT)
for x, s in zip(COL[:4], ["COLOUR", "RES", "BAND", "CUT"]):
    add(text(s, x, ROW_FLT - LBL, CONTROL))
add(f'<rect x="{COL[4] - 9.0}" y="{ROW_FLT - 11.6}" width="{COL[5] - COL[4] + 18.0}" height="17.2" rx="1.4" fill="{BG_DARK}"/>')
add(text("EXT IN", COL[4], ROW_FLT - LBL + 0.6, JACK))
add(text("OSC OUT", COL[5], ROW_FLT - LBL + 0.6, JACK))

section("ENVELOPE", ROW_ENV)
for x, s in zip(COL, ["DECAY", "MODE", "TONE", "DRIVE", "LEVEL", "HIT"]):
    add(text(s, x, ROW_ENV - LBL, CONTROL))
# mode positions (CKSSThree: top = CY, middle = OH, bottom = CH)
for dy, s in [(-3.4, "CY"), (0.0, "OH"), (3.4, "CH")]:
    add(text(s, COL[1] + 4.2, ROW_ENV + dy + 0.85, SMALL, FONT_MED, SOFT, anchor="start"))
# scale hints
add(text("3.4k", COL[2] - 6.6, ROW_FLT + 5.6, 2.2, FONT_MED, SOFT))
add(text("7.1k", COL[2] + 6.6, ROW_FLT + 5.6, 2.2, FONT_MED, SOFT))
add(text("low", COL[2] - 6.4, ROW_ENV + 6.4, 2.2, FONT_MED, SOFT))
add(text("high", COL[2] + 6.6, ROW_ENV + 6.4, 2.2, FONT_MED, SOFT))

section("CV IN", ROW_CV)
for x, s in zip(COL, ["V/OCT", "COLOUR", "CUT", "DECAY", "TONE", "DRIVE"]):
    add(text(s, x, ROW_CV - LBL + 0.6, JACK))

add(f'<line x1="4.0" y1="{ROW_IO - 12.6 - 0.85}" x2="{W - 4.0}" y2="{ROW_IO - 12.6 - 0.85}" stroke="{LINE}" stroke-width="0.3"/>')
for x, s in zip(COL[:3], ["TRIG", "ACCENT", "CHOKE"]):
    add(text(s, x, ROW_IO - LBL + 0.6, JACK))
add(f'<rect x="{COL[5] - 8.0}" y="{ROW_IO - 11.0}" width="16.0" height="17.0" rx="1.4" fill="{BRASS}"/>')
add(text("OUT", COL[5], ROW_IO - LBL + 0.6, JACK, fill=BG_DARK))
# a small signal-flow strip in the free space
fy = ROW_IO - 1.0
fx = COL[3] - 7.5
for i, s in enumerate(["OSC", "BPF", "VCA", "HPF"]):
    xx = fx + i * 9.6
    add(text(s, xx, fy + 0.9, 2.2, FONT_MED, SOFT))
    if i < 3:
        add(f'<path d="M{xx + 3.6} {fy} h2.4 m-0.9 -0.7 l0.9 0.7 l-0.9 0.7" stroke="{SOFT}" stroke-width="0.3" fill="none"/>')
add(text("after mjsyts, scsynth.org", (COL[3] + COL[4]) / 2 + 3.0, fy + 5.6, 2.0, FONT_MED, SOFT))

svg = (f'<svg xmlns="http://www.w3.org/2000/svg" width="{W}mm" height="{H}mm" viewBox="0 0 {W} {H}">\n'
       + "\n".join(parts) + "\n</svg>\n")
open(os.path.join(HERE, "808Metal.svg"), "w").write(svg)
print("ok")

# --- Haer maker logo (res/haer_logo.py) on the bottom rail
import sys as _sys
_sys.path.insert(0, HERE)
import haer_logo
haer_logo.add_to_file(os.path.join(HERE, "808Metal.svg"), x=W / 2, y=126.0, cap=3.0, bg=BG)
