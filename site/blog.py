"""Build approved Markdown posts using the website's shared HTML/CSS shell."""
from dataclasses import dataclass
from datetime import date
from html import escape
from itertools import islice
from pathlib import Path
from urllib.parse import quote, unquote, urlsplit
import io
import hashlib
import json
import re
import shutil

from markdown_it import MarkdownIt
from bs4 import BeautifulSoup
from PIL import Image
import yaml

MD = MarkdownIt('commonmark', {'html': False, 'maxNesting': 20})
MAX_POSTS = 200
MAX_IMAGE_BYTES = 10 * 1024 * 1024
MAX_TOTAL_IMAGE_BYTES = 256 * 1024 * 1024
IMAGE_FORMATS = {'.png': 'PNG', '.jpg': 'JPEG', '.jpeg': 'JPEG', '.webp': 'WEBP'}

BLOG_CSS = '''
    .journal { max-width: 68ch; margin-inline: auto; padding: 4rem 0 6rem }
    .journal > header { margin-bottom: 3rem }
    .journal h1 { font-size: 3.5rem; line-height: 1.1; max-width: 24ch }
    .journal .visually-hidden { position: absolute; width: 1px; height: 1px; overflow: hidden; clip-path: inset(50%); white-space: nowrap }
    .post-list, .post-body { max-width: 68ch }
    .post-month { margin: 3rem 0 1rem; color: #5f6b76; font-size: 1rem }
    .post-month:first-child { margin-top: 0 }
    .post-preview { display: flex; align-items: center; gap: 1.5rem }
    .post-preview + .post-preview { margin-top: 3rem }
    .post-preview-copy { flex: 1; min-width: 0 }
    .post-preview h2 { font-size: 2rem; overflow-wrap: anywhere }
    .post-preview .post-date { margin-top: .375rem }
    .post-summary { margin-top: .375rem; white-space: nowrap; overflow: hidden; text-overflow: ellipsis }
    .post-thumb { display: block; width: 8rem; flex: none }
    .post-thumb img { width: 100%; aspect-ratio: 8 / 5; object-fit: contain; border-radius: .75rem; background: var(--panel) }
    .post-date { display: block; color: var(--quiet); font-size: 1rem }
    .post-header .post-date { margin-top: 1rem }
    .post-author { margin-top: .5rem; color: var(--quiet); font-size: 1rem }
    .post-back { display: inline-block; margin-bottom: 2rem }
    .post-body > * + * { margin-top: 1.5rem }
    .post-body h2 { font-size: 2rem; margin-top: 3rem }
    .post-body h3 { margin-top: 2.5rem }
    .post-body ul { padding-left: 1.5rem; list-style: disc }
    .post-body ol { padding-left: 1.5rem }
    .post-body li + li { margin-top: .5rem }
    .post-body img { border-radius: 1rem; margin: 2rem 0; max-width: 100% }
    .post-body figure { margin: 2rem 0 }
    .post-body figure img { margin: 0; opacity: .9; transition: opacity .15s ease }
    .post-body figcaption { margin-top: .5rem; color: var(--quiet); font-size: 1rem; line-height: 1.4; font-style: normal; text-align: center; transition: color .15s ease }
    .post-body figure:hover img, .post-body figure:focus-within img { opacity: 1 }
    .post-body figure:hover figcaption, .post-body figure:focus-within figcaption { color: var(--text) }
    @media (prefers-reduced-motion: reduce) {
      .post-body figure img, .post-body figcaption { transition: none }
    }
    .post-body blockquote { margin: 2rem 0; padding-left: 1.5rem; color: var(--quiet) }
    .post-body pre { font-family: inherit; padding: 1.25rem; overflow-x: auto; background: var(--panel); color: var(--ivory); border-radius: 1rem }
    .post-body code { font-family: inherit; font-size: 1rem; overflow-wrap: anywhere }
    .post-body a, .post-body p { overflow-wrap: anywhere }
    @media (prefers-color-scheme: dark) {
      .post-month { color: #8794a3 }
    }
    @media (max-width: 48rem) {
      .journal { padding: 2.5rem 0 4rem }
      .journal h1 { font-size: 2.5rem }
      .post-preview { gap: 1rem }
      .post-preview h2 { font-size: 1.5rem }
      .post-thumb { width: 5.5rem }
    }
'''


