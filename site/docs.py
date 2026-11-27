"""Build the user docs at /docs/ from Markdown in site/docs-src/, using the blog's page shell."""
from dataclasses import dataclass, field
from html import escape
from pathlib import Path
import hashlib
import json
import re
import shutil

from bs4 import BeautifulSoup
from markdown_it import MarkdownIt
import yaml

from blog import SLUG, page_shell

# The blog's CommonMark settings, plus tables for the shortcut list.
MD = MarkdownIt('commonmark', {'html': False, 'maxNesting': 20}).enable('table')

URL = 'https://myarc.dev/docs/'
REPO = 'https://github.com/bitbonsai/myarc'
AREAS = ('Getting started', 'Your workspace', 'Control', 'Help', 'Brand and design')
TITLE = 'Docs'
SUMMARY = 'How to move from Arc to myArc: install myArc, import your Spaces, learn the shortcuts, find your data, script the browser, explore the design system and work around known limitations.'
MAX_PAGES = 50

CSS = '''
    .docs-brand { display: flex; align-items: center; gap: .75rem }
    .docs-pill { display: inline-flex; align-items: center; padding: .3125rem .46rem .3125rem .54rem; border-radius: 999rem; box-shadow: inset 0 0 0 .0625rem rgb(var(--link-rgb) / .45); color: var(--link-solid); font-size: .875rem; font-weight: 600; letter-spacing: .08em; line-height: 1; text-transform: uppercase; text-decoration: none; transform: translateY(-.1875rem) }
    .docs-pill:hover, .docs-pill:focus-visible { background: rgb(var(--link-rgb) / .08) }
    .docs-top .docs-actions { display: flex; align-items: center; gap: .75rem; margin-left: auto }
    .docs-search-btn { display: inline-flex; align-items: center; gap: .5rem; min-height: 2.75rem; padding: .5rem .875rem; border: .0625rem solid color-mix(in srgb, currentColor 16%, transparent); border-radius: .75rem; background: color-mix(in srgb, var(--page) 78%, transparent); color: var(--quiet); font: inherit; font-size: 1rem; font-weight: 500; line-height: 1.2; cursor: pointer; transition: border-color .2s ease-in-out, color .2s ease-in-out, background-color .2s ease-in-out }
    .docs-search-btn[hidden] { display: none }
    .docs-search-btn:hover, .docs-search-btn:focus-visible { border-color: rgb(var(--link-rgb) / .45); background: rgb(var(--link-rgb) / .08); color: var(--link-solid) }
    .docs-search-btn svg { width: 1.125rem; height: 1.125rem; flex: none }
    .docs-search-btn kbd { margin-left: .25rem; padding: .0625rem .375rem; border: .0625rem solid color-mix(in srgb, currentColor 20%, transparent); border-radius: .375rem; font-family: inherit; font-size: .875rem; font-weight: 600; line-height: 1.3 }
    .works.docs-search { width: min(38rem, calc(100% - 2rem)); max-height: min(36rem, calc(100dvh - 3rem)); padding: 1.25rem; overflow: hidden }
    .works.docs-search[open] { display: flex; flex-direction: column }
    .docs-search-bar { display: flex; align-items: center; gap: .75rem; padding: .25rem .25rem .875rem; border-bottom: .0625rem solid color-mix(in srgb, currentColor 14%, transparent) }
    .docs-search-bar svg { width: 1.25rem; height: 1.25rem; color: var(--quiet); flex: none }
    .docs-search-input { flex: 1; min-width: 0; border: 0; background: transparent; color: var(--text); font: inherit; font-size: 1.125rem; line-height: 1.4; outline: none }
    .docs-search-input::-webkit-search-cancel-button { -webkit-appearance: none; appearance: none }
    .docs-search-input::placeholder { color: var(--quiet) }
    .docs-search form { float: none; margin: 0 }
    .works .docs-search-results { flex: 1 1 auto; min-height: 0; overflow-y: auto; margin: .75rem 0 0; padding: 0; list-style: none; overscroll-behavior: contain }
    .docs-search-results li + li { margin-top: .25rem }
    .docs-search-results a { display: block; padding: .75rem .875rem; border-radius: .875rem; color: var(--text); text-decoration: none; transition: background-color .15s ease }
    .docs-search-results a:hover, .docs-search-results a:focus-visible, .docs-search-results a.is-active { background: rgb(var(--link-rgb) / .12); color: var(--text) }
    .docs-search-meta { display: block; color: var(--quiet); font-size: .875rem; font-weight: 600; letter-spacing: .06em; text-transform: uppercase }
    .docs-search-title { display: block; margin-top: .125rem; font-size: 1.125rem; font-weight: 600; line-height: 1.3 }
    .docs-search-copy { display: block; margin-top: .25rem; color: var(--quiet); font-size: 1rem; line-height: 1.4 }
    .docs-search-results mark { padding: 0 .125rem; border-radius: .25rem; background: rgb(var(--link-rgb) / .2); color: inherit }
    .docs-search-empty { margin: 1.5rem 0 .5rem; text-align: center; color: var(--quiet); font-size: 1rem }
    @media (prefers-reduced-motion: reduce) {
      .docs-search-btn, .docs-search-results a { transition: none }
    }
    .docs-layout { display: grid; grid-template-columns: 13rem minmax(0, 1fr) 13rem; gap: 3rem; align-items: start; padding: 2.5rem 0 5rem }
    .docs-layout.no-toc { grid-template-columns: 13rem minmax(0, 1fr) }
    .journal.docs-main { max-width: 68ch; min-width: 0; margin: 0; padding: 0 }
    .docs-side { position: sticky; top: 2rem; max-height: calc(100dvh - 4rem); overflow-y: auto; font-size: 1rem; line-height: 1.4 }
    .docs-label { margin: 1.75rem 0 .5rem .75rem; color: var(--quiet); font-size: .875rem; font-weight: 600; letter-spacing: .08em; text-transform: uppercase }
    .docs-nav-list > .docs-label:first-child, .docs-toc > .docs-label { margin-top: 0 }
    .docs-nav li + li { margin-top: .125rem }
    .docs-nav a { display: block; padding: .5rem .75rem; border-radius: .75rem; color: var(--text); font-weight: 500; text-decoration: none }
    .docs-nav a:hover, .docs-nav a:focus-visible { background: rgb(var(--link-rgb) / .06); color: var(--link-solid) }
    .docs-nav a[aria-current] { background: rgb(var(--link-rgb) / .12); color: var(--link-solid); font-weight: 600 }
    .docs-nav-menu { display: none }
    .docs-toc ul { padding-left: .5rem; border-left: .0625rem solid color-mix(in srgb, currentColor 14%, transparent) }
    .docs-toc a { display: block; padding: .375rem .625rem; border-radius: .625rem; color: var(--quiet); text-decoration: none }
    .docs-toc .toc-h3 a { padding-left: 1.5rem }
    .docs-toc a:hover, .docs-toc a:focus-visible { color: var(--link-solid) }
    .docs-toc a.is-active { background: rgb(var(--link-rgb) / .12); color: var(--link-solid) }
    .docs-eyebrow { color: var(--quiet); font-size: .875rem; font-weight: 600; letter-spacing: .08em; text-transform: uppercase }
    .docs-main h1 { margin-top: .5rem }
    .docs-lede { margin-top: 1rem; color: var(--quiet); font-size: 1.25rem; line-height: 1.5 }
    .docs-main .post-body :is(h2, h3) { scroll-margin-top: 1.5rem }
    .docs-next { margin-top: 4rem; font-size: 1.25rem; font-weight: 600 }
    .docs-index ul { list-style: none; padding-left: 0 }
    .docs-index li + li { margin-top: 1rem }
    .docs-index li p { margin-top: .25rem; color: var(--quiet) }
    .post-body table { width: 100%; border-collapse: collapse; font-size: 1rem }
    .post-body th, .post-body td { padding: .5rem .75rem .5rem 0; border-bottom: 1px solid color-mix(in srgb, currentColor 18%, transparent); text-align: left; vertical-align: top }
    @media (max-width: 68.75rem) {
      .docs-layout { grid-template-columns: 13rem minmax(0, 1fr) }
      .docs-toc { display: none }
    }
    @media (max-width: 48rem) {
      .docs-top { flex-wrap: wrap; row-gap: .5rem }
      .docs-top .button { padding: .5rem .875rem; font-size: 1rem }
      .docs-search-btn { min-height: 2.375rem; padding: .375rem .75rem }
      .docs-search-btn kbd { display: none }
      .docs-layout, .docs-layout.no-toc { grid-template-columns: minmax(0, 1fr); gap: 1.5rem; padding: 1rem 0 4rem }
      .docs-side { position: static; max-height: none; overflow: visible }
      .docs-nav-list { display: none }
      .docs-nav-menu { display: block }
      .docs-nav-menu summary { min-height: 2.75rem; padding: .625rem 1rem; border-radius: .75rem; background: rgb(var(--link-rgb) / .08); font-weight: 600; cursor: pointer }
      .docs-nav-menu[open] summary { margin-bottom: .5rem }
      .docs-lede { font-size: 1.125rem }
    }
'''


