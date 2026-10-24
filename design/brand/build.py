"""Generate editable artboards and transparent lockups. Requires Pillow for PNGs."""
from pathlib import Path
import io
import json
import random
import subprocess
from PIL import Image
from fontTools.pens.svgPathPen import SVGPathPen
from fontTools.ttLib import TTFont
from fontTools.varLib.instancer import instantiateVariableFont

ROOT = Path(__file__).resolve().parent
ASSETS = ROOT / 'assets'
ARTWORK = ROOT / 'artwork'
EXPORTS = ROOT / 'exports'
for directory in (ARTWORK, EXPORTS):
    directory.mkdir(exist_ok=True)

# Static original grain, drawn behind content rather than over text.
rng = random.Random(17)
grain = Image.new('LA', (256, 256))
grain.putdata([(rng.randrange(256), 12) for _ in range(256 * 256)])
grain.save(ASSETS / 'grain.png')

font = instantiateVariableFont(TTFont(ASSETS / 'Outfit.ttf'), {'wght': 800})
glyphs = font.getGlyphSet()
cmap = font.getBestCmap()
cap = font['OS/2'].sCapHeight
pad = 40
baseline = pad + cap
height = cap - font['hhea'].descent + 2 * pad
cursor = pad
paths = []
for char in 'myArc':
    name = cmap[ord(char)]
    advance = font['hmtx'][name][0]
    if char == 'A':
        scale = cap / 80
        left = cursor + (advance - cap) / 2
        paths.append(f'<path d="M7 76 L37 8 Q40 2 43 8 L73 76" transform="translate({left:g} {pad}) scale({scale:g})" fill="none" stroke="currentColor" stroke-width="18" stroke-linejoin="round"/>')
    else:
        pen = SVGPathPen(glyphs)
        glyphs[name].draw(pen)
        paths.append(f'<path d="{pen.getCommands()}" transform="translate({cursor:g} {baseline}) scale(1 -1)"/>')
    cursor += advance - 35
width = cursor + 35 + pad
wordmarks = {}
for finish, color in (('ink', '#182432'), ('ivory', '#f5f2ec')):
    svg = f'<svg xmlns="http://www.w3.org/2000/svg" width="{width}" height="{height}" viewBox="0 0 {width} {height}" fill="{color}" color="{color}"><title>myArc, Outfit ExtraBold with custom A</title>{"".join(paths)}</svg>\n'
    (ASSETS / f'wordmark-{finish}.svg').write_text(svg)
    rendered = subprocess.run(['rsvg-convert', '--height', '320'], input=svg.encode(), stdout=subprocess.PIPE, check=True)
    wordmarks[finish] = Image.open(io.BytesIO(rendered.stdout)).convert('RGBA')
    assert wordmarks[finish].width <= 1010, 'Wordmark exceeds lockup clear space'

CSS = '''
@font-face{font-family:Outfit;src:url('../assets/Outfit.ttf');font-weight:100 900}
*{box-sizing:border-box}html{font-size:100%}html,body{margin:0}body{font-family:Outfit,sans-serif;font-size:1rem}
.art{position:relative;overflow:hidden;background:#f5f2ec;color:#182432}
.word{font-weight:800;letter-spacing:-.035em;line-height:1}
.word img{display:block;height:1em;width:auto}.desktop .word img,.phone .word img{margin:auto}
.top{position:absolute;top:3.25rem;left:4rem;right:4rem}
.top .word{font-size:2.25rem}
.dark{background:#101c2b;color:#f5f2ec}
h1{font-size:5.5rem;line-height:1.02;letter-spacing:-.045em;font-weight:600;margin:0}
.social .headline{position:absolute;top:14.0625rem;left:4rem;z-index:1}
.social .mark{position:absolute;width:27.5rem;height:27.5rem;right:3.5rem;top:5.9375rem}
.orbit{position:absolute;width:40rem;height:40rem;border:.0625rem solid #b8a8b130;border-radius:50%;right:-3.125rem;top:-.5625rem}
.square .top{top:4.0625rem;left:4.5rem;right:4.5rem}.square .top .word{font-size:2.75rem}
.square .mark{position:absolute;width:35.625rem;height:35.625rem;top:8.75rem;left:15.9375rem}
.square h1{position:absolute;left:4.5rem;bottom:6rem;font-size:6rem}
.desktop{background:radial-gradient(ellipse at 52% 49%,#263e59 0%,#101c2b 58%)}
.desktop .mark{position:absolute;width:66.25rem;height:66.25rem;left:46.875rem;top:12.5rem}
.desktop .word{position:absolute;bottom:10rem;width:100%;text-align:center;font-size:5.875rem}
.desktop .tagline{position:absolute;bottom:6.75rem;width:100%;text-align:center;font-size:2.25rem;color:#b4c1ce}
.desktop .ring{position:absolute;width:83.125rem;height:83.125rem;left:38.4375rem;top:8.4375rem;border:.0625rem solid #95b6d51c;border-radius:50%}
.light{background:#f5f2ec;color:#182432}.desktop.light .tagline{color:#54616d}.desktop.light .ring{border-color:#18243212}
.phone{background:radial-gradient(ellipse at 50% 50%,#294361 0%,#101c2b 65%);color:#f5f2ec}
.phone .mark{position:absolute;width:71.875rem;height:71.875rem;left:9.0625rem;top:46.25rem}
.phone .word{position:absolute;bottom:26.875rem;width:100%;text-align:center;font-size:8rem}
.phone p{position:absolute;bottom:19.375rem;width:100%;text-align:center;font-size:2.5rem;color:#c3ced9}
.phone .ring{position:absolute;width:81.25rem;height:81.25rem;left:4.375rem;top:41.5625rem;border:.125rem solid #95b6d51c;border-radius:50%}
'''