@dataclass
class Post:
    slug: str
    published: date
    title: str
    summary: str
    html: str
    images: dict
    cover_alt: str = ''
    source_bytes: int = 0

    @property
    def url(self):
        return f'https://myarc.dev/blog/{self.slug}/'

    @property
    def cover(self):
        if not self.images:
            return None
        name, data = next(iter(self.images.items()))
        with Image.open(io.BytesIO(data)) as image:
            return {'url': self.url + quote(name, safe='/'), 'width': image.width,
                    'height': image.height, 'type': Image.MIME[image.format], 'alt': self.cover_alt}


def screenshot(folder, source):
    url = urlsplit(source)
    if url.scheme or url.netloc or url.query or url.fragment:
        raise ValueError(f'Screenshots must be local files: {source}')
    relative = Path(unquote(url.path))
    if relative.is_absolute() or '..' in relative.parts or '\\' in str(relative) or relative.suffix.lower() not in IMAGE_FORMATS:
        raise ValueError(f'Use a relative PNG/JPEG/WebP screenshot: {source}')
    path = (folder / relative).resolve()
    try:
        path.relative_to(folder.resolve())
    except ValueError:
        raise ValueError(f'Screenshot escapes its post folder: {source}') from None
    if path.stat().st_size > MAX_IMAGE_BYTES:
        raise ValueError(f'Screenshot exceeds 10 MiB: {source}')
    data = path.read_bytes()
    with Image.open(io.BytesIO(data)) as image:
        if image.format != IMAGE_FORMATS[relative.suffix.lower()]:
            raise ValueError(f'Screenshot format does not match extension: {source}')
        if image.width * image.height > 24_000_000 or getattr(image, 'is_animated', False):
            raise ValueError(f'Use a static screenshot under 24 megapixels: {source}')
        image.verify()
    return relative.as_posix(), data


def webp_images(name, data):
    files = {}
    with Image.open(io.BytesIO(data)) as original:
        image = original.convert('RGBA')
        image.thumbnail((2400, 2400), Image.Resampling.LANCZOS)
        width, height = image.size
        output = io.BytesIO()
        image.save(output, format='WEBP', quality=90, method=6, icc_profile=original.info.get('icc_profile', b''))
        encoded = output.getvalue()
        path = Path(name)
        name = (path.parent / (path.stem + '.' + hashlib.sha256(encoded).hexdigest()[:12] + '.webp')).as_posix()
        files[name] = encoded
        if width > 960:
            image.thumbnail((960, 2400), Image.Resampling.LANCZOS)
            output = io.BytesIO()
            image.save(output, format='WEBP', quality=90, method=6, icc_profile=original.info.get('icc_profile', b''))
            files[str(Path(name).with_suffix('.960.webp'))] = output.getvalue()
    return name, files, width, height