@dataclass
class Doc:
    slug: str
    area: str
    order: int
    title: str
    summary: str
    html: str
    toc: list = field(default_factory=list)
    extra_css: str = ''

    @property
    def url(self):
        return f'{URL}{self.slug}/'


def heading_id(text, seen):
    """A stable anchor from the heading text, numbered when two headings share it."""
    base = re.sub(r'[^a-z0-9]+', '-', re.sub("['\u2019]", '', text.lower())).strip('-') or 'section'
    anchor, count = base, 2
    while anchor in seen:
        anchor, count = f'{base}-{count}', count + 1
    seen.add(anchor)
    return anchor


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
    # On this page lists ## and ### headings; each gets an id it can link to.
    toc = []
    seen = {'main', 'toc-title'}
    for index, token in enumerate(tokens):
        if token.type == 'heading_open' and token.tag in ('h2', 'h3'):
            text = ''.join(child.content for child in tokens[index + 1].children or [] if child.type in ('text', 'code_inline')).strip()
            anchor = heading_id(text, seen)
            token.attrSet('id', anchor)
            toc.append((int(token.tag[1]), anchor, text))
    html = MD.renderer.render(tokens, MD.options, {})
    return Doc(slug, meta['area'], meta['order'], meta['title'].strip(), meta['summary'].strip(), html, toc)


