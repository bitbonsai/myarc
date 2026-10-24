"""Check the deployable file for complete markup and embedded resources."""
from html.parser import HTMLParser
from pathlib import Path
from urllib.parse import urlsplit
import json
import re

ROOT = Path(__file__).resolve().parent


class Page(HTMLParser):
    def __init__(self):
        super().__init__()
        self.ids = set()
        self.anchors = []
        self.assets = 0
        self.h1 = False
        self.headline = []

    def handle_starttag(self, tag, attrs):
        attrs = dict(attrs)
        if 'id' in attrs:
            assert attrs['id'] not in self.ids, f'Duplicate id: {attrs["id"]}'
            self.ids.add(attrs['id'])
        if tag == 'h1':
            self.h1 = True
        if self.h1 and tag == 'br':
            self.headline.append(' ')
        if tag == 'a':
            self.anchors.append(attrs.get('href', ''))
        assert tag not in ('iframe', 'base'), f'Unexpected {tag}'
        for key in ('src', 'srcset', 'poster'):
            if key in attrs:
                assert attrs[key].startswith('data:'), f'External resource: {tag} {key}'
                self.assets += 1
        if tag == 'link' and attrs.get('rel') != 'canonical':
            assert attrs.get('href', '').startswith('data:'), 'External link resource'
            self.assets += 1
        if tag == 'script':
            assert 'src' not in attrs, 'External script'

    def handle_endtag(self, tag):
        if tag == 'h1':
            self.h1 = False

    def handle_data(self, data):
        if self.h1:
            self.headline.append(data)


def check(html):
    assert len(html.encode()) < 5_000_000, 'Unexpectedly large standalone file'
    assert not re.search(r'@@[A-Z_]+@@', html), 'Unresolved template token'
    assert '<html lang="en"' in html and html.rstrip().endswith('</html>')
    page = Page()
    page.feed(html)
    assert ' '.join(''.join(page.headline).split()) == 'myArc, forever yours.'
    for href in page.anchors:
        assert href, 'Empty link'
        if href.startswith('#'):
            assert href[1:] in page.ids, f'Missing anchor: {href}'
        else:
            assert urlsplit(href).scheme == 'https', f'Unexpected link: {href}'
    css = '\n'.join(re.findall(r'<style\b[^>]*>(.*?)</style>', html, re.S))
    for target in re.findall(r'url\([\s\'"]*([^\)\'"\s]+)', css):
        assert target.startswith(('data:', '#')), f'External CSS resource: {target}'
    for size in re.findall(r'font-size\s*:\s*([^;}]+)', css):
        size = size.strip()
        assert size == '100%' or re.fullmatch(r'\d*\.?\d+rem', size), f'Use rem: {size}'
        assert size == '100%' or float(size[:-3]) >= 0.875, f'Tiny font: {size}'
    assert '@import' not in css
    assert not re.search(r'\b(?:fetch|XMLHttpRequest|WebSocket|sendBeacon)\s*\(', html)
    assert 'href="https://myarc.dev/"' in html
    notices = re.search(r'<script type="application/json" id="asset-notices">(.*?)</script>', html, re.S)
    assert notices and {'Outfit', 'Lucide'} <= json.loads(notices.group(1)).keys()
    return page.assets, len(page.anchors)


if __name__ == '__main__':
    html = (ROOT / 'index.html').read_text()
    assets, anchors = check(html)
    # An accidental asset reference must fail before publication.
    try:
        check(html.replace('<body>', '<body><img src="https://example.invalid/asset.png">', 1))
    except AssertionError:
        pass
    else:
        raise AssertionError('Resource guard did not reject an external image')
    print(f'PASS: one complete HTML file, {assets} embedded resources, {anchors} valid links, rem typography and retained licenses')
