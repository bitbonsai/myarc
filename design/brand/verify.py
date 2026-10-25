"""Check local kit links, dimensions, transparency and visibly nonempty exports."""
from html.parser import HTMLParser
from pathlib import Path
from urllib.parse import unquote, urlsplit
import hashlib
import json
import re
from xml.etree import ElementTree
from PIL import Image, ImageStat
from fontTools.pens.boundsPen import BoundsPen
from fontTools.svgLib.path import parse_path

ROOT = Path(__file__).resolve().parent


class Links(HTMLParser):
    def __init__(self, path):
        super().__init__()
        self.path = path

    def handle_starttag(self, tag, attrs):
        for key, value in attrs:
            if key not in ('src', 'href') or not value:
                continue
            url = urlsplit(value)
            if url.scheme or not url.path or url.path.endswith('.zip'):
                continue
            target = self.path.parent / unquote(url.path)
            assert target.is_file(), f'Missing local asset: {target}'
            if key == 'src' and target.suffix in ('.png', '.svg'):
                expected = 'v=' + hashlib.sha256(target.read_bytes()).hexdigest()[:12]
                assert url.query == expected, f'Stale image revision: {target}'


for path in [ROOT / 'index.html', ROOT / 'preview.html', ROOT / 'type-study.html', ROOT / 'alignment.html', *sorted((ROOT / 'artwork').glob('*.html'))]:
    source = path.read_text()
    Links(path).feed(source)
    if path.name != 'type-study.html':
        assert 'Manrope' not in source, f'Outdated brand typeface: {path}'
        assert re.search(r'font-family:\s*Outfit', source), f'Use Outfit: {path}'
    for size in re.findall(r'font-size\s*:\s*([^;}]+)', source):
        size = size.strip()
        if size == '100%':
            continue
        assert re.fullmatch(r'\d*\.?\d+rem', size), f'Use rem font sizes: {path}: {size}'
        assert float(size[:-3]) >= 0.875, f'Font below minimum: {path}: {size}'
    text = re.sub(r'<style\b[^>]*>.*?</style>', '', source, flags=re.S)
    text = re.sub(r'<[^>]+>', ' ', text)
    assert not re.search(r'\bmyarc\b', text), f'Use myArc branding: {path}'
    assert not re.search(r'\bArc,\s+forever', text), f'Use the full myArc tagline: {path}'
    for old_copy in ('The web,', 'made yours.', 'Your browsing space,', 'Your browser.'):
        assert old_copy not in text, f'Outdated copy: {path}'

page = (ROOT / 'index.html').read_text()
heart = ElementTree.fromstring(re.search(r'<svg class="heart".*?</svg>', page, re.S).group(0))
licensed_heart = ElementTree.parse(ROOT / 'assets' / 'heart.svg').getroot()
assert heart.attrib['fill'] == 'none' and heart.attrib['aria-label'] == 'love'
assert heart.find('{http://www.w3.org/2000/svg}path').attrib['d'] == licensed_heart.find('{http://www.w3.org/2000/svg}path').attrib['d']
assert 'href="https://github.com/bitbonsai"' in page
assert 'Project in planning.' not in page

