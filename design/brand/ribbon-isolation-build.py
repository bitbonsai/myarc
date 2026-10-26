#!/usr/bin/env python3
"""Ribbon isolation study: reconstruct the horizontal twisted ribbon of the
myArc mark without the A, export small transparent PNGs and a study page.

Vector geometry is a hand trace over design/brand/assets/myarc-matte.png in
its own 1254-pixel coordinate space. It is a reconstruction, not extracted
pixels. Run from repository root; needs rsvg-convert and ImageMagick."""
import base64, pathlib, subprocess, sys

ROOT = pathlib.Path(__file__).resolve().parents[2]
BRAND = ROOT / "design/brand"
ASSETS = BRAND / "assets"
OUT = BRAND / "exports/ribbon-isolation"
OUT.mkdir(parents=True, exist_ok=True)
ORIGINAL = ASSETS / "myarc-matte.png"

# Edge trace, original pixel coordinates. Front band wraps the left curl
# outer arc and the lower half of the right curl; back band owns the upper
# half of the right curl and tucks inside the left curl.
FRONT = """M110,772
C60,740 38,650 55,580
C70,520 110,462 170,448
C220,433 300,430 376,462
C450,490 520,560 604,631
C680,700 760,760 866,773
C920,780 980,780 1003,773
C1050,766 1095,748 1120,728
C1146,760 1152,830 1122,880
C1095,925 1060,952 1026,950
C980,975 940,975 912,973
C870,972 830,968 798,961
C760,950 720,935 684,916
C630,890 590,860 547,836
C510,810 480,785 456,756
C420,720 380,680 342,659
C300,640 260,620 228,625
C190,630 140,640 105,642
C90,660 80,700 90,735
C95,750 100,765 110,772Z"""

BACK = """M75,700
C78,660 82,640 92,634
C140,618 190,612 228,614
C290,620 320,632 342,648
C400,700 430,718 456,720
C500,732 530,737 560,735
C600,728 620,720 640,710
C700,690 760,670 800,652
C840,630 870,610 900,590
C940,560 980,542 1010,538
C1040,528 1060,525 1080,526
C1160,528 1230,560 1246,650
C1256,720 1230,820 1170,880
C1150,898 1130,905 1112,896
C1136,860 1142,800 1136,740
C1132,700 1126,676 1112,668
C1100,662 1080,664 1065,672
C1040,680 1020,686 1000,690
C950,700 910,710 880,722
C840,740 800,760 760,782
C720,802 680,822 640,842
C600,860 560,880 520,890
C480,900 440,906 400,905
C330,900 230,860 150,815
C110,792 85,765 78,740
C74,725 74,712 75,700Z"""

FRONT_STOPS = [(48,"#10aefc"),(260,"#22adfc"),(400,"#2a98fd"),(520,"#4593fb"),
    (620,"#5f8ffb"),(720,"#956efa"),(830,"#cc6dfa"),(950,"#f745d2"),
    (1060,"#fb78b1"),(1150,"#fba5ae"),(1248,"#fbbeae")]
BACK_STOPS = [(48,"#0a92f8"),(130,"#0a5ce8"),(200,"#1040ea"),(300,"#2a36f6"),
    (450,"#3a44f8"),(600,"#4458fa"),(700,"#465dfb"),(820,"#8a6ff5"),
    (920,"#e07fc0"),(1010,"#fb88ad"),(1100,"#fba8ad"),(1248,"#fbbeae")]

def stops(lst):
    return "".join(f'<stop offset="{(x-48)/1200:.4f}" stop-color="{c}"/>' for x,c in lst)

def defs(prefix=""):
    return f"""<defs>
<linearGradient id="{prefix}front" gradientUnits="userSpaceOnUse" x1="48" y1="0" x2="1248" y2="0">{stops(FRONT_STOPS)}</linearGradient>
<linearGradient id="{prefix}back" gradientUnits="userSpaceOnUse" x1="48" y1="0" x2="1248" y2="0">{stops(BACK_STOPS)}</linearGradient>
<clipPath id="{prefix}backclip"><path d="{BACK}"/></clipPath>
<filter id="{prefix}soft" x="-20%" y="-20%" width="140%" height="140%"><feGaussianBlur stdDeviation="22"/></filter>
</defs>"""

def body(prefix="", shadow=True):
    s = f'<path d="{BACK}" fill="url(#{prefix}back)"/>'
    if shadow:
        # contact shadow of the front band on the back band gives depth
        s += (f'<g clip-path="url(#{prefix}backclip)"><path d="{FRONT}" transform="translate(0,26)" '
              f'fill="#0b1420" opacity="0.28" filter="url(#{prefix}soft)"/></g>')
    s += f'<path d="{FRONT}" fill="url(#{prefix}front)"/>'
    return s

# Ribbon bounds in original space
BX0, BY0, BX1, BY1 = 38, 430, 1250, 976
BW, BH = BX1-BX0, BY1-BY0

def svg_master():
    return (f'<svg xmlns="http://www.w3.org/2000/svg" viewBox="0 0 1254 1254" width="1254" height="1254">'
            f'<title>myArc ribbon isolation study, reconstruction</title>{defs()}{body()}</svg>')

def svg_icon(angle=0.0, pad=0.04):
    """Square icon artwork. Ribbon fitted into a 1000-unit box, optional tilt."""
    import math
    cx, cy = (BX0+BX1)/2, (BY0+BY1)/2
    a = math.radians(angle)
    w = BW*abs(math.cos(a)) + BH*abs(math.sin(a))
    h = BW*abs(math.sin(a)) + BH*abs(math.cos(a))
    box = 1000
    scale = box*(1-2*pad)/max(w, h)
    t = (f'translate({box/2} {box/2}) scale({scale:.5f}) rotate({angle}) translate({-cx} {-cy})')
    return (f'<svg xmlns="http://www.w3.org/2000/svg" viewBox="0 0 {box} {box}">'
            f'{defs()}<g transform="{t}">{body()}</g></svg>')

def run(*cmd):
    subprocess.run(cmd, check=True)

master = ASSETS / "ribbon-isolation-source.svg"
master.write_text(svg_master())
flat = ASSETS / "ribbon-isolation-icon.svg"
flat.write_text(svg_icon(0))
TILT = -20
tilted = ASSETS / "ribbon-isolation-icon-tilted.svg"
tilted.write_text(svg_icon(TILT))

sizes = [16, 24, 32, 48]
for name, src in (("flat", flat), ("tilted", tilted)):
    for s in sizes:
        run("rsvg-convert", "-w", str(s), "-h", str(s), str(src), "-o", str(OUT/f"ribbon-isolation-{name}-{s}.png"))
    run("rsvg-convert", "-w", "512", "-h", "512", str(src), "-o", str(OUT/f"ribbon-isolation-{name}-512.png"))
run("rsvg-convert", "-w", "627", "-h", "627", str(master), "-o", str(OUT/"ribbon-isolation-reconstruction-627.png"))
for s in (32, 48, 627):
    run("magick", str(ORIGINAL), "-filter", "Lanczos", "-resize", f"{s}x{s}", str(OUT/f"ribbon-isolation-full-logo-{s}.png"))
# ghost overlay: original at 35 percent under the reconstruction, for trace judgement
run("magick", str(OUT/"ribbon-isolation-full-logo-627.png"), "-channel", "A", "-evaluate", "multiply", "0.35", "+channel",
    str(OUT/"ribbon-isolation-reconstruction-627.png"), "-composite", str(OUT/"ribbon-isolation-overlay-627.png"))
print("exports ok", OUT)
