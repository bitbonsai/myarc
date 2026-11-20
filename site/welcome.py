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

# (title, text, docs link or None, art). Only features with their own docs page or section link out.
CARDS = [
    ('Spaces and Profiles', 'A Space holds the tabs for one part of your life, like Work or Travel. A Profile keeps your logins apart, and several Spaces can share one.', '/docs/spaces-and-profiles/', 'spaces'),
    ('Peek', 'A link from a pinned tab or Favorite to another site opens in a card over the page. Press ⌘O to keep it as a tab.', '/docs/peek/', 'peek'),
    ('Split View', 'Press ⌃⇧= to put two tabs side by side. The pair shows as one row in the sidebar and comes back after a restart.', None, 'split'),
    ('Little Arc', 'Links from other apps open in a small window, and ⌥⌘N opens one from anywhere. Press ⌘O to move the page into your current Space.', None, 'little'),
    ('Command bar', '⌘T opens it for a new tab and ⌘L on the current address. Type an action like pin or split to run it, or / to list them all.', None, 'command'),
    ('Pinned tabs and Favorites', 'Press ⌘D to pin a tab to its Space. Favorites show in every Space of a Profile, and closing either one keeps its tile.', '/docs/spaces-and-profiles/#pinned-tabs-and-favorites', 'pinned'),
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
    .journal.welcome { max-width: 68rem }
    .welcome { --rb-blue: #1a85ff; --rb-violet: #7c6cff; --rb-pink: #f25cbf; --rb-coral: #ff7a5c }
    .welcome h2 { margin-top: 4.5rem; font-size: 1.5rem }

    /* Hero. Title on the left, the homepage ribbon on the right over a soft glow in its own colours. */
    .welcome .welcome-hero { position: relative; display: grid; grid-template-columns: minmax(0, 1fr) minmax(0, 24rem); align-items: center; gap: 2rem 3rem; margin-bottom: 1.5rem }
    .welcome-hero::before { content: ''; position: absolute; z-index: -1; top: -6rem; right: -8rem; width: 44rem; height: 34rem; pointer-events: none; filter: blur(3rem); opacity: .5;
      background: radial-gradient(40% 45% at 35% 40%, rgb(26 133 255 / .45), transparent 70%), radial-gradient(35% 40% at 65% 35%, rgb(124 108 255 / .4), transparent 70%), radial-gradient(40% 40% at 55% 70%, rgb(255 122 92 / .35), transparent 70%) }
    .welcome h1 { font-size: 4.25rem; max-width: 12ch }
    .welcome-intro { margin-top: 1.25rem; max-width: 40ch; color: var(--quiet); font-size: 1.25rem; line-height: 1.5 }
    .welcome .hero-art { --shift: 0%; max-width: 24rem; justify-self: end }

    /* Features. Big art stages alternate sides with the copy, Zen-style. Each stage is a mesh of the ribbon colours,
       a pane of frosted glass and a mini myArc window drawn in SVG. */
    .welcome-cards, .welcome-keys, .welcome-links { margin: 1.25rem 0 0; padding: 0; list-style: none }
    .welcome-cards { display: grid; gap: 4.5rem; margin-top: 2.5rem }
    .welcome-cards > li { display: grid; grid-template-columns: minmax(0, 7fr) minmax(0, 5fr); align-items: center; gap: 2rem 4rem; animation: rise .9s var(--ease) both; animation-delay: calc(.45s + var(--i) * .09s) }
    .welcome-cards > li:nth-child(even) { grid-template-columns: minmax(0, 5fr) minmax(0, 7fr) }
    .welcome-cards > li:nth-child(even) .feature-art { order: 2 }
    .feature-art { position: relative; isolation: isolate; display: grid; place-items: center; padding: 1.5rem 2rem; overflow: hidden; border-radius: 1.75rem; background: rgb(var(--link-rgb) / .05); box-shadow: inset 0 0 0 .0625rem rgb(255 255 255 / .35), 0 1.75rem 3.5rem -2rem rgb(16 28 43 / .45) }
    .feature-art::before { content: ''; position: absolute; z-index: -2; inset: -12%; transition: transform 1.2s var(--ease);
      background: radial-gradient(45% 55% at var(--m1, 14% 20%), rgb(26 133 255 / .6), transparent 70%), radial-gradient(45% 50% at var(--m2, 88% 14%), rgb(124 108 255 / .5), transparent 70%), radial-gradient(50% 55% at var(--m3, 84% 90%), rgb(255 122 92 / .55), transparent 70%), radial-gradient(45% 50% at var(--m4, 16% 92%), rgb(242 92 191 / .45), transparent 70%) }
    .feature-art::after { content: ''; position: absolute; z-index: -1; inset: 0; background-image: var(--grain), var(--grain); background-size: 16rem 16rem; mix-blend-mode: overlay; pointer-events: none }
    .welcome-cards > li:nth-child(3n + 2) .feature-art { --m1: 86% 18%; --m2: 12% 70%; --m3: 70% 96%; --m4: 40% 6% }
    .welcome-cards > li:nth-child(3n) .feature-art { --m1: 50% 100%; --m2: 8% 12%; --m3: 96% 40%; --m4: 60% 10% }
    .glass { position: absolute; z-index: -1; inset: 16% 5% 6% 18%; border-radius: 1.25rem; background: rgb(255 255 255 / .26); box-shadow: inset 0 0 0 .0625rem rgb(255 255 255 / .5); -webkit-backdrop-filter: blur(.75rem); backdrop-filter: blur(.75rem); transition: transform 1s var(--ease) }
    .welcome-cards > li:nth-child(even) .glass { inset: 6% 18% 16% 5% }
    .ui { display: block; width: 100%; max-width: 30rem; height: auto; overflow: visible; transition: transform .8s var(--ease) }
    .ui .float { transition: transform .8s var(--ease) }
    .welcome-cards > li:hover .feature-art::before, .welcome-cards > li:focus-within .feature-art::before { transform: rotate(8deg) scale(1.1) }
    .welcome-cards > li:hover .glass, .welcome-cards > li:focus-within .glass { transform: translate(-.5rem, .5rem) }
    .welcome-cards > li:hover .ui, .welcome-cards > li:focus-within .ui { transform: translateY(-.375rem) rotate(-.6deg) }
    .welcome-cards > li:hover .ui .float, .welcome-cards > li:focus-within .ui .float { transform: translate(4px, -8px) }

    /* Mini-UI paint. Light first, dark below. */
    .ui .ui-win { fill: #fbfaf7; stroke: rgb(24 36 50 / .08) }
    .ui .ui-pane { fill: #fff }
    .ui .ui-row { fill: rgb(24 36 50 / .08) }
    .ui .ui-row-faint { fill: rgb(24 36 50 / .04) }
    .ui .ui-sel { fill: rgb(255 255 255 / .9) }
    .ui .ui-line { fill: rgb(24 36 50 / .14) }
    .ui .ui-ink { fill: rgb(24 36 50 / .6) }
    .ui .ui-dim { fill: rgb(16 28 43 / .16) }
    .ui .glyph { fill: none; stroke: #fff; stroke-width: 1.6; stroke-linecap: round; stroke-linejoin: round }
    .ui .glyph-fill { fill: #fff }
    .ui .f-blue { fill: var(--rb-blue) }
    .ui .f-violet { fill: var(--rb-violet) }
    .ui .f-pink { fill: var(--rb-pink) }
    .ui .f-coral { fill: var(--rb-coral) }
    .ui .side { fill-opacity: .2 }
    .ui .tint { fill-opacity: .3 }
    .ui .tint-strong { fill-opacity: .16 }
    .ui .ghost { opacity: .6 }
    .ui .tl-r { fill: #ff5f57 }
    .ui .tl-y { fill: #febc2e }
    .ui .tl-g { fill: #28c840 }
    .ui .caret { animation: caret 1.1s steps(2, jump-none) infinite }
    @keyframes caret { 50% { opacity: 0 } }
    @media (prefers-color-scheme: dark) {
      .feature-art { background: rgb(var(--link-rgb) / .04); box-shadow: inset 0 0 0 .0625rem rgb(255 255 255 / .1), 0 1.75rem 3.5rem -2rem rgb(0 0 0 / .7) }
      .feature-art::before { opacity: .9 }
      .glass { background: rgb(16 28 43 / .25); box-shadow: inset 0 0 0 .0625rem rgb(255 255 255 / .14) }
      .ui .ui-win { fill: #1a2635; stroke: rgb(255 255 255 / .08) }
      .ui .ui-pane { fill: #223145 }
      .ui .ui-row { fill: rgb(255 255 255 / .09) }
      .ui .ui-row-faint { fill: rgb(255 255 255 / .04) }
      .ui .ui-sel { fill: rgb(255 255 255 / .14) }
      .ui .ui-line { fill: rgb(255 255 255 / .16) }
      .ui .ui-ink { fill: rgb(255 255 255 / .62) }
      .ui .ui-dim { fill: rgb(0 0 0 / .35) }
      .ui .side { fill-opacity: .3 }
      .ui .tint { fill-opacity: .42 }
      .ui .tint-strong { fill-opacity: .3 }
    }
    .feature-copy h3 { font-size: 2rem; line-height: 1.15 }
    .feature-copy h3 a { color: var(--link-solid); text-decoration: none }
    .feature-copy h3 a::after { content: '\\2192' / ''; display: inline-block; margin-left: .3em; transition: transform .3s var(--ease) }
    .feature-copy h3 a:hover, .feature-copy h3 a:focus-visible { text-decoration: underline }
    .feature-copy h3 a:hover::after, .feature-copy h3 a:focus-visible::after { transform: translateX(.25rem) }
    .feature-copy p { margin-top: .75rem; max-width: 34ch; color: var(--quiet); font-size: 1.125rem; line-height: 1.55 }

    .welcome-keys { display: grid; grid-template-columns: repeat(auto-fill, minmax(14rem, 1fr)); gap: .75rem 1.5rem }
    .welcome-keys li { display: flex; align-items: baseline; gap: .75rem; font-size: 1rem }
    .welcome kbd { min-width: 4.5rem; padding: .05em .4em; border: 1px solid color-mix(in srgb, currentColor 18%, transparent); border-bottom-width: 2px; border-radius: .375rem; background: color-mix(in srgb, currentColor 6%, transparent); font-family: inherit; font-size: 1rem; font-weight: 500; text-align: center; white-space: nowrap }
    .welcome-cards kbd { min-width: 0 }
    .welcome-more { margin-top: 1rem; font-size: 1rem }
    .welcome-links { display: grid; grid-template-columns: repeat(auto-fill, minmax(17rem, 1fr)); gap: 1rem 1.5rem }
    .welcome-links a { font-weight: 600 }
    .welcome-links p { margin-top: .25rem; color: var(--quiet); font-size: 1rem }

    @media (max-width: 48rem) {
      .welcome .welcome-hero { grid-template-columns: minmax(0, 1fr) }
      .welcome .hero-art { justify-self: center; max-width: 16rem }
      .welcome h1 { font-size: 3rem }
      .welcome-cards { gap: 3rem }
      .welcome-cards > li, .welcome-cards > li:nth-child(even) { grid-template-columns: minmax(0, 1fr); gap: 1.5rem }
      .welcome-cards > li:nth-child(even) .feature-art { order: 0 }
      .feature-art { padding: 1rem; border-radius: 1.25rem }
      .feature-copy h3 { font-size: 1.5rem }
    }
    @media (prefers-reduced-motion: reduce) {
      .welcome-cards > li, .ui .caret { animation: none }
      .feature-art::before, .glass, .ui, .ui .float, .feature-copy h3 a::after { transition: none }
    }
'''


# Card art: a mini myArc window drawn in SVG over a soft mesh in the ribbon colours.
# Shapes carry classes only, so the page CSS paints them for light and dark.
def _tabs(x, y, width, count, active=0, start=0):
    hues = ['blue', 'coral', 'violet', 'pink']
    rows = ''
    for i in range(count):
        top = y + i * 15
        rows += (f'<rect class="{"ui-sel" if i == active else "ui-row"}" x="{x}" y="{top}" width="{width}" height="11" rx="4"/>'
                 f'<rect class="f-{hues[(i + start) % 4]}" x="{x + 4}" y="{top + 3}" width="5" height="5" rx="1.5"/>'
                 f'<rect class="ui-line" x="{x + 13}" y="{top + 4.5}" width="{width * (.62 - .1 * (i % 2)):.0f}" height="2" rx="1"/>')
    return rows


def _page(x, y, width, hue='blue'):
    return (f'<rect class="ui-ink" x="{x}" y="{y}" width="{width * .5:.0f}" height="7" rx="3"/>'
            f'<rect class="ui-line" x="{x}" y="{y + 15}" width="{width * .86:.0f}" height="3" rx="1.5"/>'
            f'<rect class="ui-line" x="{x}" y="{y + 23}" width="{width * .7:.0f}" height="3" rx="1.5"/>'
            f'<rect class="tint f-{hue}" x="{x}" y="{y + 36}" width="{width:.0f}" height="58" rx="6"/>'
            f'<rect class="ui-line" x="{x}" y="{y + 104}" width="{width * .8:.0f}" height="3" rx="1.5"/>'
            f'<rect class="ui-line" x="{x}" y="{y + 112}" width="{width * .55:.0f}" height="3" rx="1.5"/>')


def _window(x, y, width, height, side=86, hue='blue', pane=None, tabs=None):
    """A window frame with a tinted sidebar, traffic lights, an address pill and tab rows."""
    pane_x = x + side + 4
    out = (f'<rect class="ui-win" x="{x}" y="{y}" width="{width}" height="{height}" rx="12"/>'
           f'<rect class="side f-{hue}" x="{x + 5}" y="{y + 5}" width="{side - 5}" height="{height - 10}" rx="8"/>'
           f'<circle class="tl-r" cx="{x + 15}" cy="{y + 15}" r="2.6"/><circle class="tl-y" cx="{x + 24}" cy="{y + 15}" r="2.6"/><circle class="tl-g" cx="{x + 33}" cy="{y + 15}" r="2.6"/>'
           f'<rect class="ui-row" x="{x + 11}" y="{y + 26}" width="{side - 17}" height="10" rx="5"/>')
    out += tabs if tabs is not None else _tabs(x + 11, y + 46, side - 17, 4)
    out += f'<rect class="ui-pane" x="{pane_x}" y="{y + 5}" width="{x + width - 5 - pane_x}" height="{height - 10}" rx="8"/>'
    out += pane if pane is not None else _page(pane_x + 14, y + 20, x + width - pane_x - 33, hue)
    return out


def _space_dots(x, y, active):
    return ''.join(f'<circle class="{"f-" + hue if hue == active else "ui-line"}" cx="{x + i * 11}" cy="{y}" r="3"/>' for i, hue in enumerate(('blue', 'coral', 'violet')))


def _art(kind, n):
    shadow = f'<defs><filter id="lift-{n}" x="-20%" y="-20%" width="140%" height="150%"><feDropShadow dx="0" dy="8" stdDeviation="9" flood-color="#101c2b" flood-opacity=".28"/></filter></defs>'
    if kind == 'spaces':
        # Two more Spaces stack behind the current one, each with its own colour.
        body = (f'<rect class="f-violet" x="12" y="14" width="80" height="196" rx="10" opacity=".45"/>'
                f'<rect class="f-coral" x="25" y="28" width="80" height="196" rx="10" opacity=".6"/>'
                f'<g filter="url(#lift-{n})">{_window(38, 42, 300, 196, hue="blue")}</g>'
                + _space_dots(60, 226, 'blue')
                + '<circle class="f-coral" cx="112" cy="57" r="5"/><circle class="ui-pane" cx="112" cy="57" r="2"/>')
    elif kind == 'peek':
        card = (f'<g class="float" filter="url(#lift-{n})"><rect class="ui-pane" x="150" y="62" width="150" height="148" rx="10"/>'
                '<rect class="ui-row" x="160" y="71" width="44" height="8" rx="4"/><circle class="f-blue" cx="287" cy="75" r="4"/>'
                + _page(162, 90, 126, 'pink') + '</g>')
        body = _window(30, 30, 340, 210, hue='violet', pane=_page(132, 44, 220, 'blue') + '<rect class="ui-dim" x="120" y="35" width="245" height="200" rx="8"/>') + card
    elif kind == 'split':
        pane = ('<rect class="ui-pane" x="120" y="35" width="122" height="200" rx="8"/><rect class="ui-pane" x="246" y="35" width="119" height="200" rx="8"/>'
                + _page(132, 50, 98, 'coral') + _page(258, 50, 95, 'blue'))
        tabs = _tabs(41, 91, 69, 3, active=-1, start=1)
        tabs += ('<rect class="ui-sel" x="41" y="76" width="69" height="11" rx="4"/>'
                 '<rect class="f-coral" x="45" y="79" width="5" height="5" rx="1.5"/><rect class="f-blue" x="53" y="79" width="5" height="5" rx="1.5"/>'
                 '<rect class="ui-line" x="62" y="80.5" width="34" height="2" rx="1"/>')
        body = (f'<g filter="url(#lift-{n})"><rect class="ui-win" x="30" y="30" width="340" height="210" rx="12"/>'
                '<rect class="side f-coral" x="35" y="35" width="81" height="200" rx="8"/>'
                '<circle class="tl-r" cx="45" cy="45" r="2.6"/><circle class="tl-y" cx="54" cy="45" r="2.6"/><circle class="tl-g" cx="63" cy="45" r="2.6"/>'
                '<rect class="ui-row" x="41" y="56" width="69" height="10" rx="5"/>'
                f'{tabs}{pane}</g>')
    elif kind == 'little':
        small = (f'<g class="float" filter="url(#lift-{n})"><rect class="ui-win" x="150" y="78" width="200" height="150" rx="11"/>'
                 '<circle class="tl-r" cx="162" cy="90" r="2.6"/><circle class="tl-y" cx="171" cy="90" r="2.6"/><circle class="tl-g" cx="180" cy="90" r="2.6"/>'
                 '<rect class="ui-row" x="190" y="85" width="92" height="10" rx="5"/>'
                 '<rect class="f-blue" x="290" y="84" width="50" height="12" rx="6"/><rect class="glyph-fill" x="300" y="89" width="22" height="2" rx="1"/><path class="glyph" d="M327 87.5l3 2.5-3 2.5"/>'
                 '<rect class="ui-pane" x="156" y="102" width="188" height="120" rx="7"/>' + _page(168, 114, 164, 'coral') + '</g>')
        body = '<g class="ghost">' + _window(30, 26, 290, 186, hue='blue') + '</g>' + small
    elif kind == 'command':
        rows = ''
        for i, (hue, width) in enumerate((('violet', 70), ('blue', 92), ('coral', 58))):
            top = 126 + i * 24
            rows += (f'<rect class="{"tint-strong f-violet" if i == 0 else "ui-row-faint"}" x="110" y="{top}" width="180" height="19" rx="6"/>'
                     f'<rect class="f-{hue}" x="118" y="{top + 5}" width="9" height="9" rx="2.5"/>'
                     f'<rect class="ui-line" x="134" y="{top + 8}" width="{width}" height="3" rx="1.5"/>'
                     f'<rect class="ui-row" x="262" y="{top + 5}" width="20" height="9" rx="3"/>')
        palette = (f'<g class="float" filter="url(#lift-{n})"><rect class="ui-pane" x="100" y="80" width="200" height="128" rx="12"/>'
                   '<rect class="f-blue" x="112" y="94" width="11" height="15" rx="3"/><path class="glyph" d="M120 97.5l-4 8"/>'
                   '<rect class="caret f-blue" x="131" y="94" width="2" height="15" rx="1"/>'
                   '<rect class="ui-line" x="100" y="117" width="200" height="1"/>' + rows + '</g>')
        body = _window(30, 30, 340, 210, hue='pink') + '<rect class="ui-dim" x="30" y="30" width="340" height="210" rx="12"/>' + palette
    else:
        tiles = ''
        for i, hue in enumerate(('blue', 'coral', 'violet', 'pink', 'coral', 'blue')):
            tx, ty = 41 + (i % 3) * 27, 72 + (i // 3) * 22
            tiles += f'<rect class="ui-sel" x="{tx}" y="{ty}" width="23" height="18" rx="5"/><circle class="f-{hue}" cx="{tx + 11.5}" cy="{ty + 9}" r="4"/>'
        tabs = tiles + '<rect class="ui-line" x="41" y="124" width="77" height="1"/>' + _tabs(41, 132, 77, 3, start=2)
        body = f'<g filter="url(#lift-{n})">' + _window(30, 30, 340, 210, side=94, hue='blue', tabs=tabs) + '</g>'
    return f'<svg class="ui" viewBox="0 0 400 260" aria-hidden="true" focusable="false">{shadow}{body}</svg>'


def keys(text):
    """Wrap each run of Mac key glyphs in kbd."""
    return re.sub(r'[⌘⌃⌥⇧]+[A-Z0-9=–]+', lambda match: f'<kbd>{match[0]}</kbd>', escape(text))


def content():
    cards = ''
    for n, (title, text, link, art) in enumerate(CARDS):
        heading = f'<a href="{escape(link, quote=True)}">{escape(title)}</a>' if link else escape(title)
        cards += (f'<li style="--i: {n}"><div class="feature-art" aria-hidden="true"><span class="glass"></span>{_art(art, n)}</div>'
                  f'<div class="feature-copy"><h3>{heading}</h3><p>{keys(text)}</p></div></li>')
    shortcuts = ''.join(f'<li><kbd>{key}</kbd><span>{escape(label)}</span></li>' for key, label in KEYS)
    links = ''.join(f'<li><a href="{escape(href, quote=True)}">{escape(label)}</a><p>{escape(text)}</p></li>' for label, href, text in LINKS)
    # @@RIBBON@@ is the homepage hero ribbon; embed() inlines the one shared copy.
    return (f'<header class="post-header welcome-hero"><div class="welcome-hero-copy"><h1 class="rise">{TITLE}</h1><p class="welcome-intro rise">{escape(INTRO)}</p></div>@@RIBBON@@</header>'
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