def load_post(path):
    if path.is_symlink() or path.parent.is_symlink():
        raise ValueError(f'Post sources cannot be symlinks: {path}')
    if path.stat().st_size > 128 * 1024:
        raise ValueError(f'Post exceeds 128 KiB: {path}')
    text = path.read_text()
    if not text.startswith('---\n') or '\n---\n' not in text[4:]:
        raise ValueError(f'Post needs YAML front matter: {path}')
    front, body = text[4:].split('\n---\n', 1)
    if len(front.encode()) > 4096:
        raise ValueError(f'Front matter exceeds 4 KiB: {path}')
    try:
        node = yaml.compose(front, Loader=yaml.SafeLoader)
        if isinstance(node, yaml.MappingNode):
            if any(not isinstance(key, yaml.ScalarNode) for key, _ in node.value):
                raise ValueError(f'Metadata keys must be plain field names: {path}')
            keys = [key.value for key, _ in node.value]
            if len(keys) != len(set(keys)):
                raise ValueError(f'Duplicate metadata field: {path}')
        meta = yaml.safe_load(front)
    except yaml.YAMLError:
        raise ValueError(f'Invalid YAML front matter: {path}') from None
    if not isinstance(meta, dict) or set(meta) - {'title', 'summary', 'draft'}:
        raise ValueError(f'Use only title, summary and draft metadata: {path}')
    if type(meta.get('draft', True)) is not bool:
        raise ValueError(f'draft must be true or false: {path}')
    if meta.get('draft', True):
        return None
    slug = path.parent.name
    if not re.fullmatch(r'\d{4}-\d{2}-\d{2}-[a-z0-9]+(?:-[a-z0-9]+)*', slug):
        raise ValueError(f'Use YYYY-MM-DD-slug folder names: {path}')
    published = date.fromisoformat(slug[:10])
    for field, maximum in (('title', 160), ('summary', 360)):
        value = meta.get(field)
        if not isinstance(value, str) or not value.strip() or len(value) > maximum:
            raise ValueError(f'{field} needs 1–{maximum} characters: {path}')
    if not body.strip():
        raise ValueError(f'Published post has no body: {path}')
    tokens = MD.parse(body)
    images = {}
    cover_alt = ''
    replacements = {}
    source_bytes = 0
    count = 0
    for token in tokens:
        if token.type == 'heading_open' and token.tag == 'h1':
            raise ValueError(f'Title comes from metadata; use ## section headings: {path}')
        for child in token.children or []:
            if child.type == 'image':
                if not child.content.strip():
                    raise ValueError(f'Screenshot needs descriptive alt text: {path}')
                count += 1
                if count > 20:
                    raise ValueError(f'Maximum 20 screenshots per post: {path}')
                source = child.attrGet('src')
                name, data = screenshot(path.parent, source)
                source_bytes += len(data)
                first = not images
                if first:
                    cover_alt = child.content.strip()
                name, files, width, height = webp_images(name, data)
                images.update(files)
                destination = quote(name, safe='/')
                replacements[source] = destination
                child.attrSet('src', destination)
                child.attrSet('width', str(width))
                child.attrSet('height', str(height))
                child.attrSet('decoding', 'async')
                child.attrSet('loading', 'eager' if first else 'lazy')
                if first:
                    child.attrSet('fetchpriority', 'high')
                variant = str(Path(name).with_suffix('.960.webp'))
                if variant in files:
                    child.attrSet('srcset', f'{quote(variant, safe="/")} 960w, {destination} {width}w')
                    child.attrSet('sizes', '(max-width: 48rem) calc(100vw - 3rem), 50rem')
    for token in tokens:
        for child in token.children or []:
            if child.type == 'link_open' and child.attrGet('href') in replacements:
                child.attrSet('href', replacements[child.attrGet('href')])
    rendered = BeautifulSoup(MD.renderer.render(tokens, MD.options, {}), 'html.parser')
    for paragraph in list(rendered.find_all('p')):
        if len(paragraph.find_all('img')) != 1 or paragraph.get_text(strip=True):
            continue
        caption = paragraph.find_next_sibling()
        paragraph.name = 'figure'
        if caption is not None and caption.name == 'p':
            content = [child for child in caption.contents if str(child).strip()]
            if len(content) == 1 and getattr(content[0], 'name', None) == 'em':
                caption.name = 'figcaption'
                content[0].unwrap()
                paragraph.append(caption.extract())
    return Post(slug, published, meta['title'].strip(), meta['summary'].strip(), str(rendered), images, cover_alt, source_bytes)


