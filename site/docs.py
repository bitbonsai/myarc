"""Build the user docs at /docs/ from Markdown in site/docs-src/, using the blog's page shell."""
from dataclasses import dataclass
from html import escape
from pathlib import Path
import json
import re
import shutil

from markdown_it import MarkdownIt
import yaml

from blog import SLUG, page_shell

# The blog's CommonMark settings, plus tables for the shortcut list.
MD = MarkdownIt('commonmark', {'html': False, 'maxNesting': 20}).enable('table')

URL = 'https://myarc.dev/docs/'
REPO = 'https://github.com/bitbonsai/myarc'
AREAS = ('Getting started', 'Your workspace', 'Control', 'Help')
TITLE = 'Docs'
SUMMARY = 'How to move from Arc to myArc: install the nightly, import your Spaces, learn the shortcuts, find your data, script the browser and work around known limitations.'
MAX_PAGES = 50

CSS = '''
    .docs-intro { margin-top: 1rem; color: var(--quiet) }
    .docs-index ul { list-style: none; padding-left: 0 }
    .docs-index li + li { margin-top: 1rem }
    .docs-index li p { margin-top: .25rem; color: var(--quiet) }
    .post-body table { width: 100%; border-collapse: collapse; font-size: 1rem }
    .post-body th, .post-body td { padding: .5rem .75rem .5rem 0; border-bottom: 1px solid color-mix(in srgb, currentColor 18%, transparent); text-align: left; vertical-align: top }
'''


@dataclass
class Doc:
    slug: str
    area: str
    order: int
    title: str
    summary: str
    html: str

    @property
    def url(self):
        return f'{URL}{self.slug}/'


def load_doc(path):
    if path.is_symlink() or path.parent.is_symlink():
        raise ValueError(f'Doc sources cannot be symlinks: {path}')
    if path.stat().st_size > 128 * 1024:
        raise ValueError(f'Doc exceeds 128 KiB: {path}')
    text = path.read_text()
    if not text.startswith('---\n') or '\n---\n' not in text[4:]:
        raise ValueError(f'Doc needs YAML front matter: {path}')
    front, body = text[4:].split('\n---\n', 1)
    try:
        meta = yaml.safe_load(front)
    except yaml.YAMLError:
        raise ValueError(f'Invalid YAML front matter: {path}') from None
    if not isinstance(meta, dict) or set(meta) != {'title', 'summary', 'area', 'order'}:
        raise ValueError(f'Use exactly title, summary, area and order metadata: {path}')
    slug = path.parent.name
    if not SLUG.fullmatch(slug):
        raise ValueError(f'Use lowercase hyphenated folder names: {path}')
    if meta['area'] not in AREAS:
        raise ValueError(f'area must be one of {", ".join(AREAS)}: {path}')
    if type(meta['order']) is not int:
        raise ValueError(f'order must be a whole number: {path}')
    for field, maximum in (('title', 160), ('summary', 360)):
        value = meta[field]
        if not isinstance(value, str) or not value.strip() or len(value) > maximum:
            raise ValueError(f'{field} needs 1–{maximum} characters: {path}')
    if not body.strip():
        raise ValueError(f'Doc has no body: {path}')
    tokens = MD.parse(body)
    for token in tokens:
        if token.type == 'heading_open' and token.tag == 'h1':
            raise ValueError(f'Title comes from metadata; use ## section headings: {path}')
        if any(child.type == 'image' for child in token.children or []):
            raise ValueError(f'Docs take no images yet: {path}')
    html = MD.renderer.render(tokens, MD.options, {})
    return Doc(slug, meta['area'], meta['order'], meta['title'].strip(), meta['summary'].strip(), html)


def load_docs(root):
    paths = sorted((Path(root) / 'docs-src').glob('*/index.md'))
    if len(paths) > MAX_PAGES:
        raise ValueError(f'Maximum {MAX_PAGES} docs per build')
    docs = [load_doc(path) for path in paths]
    return sorted(docs, key=lambda doc: (AREAS.index(doc.area), doc.order, doc.slug))


def shell(template, title, summary, url, content, crumbs):
    page = page_shell(template, title, summary, url, content)
    items = [{'@type': 'ListItem', 'position': 1, 'name': 'myArc', 'item': 'https://myarc.dev/'}]
    items += [{'@type': 'ListItem', 'position': index, 'name': name, 'item': item} for index, (name, item) in enumerate(crumbs, 2)]
    schema = {'@context': 'https://schema.org', '@graph': [
        {'@type': 'WebPage', 'name': title, 'description': summary, 'url': url, 'inLanguage': 'en', 'isPartOf': {'@type': 'WebSite', 'name': 'myArc', 'url': 'https://myarc.dev/'}},
        {'@type': 'BreadcrumbList', 'itemListElement': items}]}
    page = re.sub(r'(<script type="application/ld\+json">).*?(</script>)', lambda match: match[1] + json.dumps(schema).replace('<', '\\u003c') + match[2], page, count=1, flags=re.S)
    page = page.replace(' aria-current="page"', '').replace('class="docs-link"', 'class="docs-link" aria-current="page"')
    return page.replace('</style>', CSS + '  </style>', 1)


def build_docs(root, template, embed, docs):
    output = Path(root) / 'docs'
    if output.is_symlink():
        raise ValueError('Generated docs output cannot be a symlink')
    if output.exists():
        shutil.rmtree(output)
    output.mkdir()
    listing = ''
    for area in AREAS:
        rows = ''.join(f'<li><a href="{doc.slug}/">{escape(doc.title)}</a><p>{escape(doc.summary)}</p></li>' for doc in docs if doc.area == area)
        if rows:
            listing += f'<h2>{area}</h2><ul>{rows}</ul>'
    listing += f'<h2>Contributing</h2><p>Plans, the alpha checklist and contributor docs live in the <a href="{REPO}">myArc repository</a> on GitHub.</p>'
    content = f'<header class="post-header"><h1>{TITLE}</h1><p class="docs-intro">{escape(SUMMARY)}</p></header><div class="post-body docs-index">{listing}</div>'
    # Shared hashed assets live with the blog, which the build always creates first.
    (output / 'index.html').write_text(embed(shell(template, TITLE, SUMMARY, URL, content, [(TITLE, URL)]), '../blog/'))
    for doc in docs:
        folder = output / doc.slug
        folder.mkdir()
        content = f'<a class="post-back" href="/docs/">All docs</a><header class="post-header"><h1>{escape(doc.title)}</h1></header><article class="post-body">{doc.html}</article>'
        page = shell(template, doc.title, doc.summary, doc.url, content, [(TITLE, URL), (doc.title, doc.url)])
        (folder / 'index.html').write_text(embed(page, '../../blog/'))
    print(f'Built docs index and {len(docs)} pages')