logo = lambda finish: f'<img class="mark" src="../assets/myarc-{finish}.png" alt="myArc {finish} logo">'
wordmark = lambda finish: f'<div class="word"><img src="../assets/wordmark-{finish}.svg" alt="myArc"></div>'
top = lambda finish: f'<div class="top">{wordmark(finish)}</div>'
ART = {
    'social-dark': (1200, 630, 'social dark', top('ivory') + '<div class="orbit"></div><div class="headline"><h1>Arc, forever<br>yours.</h1></div>' + logo('glass')),
    'social-light': (1200, 630, 'social', top('ink') + '<div class="orbit"></div><div class="headline"><h1>Arc, forever<br>yours.</h1></div>' + logo('matte')),
    'social-square': (1080, 1080, 'square', top('ink') + logo('glass') + '<h1>Arc, forever<br>yours.</h1>'),
    'wallpaper-dark': (2560, 1600, 'desktop dark', '<div class="ring"></div>' + logo('glass') + wordmark('ivory') + '<div class="tagline">Arc, forever yours.</div>'),
    'wallpaper-light': (2560, 1600, 'desktop light', '<div class="ring"></div>' + logo('matte') + wordmark('ink') + '<div class="tagline">Arc, forever yours.</div>'),
    'wallpaper-phone': (1440, 2560, 'phone', '<div class="ring"></div>' + logo('glass') + wordmark('ivory') + '<p>Arc, forever yours.</p>'),
}

manifest = []
for name, (width, height, classes, content) in ART.items():
    html = f'''<!doctype html>
<html lang="en"><head><meta charset="utf-8"><title>myArc / {name}</title>
<style>{CSS}</style></head><body><main class="art {classes}" style="width:{width / 16:g}rem;height:{height / 16:g}rem">{content}</main></body></html>'''
    (ARTWORK / f'{name}.html').write_text(html)
    manifest.append({'name': name, 'width': width, 'height': height})

for finish, wordmark_finish in (('matte', 'ink'), ('glass', 'ivory')):
    # Keep original logo proportions and full canvas, including transparent margin.
    mark = Image.open(ASSETS / f'myarc-{finish}.png').convert('RGBA')
    lockup = Image.new('RGBA', (1550, 480))
    lockup.alpha_composite(mark.resize((400, 400), Image.Resampling.LANCZOS), (40, 40))
    lockup.alpha_composite(wordmarks[wordmark_finish], (500, 80))
    lockup.save(EXPORTS / f'lockup-{finish}.png')
    for size in (128, 256, 512, 1024):
        mark.resize((size, size), Image.Resampling.LANCZOS).save(EXPORTS / f'mark-{finish}-{size}.png')

(ROOT / 'artwork' / 'manifest.json').write_text(json.dumps(manifest, indent=2) + '\n')
assert len(manifest) == 6
for finish in ('matte', 'glass'):
    assert Image.open(EXPORTS / f'lockup-{finish}.png').size == (1550, 480)
    assert Image.open(EXPORTS / f'mark-{finish}-128.png').mode == 'RGBA'
print('Generated 6 editable artboards, 2 transparent lockups, 8 icon PNGs')
