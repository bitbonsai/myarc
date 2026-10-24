"""Check local kit links, dimensions, transparency and visibly nonempty exports."""
from html.parser import HTMLParser
from pathlib import Path
from urllib.parse import unquote, urlsplit
import json
import re
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


for path in [ROOT / 'index.html', ROOT / 'preview.html', *sorted((ROOT / 'artwork').glob('*.html'))]:
    source = path.read_text()
    Links(path).feed(source)
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

print('PASS: 8 HTML pages, rem type/minimum/casing checks, 6 artwork dimensions/content checks, 10 transparent PNGs')
