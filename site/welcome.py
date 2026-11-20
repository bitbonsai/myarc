"""Build the quick start page at /welcome/, the first tab after the welcome tour on a new profile."""
from html import escape
from pathlib import Path
import json
import re

from blog import page_shell

URL = 'https://myarc.dev/welcome/'
REPO = 'https://github.com/bitbonsai/myarc'
TITLE = 'Welcome to myArc'
INTRO = "myArc is an open-source browser that keeps Arc's way of browsing, with no account to sign up for. Here's where to start."
SUMMARY = 'Quick start for myArc: Spaces, Peek, Split View, Little Arc, the command bar, pinned tabs and the shortcuts to learn first.'

# (title, text, docs link or None). Only features with their own docs page or section link out.
CARDS = [
    ('Spaces and Profiles', 'A Space holds the tabs for one part of your life, like Work or Travel. A Profile keeps your logins apart, and several Spaces can share one.', '/docs/spaces-and-profiles/'),
    ('Peek', 'A link from a pinned tab or Favorite to another site opens in a card over the page. Press ⌘O to keep it as a tab.', '/docs/peek/'),
    ('Split View', 'Press ⌃⇧= to put two tabs side by side. The pair shows as one row in the sidebar and comes back after a restart.', None),
    ('Little Arc', 'Links from other apps open in a small window, and ⌥⌘N opens one from anywhere. Press ⌘O to move the page into your current Space.', None),
    ('Command bar', '⌘T opens it for a new tab and ⌘L on the current address. Type an action like pin or split to run it, or / to list them all.', None),
    ('Pinned tabs and Favorites', 'Press ⌘D to pin a tab to its Space. Favorites show in every Space of a Profile, and closing either one keeps its tile.', '/docs/spaces-and-profiles/#pinned-tabs-and-favorites'),
]

KEYS = [
    ('⌘T', 'New tab'),
    ('⌘L', 'Edit the address'),
    ('⌘D', 'Pin or unpin the tab'),
    ('⌃1–9', 'Switch Spaces'),
    ('⌥⌘N', 'Open Little Arc'),
    ('⌃⇧=', 'Open a Split View'),
    ('⌃⌥⌘A', 'Replay the welcome'),
]

LINKS = [
    ('Import from Arc', '/docs/import-from-arc/', 'Bring over your Spaces, folders and pinned tabs.'),
    ('Docs', '/docs/', 'How everything works, and what is still missing.'),
    ('Release notes', '/download/', 'What works in this nightly, and the latest download.'),
    ('GitHub', REPO, 'The code, the plans and the roadmap.'),
    ('Report an issue', REPO + '/issues', 'Found a bug? Tell me what happened.'),
]

CSS = '''
    .journal.welcome { max-width: 60rem }
    .welcome .post-header { margin-bottom: 3rem }
    .welcome-intro { margin-top: 1rem; max-width: 40ch; color: var(--quiet); font-size: 1.25rem; line-height: 1.5 }
    .welcome h2 { margin-top: 3.5rem; font-size: 1.5rem }
    .welcome-cards, .welcome-keys, .welcome-links { margin: 1.25rem 0 0; padding: 0; list-style: none }
    .welcome-cards { display: grid; grid-template-columns: repeat(auto-fill, minmax(17rem, 1fr)); gap: 1rem }
    .welcome-cards li { padding: 1.25rem 1.5rem 1.5rem; border-radius: 1rem; background: rgb(var(--link-rgb) / .06) }
    .welcome-cards h3 { font-size: 1.25rem }
    .welcome-cards h3 a { color: var(--link-solid); text-decoration: none }
    .welcome-cards h3 a::after { content: ' \\2192' / '' }
    .welcome-cards h3 a:hover, .welcome-cards h3 a:focus-visible { text-decoration: underline }
    .welcome-cards p { margin-top: .5rem; font-size: 1rem; line-height: 1.5 }
    .welcome-keys { display: grid; grid-template-columns: repeat(auto-fill, minmax(14rem, 1fr)); gap: .75rem 1.5rem }
    .welcome-keys li { display: flex; align-items: baseline; gap: .75rem; font-size: 1rem }
    .welcome kbd { min-width: 4.5rem; padding: .05em .4em; border: 1px solid color-mix(in srgb, currentColor 18%, transparent); border-bottom-width: 2px; border-radius: .375rem; background: color-mix(in srgb, currentColor 6%, transparent); font-family: inherit; font-size: 1rem; font-weight: 500; text-align: center; white-space: nowrap }
    .welcome-cards kbd { min-width: 0 }
    .welcome-more { margin-top: 1rem; font-size: 1rem }
    .welcome-links { display: grid; grid-template-columns: repeat(auto-fill, minmax(17rem, 1fr)); gap: 1rem 1.5rem }
    .welcome-links a { font-weight: 600 }
    .welcome-links p { margin-top: .25rem; color: var(--quiet); font-size: 1rem }
'''


