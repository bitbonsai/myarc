"""Embed approved brand assets into one independently hostable HTML file."""
from pathlib import Path
import base64
import hashlib
import io
import json
import re

from PIL import Image
from fontTools.ttLib import TTFont

from blog import build_blog
from discovery import build_discovery
from download import apply_release, build_download, read_release

ROOT = Path(__file__).resolve().parent
BRAND = ROOT.parent / 'design' / 'brand'
ASSETS = BRAND / 'assets'


def uri(mime, data):
    return f'data:{mime};base64,' + base64.b64encode(data).decode('ascii')


def logo(name):
    with Image.open(ASSETS / name) as image:
        image = image.convert('RGBA')
        image.thumbnail((960, 960), Image.Resampling.LANCZOS)
        output = io.BytesIO()
        image.save(output, format='WEBP', quality=90, method=6)
        return uri('image/webp', output.getvalue())


font = TTFont(ASSETS / 'Outfit.ttf', recalcTimestamp=False)
font.flavor = 'woff2'
font_bytes = io.BytesIO()
font.save(font_bytes)
heart = (ASSETS / 'heart.svg').read_text()
heart = heart.replace('<svg\n', '<svg class="heart" role="img" aria-label="love" focusable="false"\n', 1)
assets = {
    '@@OUTFIT_FONT@@': uri('font/woff2', font_bytes.getvalue()),
    '@@WORDMARK_INK@@': uri('image/svg+xml', (ASSETS / 'wordmark-ink.svg').read_bytes()),
    '@@WORDMARK_IVORY@@': uri('image/svg+xml', (ASSETS / 'wordmark-ivory.svg').read_bytes()),
    '@@LOGO_MATTE@@': logo('myarc-matte.png'),
    '@@LOGO_GLASS@@': logo('myarc-glass.png'),
    '@@GRAIN@@': uri('image/png', (ASSETS / 'grain.png').read_bytes()),
    '@@FAVICON@@': uri('image/png', (BRAND / 'exports' / 'mark-matte-128.png').read_bytes()),
    '@@HEART@@': heart,
}
notices = {
    'Outfit': (ASSETS / 'Outfit-OFL.txt').read_text(),
    'Lucide': (ASSETS / 'Lucide-LICENSE.txt').read_text(),
    'artwork': 'User-supplied ribbon logos. Original grain and custom wordmark generated for myArc. See repository ATTRIBUTION.md for provenance and rights limitations.',
}
notice_json = json.dumps(notices, ensure_ascii=True).replace('<', '\\u003c')


def embed(template, prefix=None):
    assert len(template.encode()) < 1_000_000, 'Page template is unexpectedly large'
    for token, value in assets.items():
        if token not in template:
            continue
        if prefix is not None and value.startswith('data:'):
            mime, encoded = value[5:].split(';base64,', 1)
            data = base64.b64decode(encoded)
            if token in ('@@LOGO_MATTE@@', '@@GRAIN@@'):
                with Image.open(io.BytesIO(data)) as image:
                    image = image.convert('RGBA')
                    if token == '@@LOGO_MATTE@@':
                        image.thumbnail((64, 64), Image.Resampling.LANCZOS)
                    output = io.BytesIO()
                    image.save(output, format='WEBP', lossless=token == '@@GRAIN@@', quality=90, method=6)
                    data = output.getvalue()
                    mime = 'image/webp'
            suffix = {'font/woff2': 'woff2', 'image/svg+xml': 'svg', 'image/webp': 'webp', 'image/png': 'png'}[mime]
            name = token.strip('@').lower().replace('_', '-') + '.' + hashlib.sha256(data).hexdigest()[:12] + '.' + suffix
            folder = ROOT / 'blog/_assets'
            folder.mkdir(exist_ok=True)
            (folder / name).write_bytes(data)
            value = prefix + '_assets/' + name
        template = template.replace(token, value)
    assert not re.search(r'@@[A-Z_]+@@', template), 'Unresolved asset token'
    template = template.replace('</head>', f'<script type="application/json" id="asset-notices">{notice_json}</script>\n</head>', 1)
    assert template.rstrip().endswith('</html>'), 'Incomplete HTML document'
    return template


with Image.open(ROOT / 'og-image.png') as image:
    image.save(ROOT / 'og-image.webp', format='WEBP', quality=90, method=6)

try:
    release = read_release(ROOT)
except ValueError as error:
    raise SystemExit(f'Release data invalid: {error}') from None
template = apply_release((ROOT / 'index.template.html').read_text(), release)
try:
    posts = build_blog(ROOT, template, embed, ['https://myarc.dev/download/'])
except (ValueError, OSError, RecursionError) as error:
    raise SystemExit(f'Blog build failed: {error}') from None
build_download(ROOT, template, embed, release)
template = template.replace('</head>', '<link rel="alternate" type="text/markdown" href="https://myarc.dev/index.md" title="Markdown">\n<link rel="alternate" type="text/plain" href="https://myarc.dev/llms.txt" title="Site context">\n</head>', 1)
html = embed(template)
(ROOT / 'index.html').write_text(html)
build_discovery(ROOT, html, posts)
print(f'Built {ROOT / "index.html"}: {len(html.encode()):,} bytes. All visual assets embedded.')