def load_docs(root):
    paths = sorted((Path(root) / 'docs-src').glob('*/index.md'))
    if len(paths) > MAX_PAGES:
        raise ValueError(f'Maximum {MAX_PAGES} docs per build')
    docs = [load_doc(path) for path in paths]
    return sorted(docs, key=lambda doc: (AREAS.index(doc.area), doc.order, doc.slug))


def side_nav(docs, current, prefix):
    """Pages grouped by area. Phones get the same list folded into a details element."""
    groups = ''
    for area in AREAS:
        links = ''
        for doc in docs:
            if doc.area == area:
                mark = ' aria-current="page"' if doc is current else ''
                links += f'<li><a href="{prefix}{doc.slug}/"{mark}>{escape(doc.title)}</a></li>'
        if links:
            groups += f'<p class="docs-label">{area}</p><ul>{links}</ul>'
    return f'<nav class="docs-side docs-nav" aria-label="Docs"><details class="docs-nav-menu"><summary>All docs</summary>{groups}</details><div class="docs-nav-list">{groups}</div></nav>'


def contents(doc):
    if not doc.toc:
        return ''
    items = ''.join(f'<li class="toc-h{level}"><a href="#{anchor}">{escape(text)}</a></li>' for level, anchor, text in doc.toc)
    return f'<nav class="docs-side docs-toc" aria-labelledby="toc-title"><p class="docs-label" id="toc-title">On this page</p><ul>{items}</ul></nav>'


SEARCH_ICON = '<svg viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="2" stroke-linecap="round" stroke-linejoin="round" aria-hidden="true" focusable="false"><circle cx="11" cy="11" r="8"/><path d="m21 21-4.3-4.3"/></svg>'
CLOSE_ICON = '<svg viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="2" stroke-linecap="round" stroke-linejoin="round" aria-hidden="true" focusable="false"><path d="M18 6 6 18"/><path d="m6 6 12 12"/></svg>'


def search_bundle(docs):
    records = []
    for doc in docs:
        body = ' '.join(BeautifulSoup(doc.html, 'html.parser').get_text(' ').split())
        records.append({
            'slug': doc.slug,
            'title': doc.title,
            'area': doc.area,
            'summary': doc.summary,
            'sections': [{'id': anchor, 'title': text} for _, anchor, text in doc.toc],
            'body': body,
        })
    payload = json.dumps(records, ensure_ascii=True).replace('<', '\\u003c')
    script = Path(__file__).with_name('docs-search.js').read_text()
    return f'window.__MYARC_DOCS_INDEX__ = {payload};\n{script}'.encode()


def search_dialog(prefix):
    return (
        f'<dialog id="docs-search" class="works docs-search" data-prefix="{escape(prefix, quote=True)}" aria-label="Search docs">'
        f'<div class="docs-search-bar">{SEARCH_ICON}'
        '<input class="docs-search-input" type="search" placeholder="Search docs…" autocomplete="off" spellcheck="false" aria-label="Search docs">'
        f'<form method="dialog"><button class="works-close" aria-label="Close search">{CLOSE_ICON}</button></form></div>'
        '<ul class="docs-search-results" aria-label="Search results"></ul>'
        '<p class="docs-search-empty" hidden>No matching docs.</p>'
        '</dialog>'
    )