def page_shell(template, title, summary, url, content, published=None, cover=None, posts=None):
    head, body = template.split('<body>', 1)
    header = re.search(r'<header class="top">.*?</header>', body, re.S).group()
    footer = re.search(r'<footer class="end">.*?</footer>', body, re.S).group()
    head = re.sub(r'<title>.*?</title>', lambda _: f'<title>{escape(title)} | myArc</title>', head, count=1)
    for key in ('name="description"', 'property="og:title"', 'property="og:description"', 'name="twitter:title"', 'name="twitter:description"'):
        value = title if 'title' in key else summary
        head = re.sub(r'(<meta ' + key + r' content=")[^"]*', lambda match: match[1] + escape(value, quote=True), head, count=1)
    head = head.replace('rel="canonical" href="https://myarc.dev/"', f'rel="canonical" href="{url}"')
    head = head.replace('property="og:url" content="https://myarc.dev/"', f'property="og:url" content="{url}"')
    head = head.replace('https://myarc.dev/og-image.png', 'https://myarc.dev/og-image.webp')
    head = head.replace('property="og:image:type" content="image/png"', 'property="og:image:type" content="image/webp"')
    if published:
        head = head.replace('property="og:type" content="website"', 'property="og:type" content="article"')
    schema = {'@context': 'https://schema.org', '@type': 'BlogPosting' if published else 'Blog', 'name': title, 'description': summary, 'url': url}
    author = {'@type': 'Person', 'name': 'bitbonsai', 'url': 'https://github.com/bitbonsai'}
    schema.update(inLanguage='en', publisher=author)
    if published:
        schema.update(headline=title, datePublished=published.isoformat(), author=author,
                      mainEntityOfPage=url, isPartOf={'@type': 'Blog', 'name': 'myArc Blog', 'url': 'https://myarc.dev/blog/'})
    if posts is not None:
        schema['blogPost'] = [{'@type': 'BlogPosting', 'headline': post.title, 'url': post.url, 'datePublished': post.published.isoformat(), 'description': post.summary, 'author': author} for post in posts]
    if cover:
        for key, value in (('property="og:image"', cover['url']),
                           ('property="og:image:secure_url"', cover['url']),
                           ('property="og:image:type"', cover['type']),
                           ('property="og:image:width"', str(cover['width'])),
                           ('property="og:image:height"', str(cover['height'])),
                           ('property="og:image:alt"', cover['alt']),
                           ('name="twitter:image"', cover['url']),
                           ('name="twitter:image:alt"', cover['alt'])):
            head = re.sub(r'(<meta ' + key + r' content=")[^"]*', lambda match: match[1] + escape(value, quote=True), head, count=1)
        schema['image'] = cover['url']
    crumbs = [{'@type': 'ListItem', 'position': 1, 'name': 'myArc', 'item': 'https://myarc.dev/'},
              {'@type': 'ListItem', 'position': 2, 'name': 'Blog', 'item': 'https://myarc.dev/blog/'}]
    if published:
        crumbs.append({'@type': 'ListItem', 'position': 3, 'name': title, 'item': url})
    schema.pop('@context')
    structured = json.dumps({'@context': 'https://schema.org', '@graph': [schema, {'@type': 'BreadcrumbList', 'itemListElement': crumbs}]}).replace('<', '\\u003c')
    head = re.sub(r'(<script type="application/ld\+json">).*?(</script>)', lambda match: match[1] + structured + match[2], head, count=1, flags=re.S)
    head = head.replace('</style>', BLOG_CSS + '\n  </style>', 1)
    alternates = f'<link rel="alternate" type="text/markdown" href="{url}index.md" title="Markdown">\n<link rel="alternate" type="text/plain" href="https://myarc.dev/llms.txt" title="Site context">\n'
    if published:
        alternates += f'<meta name="author" content="bitbonsai">\n<meta property="article:published_time" content="{published.isoformat()}">\n'
    head = head.replace('</head>', alternates + '</head>', 1)
    header = header.replace('href="#main"', 'href="/"').replace('href="#principles"', 'href="/#principles"').replace('href="#building"', 'href="/#building"')
    if published is None:
        header = header.replace('class="blog-link"', 'class="blog-link" aria-current="page"')
    return head + '<body><a class="skip" href="#main">Skip to content</a><div class="wrap">' + header + '<main id="main" class="journal">' + content + '</main>' + footer + '</div></body></html>\n'


