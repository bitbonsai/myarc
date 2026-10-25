"""Build approved Markdown posts using the website's shared HTML/CSS shell."""
from dataclasses import dataclass
from datetime import date
from html import escape
from itertools import islice
from pathlib import Path
from urllib.parse import unquote, urlsplit
import io
import json
import re
import shutil

from markdown_it import MarkdownIt
from PIL import Image
import yaml

MD = MarkdownIt('commonmark', {'html': False, 'maxNesting': 20})
MAX_POSTS = 200
MAX_IMAGE_BYTES = 10 * 1024 * 1024
MAX_TOTAL_IMAGE_BYTES = 256 * 1024 * 1024
IMAGE_FORMATS = {'.png': 'PNG', '.jpg': 'JPEG', '.jpeg': 'JPEG', '.webp': 'WEBP'}

BLOG_CSS = '''
    .journal { padding: 4rem 0 6rem }
    .journal > header { margin-bottom: 3rem }
    .journal h1 { font-size: 3.5rem; line-height: 1.1; max-width: 24ch }
    .journal .intro { margin-top: 1.25rem; max-width: 42rem; color: var(--quiet) }
    .post-list, .post-body { max-width: 68ch }
    .post-preview + .post-preview { margin-top: 3rem }
    .post-preview h2 { font-size: 2rem; margin: .5rem 0 .75rem }
    .post-date { display: block; color: var(--quiet); font-size: 1rem }
    .post-header .post-date { margin-top: 1rem }
    .post-back { display: inline-block; margin-bottom: 2rem }
    .post-body > * + * { margin-top: 1.5rem }
    .post-body h2 { font-size: 2rem; margin-top: 3rem }
    .post-body h3 { margin-top: 2.5rem }
    .post-body ul { padding-left: 1.5rem; list-style: disc }
    .post-body ol { padding-left: 1.5rem }
    .post-body li + li { margin-top: .5rem }
    .post-body img { border-radius: 1rem; margin: 2rem 0; max-width: 100% }
    .post-body blockquote { margin: 2rem 0; padding-left: 1.5rem; color: var(--quiet) }
    .post-body pre { font-family: inherit; padding: 1.25rem; overflow-x: auto; background: var(--panel); color: var(--ivory); border-radius: 1rem }
    .post-body code { font-family: inherit; font-size: 1rem; overflow-wrap: anywhere }
    .post-body a, .post-body p { overflow-wrap: anywhere }
    @media (max-width: 48rem) {
      .journal { padding: 2.5rem 0 4rem }
      .journal h1 { font-size: 2.5rem }
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

    @property
    def url(self):
        return f'https://myarc.dev/blog/{self.slug}/'


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
    for token in tokens:
        if token.type == 'heading_open' and token.tag == 'h1':
            raise ValueError(f'Title comes from metadata; use ## section headings: {path}')
        for child in token.children or []:
            if child.type == 'image':
                if not child.content.strip():
                    raise ValueError(f'Screenshot needs descriptive alt text: {path}')
                name, data = screenshot(path.parent, child.attrGet('src'))
                images[name] = data
                if len(images) > 20:
                    raise ValueError(f'Maximum 20 screenshots per post: {path}')
    return Post(slug, published, meta['title'].strip(), meta['summary'].strip(), MD.renderer.render(tokens, MD.options, {}), images)


def page_shell(template, title, summary, url, content, published=None):
    head, body = template.split('<body>', 1)
    header = re.search(r'<header class="top">.*?</header>', body, re.S).group()
    footer = re.search(r'<footer class="end">.*?</footer>', body, re.S).group()
    head = re.sub(r'<title>.*?</title>', lambda _: f'<title>{escape(title)} | myArc</title>', head, count=1)
    for key in ('name="description"', 'property="og:title"', 'property="og:description"', 'name="twitter:title"', 'name="twitter:description"'):
        value = title if 'title' in key else summary
        head = re.sub(r'(<meta ' + key + r' content=")[^"]*', lambda match: match[1] + escape(value, quote=True), head, count=1)
    head = head.replace('rel="canonical" href="https://myarc.dev/"', f'rel="canonical" href="{url}"')
    head = head.replace('property="og:url" content="https://myarc.dev/"', f'property="og:url" content="{url}"')
    if published:
        head = head.replace('property="og:type" content="website"', 'property="og:type" content="article"')
    schema = {'@context': 'https://schema.org', '@type': 'BlogPosting' if published else 'Blog', 'name': title, 'description': summary, 'url': url}
    if published:
        schema.update(headline=title, datePublished=published.isoformat())
    structured = json.dumps(schema).replace('<', '\\u003c')
    head = re.sub(r'(<script type="application/ld\+json">).*?(</script>)', lambda match: match[1] + structured + match[2], head, count=1, flags=re.S)
    head = head.replace('</style>', BLOG_CSS + '\n  </style>', 1)
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
            total += sum(map(len, post.images.values()))
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
    listing = ''.join(f'<article class="post-preview"><time class="post-date" datetime="{post.published.isoformat()}">{post.published.strftime("%B %d, %Y")}</time><h2><a href="{post.url}">{escape(post.title)}</a></h2><p>{escape(post.summary)}</p></article>' for post in posts)
    if not listing:
        listing = '<p class="quiet">Progress notes will appear here once published.</p>'
    content = '<header><h1>Progress journal</h1><p class="intro">Notes on building myArc, with screenshots and implementation updates.</p></header><div class="post-list">' + listing + '</div>'
    (output / 'index.html').write_text(embed(page_shell(template, 'Blog', 'Progress notes on building myArc.', 'https://myarc.dev/blog/', content)))
    for post in posts:
        folder = output / post.slug
        folder.mkdir()
        content = '<a class="post-back" href="/blog/">All posts</a><header class="post-header"><h1>' + escape(post.title) + '</h1><time class="post-date" datetime="' + post.published.isoformat() + '">' + post.published.strftime('%B %d, %Y') + '</time></header><article class="post-body">' + post.html + '</article>'
        (folder / 'index.html').write_text(embed(page_shell(template, post.title, post.summary, post.url, content, post.published)))
        for name, data in post.images.items():
            target = folder / name
            target.parent.mkdir(parents=True, exist_ok=True)
            target.write_bytes(data)
    urls = ['https://myarc.dev/', 'https://myarc.dev/blog/'] + [post.url for post in posts]
    (root / 'sitemap.xml').write_text('<?xml version="1.0" encoding="UTF-8"?>\n<urlset xmlns="http://www.sitemaps.org/schemas/sitemap/0.9">' + ''.join('<url><loc>' + url + '</loc></url>' for url in urls) + '</urlset>\n')
    print(f'Built blog index and {len(posts)} approved posts')
    return posts