def shell(template, title, summary, url, content, crumbs, nav, toc='', script='', extra_css='', search_prefix=''):
    page = page_shell(template, title, summary, url, content)
    items = [{'@type': 'ListItem', 'position': 1, 'name': 'myArc', 'item': 'https://myarc.dev/'}]
    items += [{'@type': 'ListItem', 'position': index, 'name': name, 'item': item} for index, (name, item) in enumerate(crumbs, 2)]
    schema = {'@context': 'https://schema.org', '@graph': [
        {'@type': 'WebPage', 'name': title, 'description': summary, 'url': url, 'inLanguage': 'en', 'isPartOf': {'@type': 'WebSite', 'name': 'myArc', 'url': 'https://myarc.dev/'}},
        {'@type': 'BreadcrumbList', 'itemListElement': items}]}
    page = re.sub(r'(<script type="application/ld\+json">).*?(</script>)', lambda match: match[1] + json.dumps(schema).replace('<', '\\u003c') + match[2], page, count=1, flags=re.S)
    # Docs pages swap the site header for the logo, a Docs pill, search and the Download button.
    header = re.search(r'<header class="top">.*?</header>', page, re.S).group()
    mark = re.search(r'<a class="mark plain".*?</a>', header, re.S).group()
    download = re.search(r'<a class="button download-link".*?</a>', header, re.S)
    current = ' aria-current="page"' if url == URL else ''
    search_btn = f'<button class="docs-search-btn" type="button" aria-haspopup="dialog" aria-controls="docs-search" hidden>{SEARCH_ICON}<span>Search</span><kbd>⌘K</kbd></button>'
    actions = f'<div class="docs-actions">{search_btn}{download.group() if download else ""}</div>'
    top = f'<header class="top docs-top"><div class="docs-brand">{mark}<a class="docs-link docs-pill" href="/docs/"{current}>Docs</a></div>{actions}</header>'
    page = page.replace(header, top, 1)
    layout = 'docs-layout' if toc else 'docs-layout no-toc'
    page = page.replace('<main id="main" class="journal">', f'<div class="{layout}">{nav}<main id="main" class="journal docs-main">', 1)
    page = page.replace('</main>', '</main>' + toc + '</div>', 1).replace('</body>', search_dialog(search_prefix) + script + '</body>', 1)
    return page.replace('</style>', CSS + extra_css + '  </style>', 1)


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
    content = f'<header class="post-header"><h1>{TITLE}</h1><p class="docs-lede">{escape(SUMMARY)}</p></header><div class="post-body docs-index">{listing}</div>'
    # Shared hashed scripts: On this page highlight and offline client-side search.
    script = Path(__file__).with_name('docs-toc.js').read_bytes()
    script_name = 'docs-toc.' + hashlib.sha256(script).hexdigest()[:12] + '.js'
    search_bytes = search_bundle(docs)
    search_name = 'docs-search.' + hashlib.sha256(search_bytes).hexdigest()[:12] + '.js'
    (output / '_assets').mkdir()
    (output / '_assets' / script_name).write_bytes(script)
    (output / '_assets' / search_name).write_bytes(search_bytes)
    # Shared hashed assets live with the blog, which the build always creates first.
    index_script = f'<script defer src="_assets/{search_name}"></script>'
    (output / 'index.html').write_text(embed(shell(template, TITLE, SUMMARY, URL, content, [(TITLE, URL)], side_nav(docs, None, ''), '', index_script, '', ''), '../blog/'))
    for index, doc in enumerate(docs):
        folder = output / doc.slug
        folder.mkdir()
        following = docs[index + 1] if index + 1 < len(docs) else None
        after = f'<p class="docs-next"><a href="../{following.slug}/" rel="next">Next: {escape(following.title)} &rarr;</a></p>' if following else ''
        content = f'<header class="post-header"><p class="docs-eyebrow">{escape(doc.area)} / {escape(doc.title)}</p><h1>{escape(doc.title)}</h1><p class="docs-lede">{escape(doc.summary)}</p></header><article class="post-body">{doc.html}</article>{after}'
        script_tag = (f'<script defer src="../_assets/{script_name}"></script>' if doc.toc else '') + f'<script defer src="../_assets/{search_name}"></script>'
        page = shell(template, doc.title, doc.summary, doc.url, content, [(TITLE, URL), (doc.title, doc.url)], side_nav(docs, doc, '../'), contents(doc), script_tag, doc.extra_css, '../')
        (folder / 'index.html').write_text(embed(page, '../../blog/'))
    print(f'Built docs index and {len(docs)} pages')