def build_blog(root, template, embed):
    paths = list(islice((root / 'posts').glob('*/index.md'), MAX_POSTS + 1))
    if len(paths) > MAX_POSTS:
        raise ValueError(f'Maximum {MAX_POSTS} posts per build')
    posts = []
    total = 0
    for path in sorted(paths):
        post = load_post(path)
        if post:
            total += max(post.source_bytes, sum(map(len, post.images.values())))
            if total > MAX_TOTAL_IMAGE_BYTES:
                raise ValueError('Published screenshots exceed the 256 MiB site budget')
            posts.append(post)
    posts.sort(key=lambda post: post.slug, reverse=True)
    output = root / 'blog'
    if output.is_symlink():
        raise ValueError('Generated blog output cannot be a symlink')
    if output.exists():
        shutil.rmtree(output)
    output.mkdir()
    listing = ''
    last_month = None
    for post in posts:
        month = post.published.strftime('%B %Y')
        if month != last_month:
            listing += f'<p class="post-month">{month}</p>'
            last_month = month
        thumb = ''
        if post.images:
            thumbnails = output / '_thumbnails'
            thumbnails.mkdir(exist_ok=True)
            with Image.open(io.BytesIO(next(iter(post.images.values())))) as image:
                image = image.convert('RGBA')
                image.thumbnail((384, 240), Image.Resampling.LANCZOS)
                buffer = io.BytesIO()
                image.save(buffer, format='WEBP', quality=85, method=6)
                data = buffer.getvalue()
                name = post.slug + '.' + hashlib.sha256(data).hexdigest()[:12] + '.webp'
                (thumbnails / name).write_bytes(data)
                thumb = f'<a class="post-thumb" href="{post.url}" aria-hidden="true" tabindex="-1"><img src="_thumbnails/{name}" alt="" width="{image.width}" height="{image.height}" loading="lazy"></a>'
        listing += f'<article class="post-preview">{thumb}<div class="post-preview-copy"><h2><a href="{post.url}">{escape(post.title)}</a></h2><time class="post-date" datetime="{post.published.isoformat()}">{post.published.strftime("%B %d, %Y")}</time><p class="post-summary">{escape(post.summary)}</p></div></article>'
    if not listing:
        listing = '<p class="quiet">Progress notes will appear here once published.</p>'
    content = '<h1 class="visually-hidden">Blog</h1><div class="post-list">' + listing + '</div>'
    (output / 'index.html').write_text(embed(page_shell(template, 'Development blog', 'Follow myArc development: Chromium and Rust experiments, design decisions, obstacles, and verified progress toward an open-source Arc-inspired browser.', 'https://myarc.dev/blog/', content, posts=posts), ''))
    for post in posts:
        folder = output / post.slug
        folder.mkdir()
        content = '<a class="post-back" href="/blog/">All posts</a><header class="post-header"><h1>' + escape(post.title) + '</h1><time class="post-date" datetime="' + post.published.isoformat() + '">' + post.published.strftime('%B %d, %Y') + '</time><p class="post-author">By <a rel="author" href="https://github.com/bitbonsai">bitbonsai</a> · <a href="index.md" type="text/markdown">Read Markdown</a></p></header><article class="post-body">' + post.html + '</article>'
        (folder / 'index.html').write_text(embed(page_shell(template, post.title, post.summary, post.url, content, post.published, post.cover), '../'))
        for name, data in post.images.items():
            target = folder / name
            target.parent.mkdir(parents=True, exist_ok=True)
            target.write_bytes(data)
    urls = ['https://myarc.dev/', 'https://myarc.dev/blog/'] + [post.url for post in posts]
    covers = {post.url: post.cover for post in posts}
    rows = []
    for url in urls:
        image = covers.get(url)
        image_entry = '<image:image><image:loc>' + escape(image['url']) + '</image:loc></image:image>' if image else ''
        rows.append('<url><loc>' + url + '</loc>' + image_entry + '</url>')
    (root / 'sitemap.xml').write_text('<?xml version="1.0" encoding="UTF-8"?>\n<urlset xmlns="http://www.sitemaps.org/schemas/sitemap/0.9" xmlns:image="http://www.google.com/schemas/sitemap-image/1.1">' + ''.join(rows) + '</urlset>\n')
    print(f'Built blog index and {len(posts)} approved posts')
    return posts
