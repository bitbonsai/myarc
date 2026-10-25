"""Validate generated metadata and local discovery resources before publishing."""
from pathlib import Path
from urllib.parse import unquote, urljoin, urlsplit
import json
import xml.etree.ElementTree as ET

from bs4 import BeautifulSoup
from PIL import Image

ROOT = Path(__file__).resolve().parent


def local_file(url):
    parsed = urlsplit(url)
    if parsed.scheme != 'https' or parsed.netloc != 'myarc.dev':
        raise ValueError(f'Expected canonical HTTPS site URL: {url}')
    path = ROOT / unquote(parsed.path).lstrip('/')
    path.resolve().relative_to(ROOT)
    if path.is_dir():
        path /= 'index.html'
    if not path.is_file():
        raise ValueError(f'Missing generated resource: {url}')
    return path


def check():
    titles = set()
    pages = [ROOT / 'index.html', ROOT / 'blog/index.html', *sorted((ROOT / 'blog').glob('*/index.html'))]
    for path in pages:
        soup = BeautifulSoup(path.read_text(), 'html.parser')
        expected = 'https://myarc.dev/' + path.relative_to(ROOT).as_posix().removesuffix('index.html')
        canonical = soup.select_one('link[rel="canonical"]')['href']
        assert canonical == expected, f'Canonical mismatch: {path}'
        assert soup.html['lang'] == 'en' and len(soup.select('h1')) == 1
        title = soup.title.get_text()
        assert title and title not in titles, f'Duplicate/empty title: {path}'
        titles.add(title)
        for selector in ('meta[name="description"]', 'meta[property="og:title"]', 'meta[property="og:description"]', 'meta[name="twitter:title"]', 'meta[name="twitter:description"]'):
            assert soup.select_one(selector)['content'].strip(), f'Empty metadata: {selector}'
        assert soup.select_one('meta[property="og:url"]')['content'] == canonical
        assert soup.select_one('meta[name="twitter:card"]')['content'] == 'summary_large_image'
        assert soup.select_one('meta[name="robots"]')['content'].startswith('index, follow')
        markdown = soup.select_one('link[rel="alternate"][type="text/markdown"]')['href']
        assert local_file(markdown).read_text().startswith('HTML: ' + canonical)
        schema = json.loads(soup.select_one('script[type="application/ld+json"]').string)
        nodes = schema.get('@graph', [schema])
        assert any(node.get('@type') in ('WebSite', 'Blog', 'BlogPosting') for node in nodes)
        if path.parent != ROOT:
            assert any(node.get('@type') == 'BreadcrumbList' for node in nodes)
        image_url = soup.select_one('meta[property="og:image"]')['content']
        assert soup.select_one('meta[name="twitter:image"]')['content'] == image_url
        with Image.open(local_file(image_url)) as image:
            assert int(soup.select_one('meta[property="og:image:width"]')['content']) == image.width
            assert int(soup.select_one('meta[property="og:image:height"]')['content']) == image.height
            assert soup.select_one('meta[property="og:image:type"]')['content'] == Image.MIME[image.format]
        for image in soup.select('.post-body img'):
            figure = image.find_parent('figure')
            caption = figure.find('figcaption', recursive=False) if figure else None
            assert caption and caption.get_text(strip=True), f'Article image needs an explicit caption: {path} ({image.get("alt")})'
            assert image['src'].endswith('.webp') and image.get('alt')
            assert image.get('width') and image.get('height') and image.get('decoding') == 'async'
        for element in soup.select('[src], source[srcset]'):
            if element.get('src', '').startswith('data:') or element.get('srcset', '').startswith('data:'):
                continue
            if element.get('src'):
                local_file(urljoin(canonical, element['src']))
            for candidate in element.get('srcset', '').split(','):
                if candidate.strip():
                    local_file(urljoin(canonical, candidate.strip().split()[0]))
    sitemap = ET.parse(ROOT / 'sitemap.xml')
    listed = {node.text for node in sitemap.findall('.//{http://www.sitemaps.org/schemas/sitemap/0.9}loc')}
    assert listed == {'https://myarc.dev/' + path.relative_to(ROOT).as_posix().removesuffix('index.html') for path in pages}
    assert (ROOT / 'robots.txt').read_text().startswith('User-agent: *\nAllow: /')
    for name in ('llms.txt', 'llms-full.txt'):
        assert (ROOT / name).read_text().startswith('# myArc')
    for page in (ROOT / 'blog').glob('*/index.html'):
        assert not list(page.parent.rglob('*.png')), 'Original PNG screenshots leaked into output'
    print(f'PASS: {len(pages)} canonical pages, metadata, schemas, WebP images and Markdown/discovery routes')


if __name__ == '__main__':
    check()