def keys(text):
    """Wrap each run of Mac key glyphs in kbd."""
    return re.sub(r'[⌘⌃⌥⇧]+[A-Z0-9=–]+', lambda match: f'<kbd>{match[0]}</kbd>', escape(text))


def content():
    cards = ''
    for title, text, link in CARDS:
        heading = f'<a href="{escape(link, quote=True)}">{escape(title)}</a>' if link else escape(title)
        cards += f'<li><h3>{heading}</h3><p>{keys(text)}</p></li>'
    shortcuts = ''.join(f'<li><kbd>{key}</kbd><span>{escape(label)}</span></li>' for key, label in KEYS)
    links = ''.join(f'<li><a href="{escape(href, quote=True)}">{escape(label)}</a><p>{escape(text)}</p></li>' for label, href, text in LINKS)
    return (f'<header class="post-header"><h1>{TITLE}</h1><p class="welcome-intro">{escape(INTRO)}</p></header>'
            '<div class="welcome-body">'
            f'<h2>The basics</h2><ul class="welcome-cards">{cards}</ul>'
            f'<h2>Shortcuts to learn first</h2><ul class="welcome-keys">{shortcuts}</ul>'
            '<p class="welcome-more"><a href="/docs/shortcuts/">All shortcuts</a></p>'
            f'<h2>Keep going</h2><ul class="welcome-links">{links}</ul>'
            '</div>')


def build_welcome(root, template, embed):
    page = page_shell(template, TITLE, SUMMARY, URL, content())
    schema = {'@context': 'https://schema.org', '@graph': [
        {'@type': 'WebPage', 'name': TITLE, 'description': SUMMARY, 'url': URL, 'inLanguage': 'en', 'isPartOf': {'@type': 'WebSite', 'name': 'myArc', 'url': 'https://myarc.dev/'}},
        {'@type': 'BreadcrumbList', 'itemListElement': [{'@type': 'ListItem', 'position': 1, 'name': 'myArc', 'item': 'https://myarc.dev/'}, {'@type': 'ListItem', 'position': 2, 'name': 'Welcome', 'item': URL}]}]}
    page = re.sub(r'(<script type="application/ld\+json">).*?(</script>)', lambda match: match[1] + json.dumps(schema).replace('<', '\\u003c') + match[2], page, count=1, flags=re.S)
    # A browser start page, not a search result: noindex, no Markdown twin, outside the sitemap and nav.
    page = re.sub(r'<meta name="robots" content="[^"]*">', '<meta name="robots" content="noindex">', page, count=1)
    page = page.replace(f'<link rel="alternate" type="text/markdown" href="{URL}index.md" title="Markdown">\n', '', 1)
    page = page.replace(' aria-current="page"', '').replace('<main id="main" class="journal">', '<main id="main" class="journal welcome">', 1)
    page = page.replace('</style>', CSS + '  </style>', 1)
    folder = Path(root) / 'welcome'
    folder.mkdir(exist_ok=True)
    # Shared hashed assets live with the blog, which the build always creates first.
    (folder / 'index.html').write_text(embed(page, '../blog/'))
