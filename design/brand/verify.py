"""Check local kit links, dimensions, transparency and visibly nonempty exports."""
from html.parser import HTMLParser
from pathlib import Path
from urllib.parse import unquote, urlsplit
import json
import re
from xml.etree import ElementTree
from PIL import Image, ImageStat

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


for path in [ROOT / 'index.html', ROOT / 'preview.html', ROOT / 'type-study.html', *sorted((ROOT / 'artwork').glob('*.html'))]:
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
    for old_copy in ('The web,', 'made yours.', 'Your browsing space,', 'Your browser.'):
        assert old_copy not in text, f'Outdated copy: {path}'

ink = (ROOT / 'assets' / 'wordmark-ink.svg').read_text()
ivory = (ROOT / 'assets' / 'wordmark-ivory.svg').read_text()
assert ink.replace('#182432', 'COLOR') == ivory.replace('#f5f2ec', 'COLOR')
svg = ElementTree.fromstring(ink)
ns = {'svg': 'http://www.w3.org/2000/svg'}
assert len(svg.findall('svg:path', ns)) == 5
assert not svg.findall('.//svg:text', ns), 'Wordmark must remain font-independent'
assert 'M7 76 L37 8 Q40 2 43 8 L73 76' in ink, 'Custom A geometry missing'
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

print('PASS: 9 HTML pages, rem type/minimum/casing checks, 6 artwork dimensions/content checks, 10 transparent PNGs')