ink = (ROOT / 'assets' / 'wordmark-ink.svg').read_text()
ivory = (ROOT / 'assets' / 'wordmark-ivory.svg').read_text()
assert ink.replace('#182432', 'COLOR') == ivory.replace('#f5f2ec', 'COLOR')
svg = ElementTree.fromstring(ink)
ns = {'svg': 'http://www.w3.org/2000/svg'}
assert len(svg.findall('svg:path', ns)) == 5
assert not svg.findall('.//svg:text', ns), 'Wordmark must remain font-independent'
paths = svg.findall('svg:path', ns)
a = paths[2]
d = a.attrib['d']
assert 'stroke' not in a.attrib and d.endswith('Z'), 'A must be a closed, filled outline'
assert d.startswith('M0 80') and 'L80 80 L60 80' in d and d.endswith('L20 80 Z'), 'A feet must be horizontal'
bounds = BoundsPen(None)
parse_path(d, bounds)
assert bounds.bounds == (0, 0, 80, 80), 'A must fit its cap-height/baseline box'
_, offset_y, scale = map(float, re.fullmatch(r'translate\(([-\d.]+) ([-\d.]+)\) scale\(([-\d.]+)\)', a.attrib['transform']).groups())
a_baseline = offset_y + scale * bounds.bounds[3]
for neighbor in (paths[0], paths[3]):
    bounds = BoundsPen(None)
    parse_path(neighbor.attrib['d'], bounds)
    offset_y = float(re.search(r'translate\([^ ]+ ([^)]+)\)', neighbor.attrib['transform']).group(1))
    assert abs(a_baseline - (offset_y - bounds.bounds[1])) < 0.01, 'A baseline differs from m/r'
reference_svg = re.findall(r'<svg\b.*?</svg>', (ROOT / 'alignment.html').read_text(), re.S)[0]
reference_a = ElementTree.fromstring(reference_svg).findall('svg:path', ns)[2]
bounds = BoundsPen(None)
parse_path(reference_a.attrib['d'], bounds)
reference_y = float(re.search(r'translate\([^ ]+ ([^)]+)\)', reference_a.attrib['transform']).group(1))
assert abs(a_baseline - (reference_y - bounds.bounds[1])) < 0.01, 'Custom A baseline differs from original A'
assert abs((a_baseline - 80 * scale) - (reference_y - bounds.bounds[3])) < 0.01, 'Custom A cap height differs from original A'
with Image.open(ROOT / 'assets' / 'grain.png') as image:
    assert image.size == (256, 256) and image.getextrema()[-1] == (12, 12)
    assert ImageStat.Stat(image.getchannel('L')).stddev[0] > 50

for art in json.loads((ROOT / 'artwork' / 'manifest.json').read_text()):
    path = ROOT / 'exports' / f"{art['name']}.png"
    with Image.open(path) as image:
        assert image.size == (art['width'], art['height']), path
        assert min(ImageStat.Stat(image.convert('RGB')).stddev) > 10, path

for finish in ('matte', 'glass'):
    with Image.open(ROOT / 'assets' / f'myarc-{finish}.png') as image:
        assert image.size == (1254, 1254) and image.mode == 'RGBA'
    with Image.open(ROOT / 'exports' / f'lockup-{finish}.png') as image:
        assert image.size == (1550, 480) and image.getextrema()[3] == (0, 255)
    for size in (128, 256, 512, 1024):
        with Image.open(ROOT / 'exports' / f'mark-{finish}-{size}.png') as image:
            assert image.size == (size, size) and image.getextrema()[3] == (0, 255)

for color in ('blue', 'ivory'):
    small = ElementTree.parse(ROOT / 'assets' / f'small-mark-{color}.svg').getroot()
    assert small.find('svg:path', ns).attrib['d'] == a.attrib['d'], 'Small mark differs from wordmark A'
    assert small.attrib['viewBox'] == '-6 -6 92 92'
    for size in (16, 24, 33):
        with Image.open(ROOT / 'exports' / 'small-mark' / f'{color}-{size}.png') as image:
            assert image.size == (size, size) and image.mode == 'RGBA'
            alpha = image.getchannel('A')
            box = alpha.getbbox()
            assert box and min(box[:2]) > 0 and max(box[2:]) < size, 'Small mark clipped'
            assert alpha.getextrema() == (0, 255)
Links(ROOT / 'small-mark-preview.html').feed((ROOT / 'small-mark-preview.html').read_text())

print('PASS: 10 brand HTML pages, rem type/minimum/casing checks, 6 artwork dimensions/content checks, 16 transparent PNGs and small-mark proof')
