"""Embed approved brand assets into one independently hostable HTML file."""
from pathlib import Path
import base64
import io
import json
import re

from PIL import Image
from fontTools.ttLib import TTFont

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
html = (ROOT / 'index.template.html').read_text()
assert len(html) < 200_000, 'Template is unexpectedly large'
for token, value in assets.items():
    html = html.replace(token, value)
assert not re.search(r'@@[A-Z_]+@@', html), 'Unresolved asset token'

notices = {
    'Outfit': (ASSETS / 'Outfit-OFL.txt').read_text(),
    'Lucide': (ASSETS / 'Lucide-LICENSE.txt').read_text(),
    'artwork': 'User-supplied ribbon logos. Original grain and custom wordmark generated for myArc. See repository ATTRIBUTION.md for provenance and rights limitations.',
}
notice_json = json.dumps(notices, ensure_ascii=True).replace('<', '\\u003c')
html = html.replace('</head>', f'<script type="application/json" id="asset-notices">{notice_json}</script>\n</head>', 1)
assert html.endswith('</html>\n') or html.endswith('</html>'), 'Incomplete HTML document'
(ROOT / 'index.html').write_text(html)
print(f'Built {ROOT / "index.html"}: {len(html.encode()):,} bytes. All visual assets embedded.')
