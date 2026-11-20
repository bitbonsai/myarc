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
    .welcome .welcome-hero { position: relative; display: grid; grid-template-columns: minmax(0, 1fr) minmax(0, 22rem); align-items: center; gap: 2rem 3rem; margin-bottom: 1rem }
    .welcome-hero::before { content: ''; position: absolute; z-index: -1; top: -6rem; right: -8rem; width: 44rem; height: 34rem; pointer-events: none; filter: blur(3rem); opacity: .5;
      background: radial-gradient(40% 45% at 35% 40%, rgb(26 133 255 / .45), transparent 70%), radial-gradient(35% 40% at 65% 35%, rgb(124 108 255 / .4), transparent 70%), radial-gradient(40% 40% at 55% 70%, rgb(255 122 92 / .35), transparent 70%) }
    .welcome h1 { font-size: 4.25rem; max-width: 12ch }
    .welcome-intro { margin-top: 1.25rem; max-width: 40ch; color: var(--quiet); font-size: 1.25rem; line-height: 1.5 }
    .welcome .hero-art { --shift: 0%; max-width: 22rem; justify-self: end }

    /* The tour, after Zen's welcome: a short list on the left, one sticky stage on the right.
       Picking a feature swaps the stage to its scene. Without script the first scene stays as a still. */
    .tour { display: grid; grid-template-columns: minmax(0, 5fr) minmax(0, 7fr); align-items: start; gap: 2rem 3.5rem; margin-top: 1.5rem }
    .welcome-cards { display: grid; gap: .25rem; margin: 0; padding: 0; list-style: none }
    .welcome-cards > li { position: relative; padding: 1rem 1.25rem 1.125rem; border-radius: 1rem; transition: background-color .25s ease-in-out; animation: rise .9s var(--ease) both; animation-delay: calc(.4s + var(--i) * .07s) }
    .welcome-cards > li:hover { background: rgb(var(--link-rgb) / .045) }
    .welcome-cards > li.is-active { background: rgb(var(--link-rgb) / .09) }
    .welcome-cards h3 { font-size: 1.25rem; line-height: 1.25 }
    .welcome-cards h3 button { padding: 0; border: 0; background: none; color: inherit; font: inherit; text-align: left; cursor: pointer }
    /* The whole card is the button's hit area; the docs link sits above it. */
    .welcome-cards h3 button::after { content: ''; position: absolute; inset: 0; border-radius: 1rem }
    .welcome-cards h3 button:focus-visible { outline: 0 }
    .welcome-cards h3 button:focus-visible::after { outline: .1875rem solid var(--link-solid); outline-offset: .125rem }
    .welcome-cards p { margin-top: .375rem; color: var(--quiet); font-size: 1rem; line-height: 1.5 }
    .welcome-cards kbd { min-width: 0 }
    .tour-docs { position: relative; z-index: 1; display: inline-block; margin-top: .375rem; font-size: 1rem }

    .tour-stage { position: sticky; top: 2rem; isolation: isolate; aspect-ratio: 400 / 290; overflow: hidden; border-radius: 1.75rem; background: linear-gradient(rgb(var(--link-rgb) / .05), rgb(var(--link-rgb) / .05)) var(--page); box-shadow: inset 0 0 0 .0625rem rgb(255 255 255 / .35), 0 1.75rem 3.5rem -2rem rgb(16 28 43 / .45); animation: rise 1s var(--ease) .3s both }
    .tour-stage::before { content: ''; position: absolute; z-index: -2; inset: -15%; transform: rotate(calc(var(--turn, 0) * 1deg)) scale(1.05); transition: transform 1.4s var(--ease);
      background: radial-gradient(45% 55% at 14% 20%, rgb(26 133 255 / .6), transparent 70%), radial-gradient(45% 50% at 88% 14%, rgb(124 108 255 / .5), transparent 70%), radial-gradient(50% 55% at 84% 90%, rgb(255 122 92 / .55), transparent 70%), radial-gradient(45% 50% at 16% 92%, rgb(242 92 191 / .45), transparent 70%) }
    .tour-stage::after { content: ''; position: absolute; z-index: -1; inset: 0; background-image: var(--grain), var(--grain); background-size: 16rem 16rem; mix-blend-mode: overlay; pointer-events: none }
    .glass { position: absolute; z-index: -1; inset: 15% 5% 7% 17%; border-radius: 1.25rem; background: rgb(255 255 255 / .26); box-shadow: inset 0 0 0 .0625rem rgb(255 255 255 / .5); -webkit-backdrop-filter: blur(.75rem); backdrop-filter: blur(.75rem) }
    .scene { position: absolute; inset: 0; display: grid; place-items: center; padding: 1.25rem 1.75rem; opacity: 0; transform: translateY(.75rem) scale(.97); transition: opacity .35s ease-in-out, transform .5s var(--ease) }
    .scene.is-active { opacity: 1; transform: none }
    .ui { display: block; width: 100%; height: auto; overflow: visible }

    /* Mini-UI paint. Light first, dark below. */
    .ui .ui-win { fill: #fbfaf7; stroke: rgb(24 36 50 / .08) }
    .ui .ui-pane { fill: #fff }
    .ui .ui-row { fill: rgb(24 36 50 / .08) }
    .ui .ui-row-faint { fill: rgb(24 36 50 / .04) }
    .ui .ui-sel { fill: rgb(255 255 255 / .9) }
    .ui .ui-line { fill: rgb(24 36 50 / .14) }
    .ui .ui-slot { fill: none; stroke: rgb(24 36 50 / .22); stroke-dasharray: 3 2 }
    .ui .ui-ink { fill: rgb(24 36 50 / .6) }
    .ui .ui-dim { fill: rgb(16 28 43 / .16) }
    .ui .ui-app { fill: #2b3442 }
    .ui .ui-app-row { fill: rgb(255 255 255 / .12) }
    .ui .glyph { fill: none; stroke: #fff; stroke-width: 1.6; stroke-linecap: round; stroke-linejoin: round }
    .ui .glyph-fill { fill: #fff }
    .ui .key { fill: var(--ink); font-size: 9px; font-weight: 600 }
    .ui .f-blue { fill: var(--rb-blue) }
    .ui .f-violet { fill: var(--rb-violet) }
    .ui .f-pink { fill: var(--rb-pink) }
    .ui .f-coral { fill: var(--rb-coral) }
    .ui .side { fill-opacity: .2 }
    .ui .tint { fill-opacity: .3 }
    .ui .tint-strong { fill-opacity: .16 }
    .ui .tl-r { fill: #ff5f57 }
    .ui .tl-y { fill: #febc2e }
    .ui .tl-g { fill: #28c840 }
    @media (prefers-color-scheme: dark) {
      .tour-stage { background: linear-gradient(rgb(var(--link-rgb) / .04), rgb(var(--link-rgb) / .04)) var(--page); box-shadow: inset 0 0 0 .0625rem rgb(255 255 255 / .1), 0 1.75rem 3.5rem -2rem rgb(0 0 0 / .7) }
      .tour-stage::before { opacity: .9 }
      .glass { background: rgb(16 28 43 / .25); box-shadow: inset 0 0 0 .0625rem rgb(255 255 255 / .14) }
      .ui .ui-win { fill: #1a2635; stroke: rgb(255 255 255 / .08) }
      .ui .ui-pane { fill: #223145 }
      .ui .ui-row { fill: rgb(255 255 255 / .09) }
      .ui .ui-row-faint { fill: rgb(255 255 255 / .04) }
      .ui .ui-sel { fill: rgb(255 255 255 / .14) }
      .ui .ui-line { fill: rgb(255 255 255 / .16) }
      .ui .ui-slot { stroke: rgb(255 255 255 / .28) }
      .ui .ui-ink { fill: rgb(255 255 255 / .62) }
      .ui .ui-dim { fill: rgb(0 0 0 / .35) }
      .ui .ui-app { fill: #0f1722 }
      .ui .key { fill: var(--ivory) }
      .ui .side { fill-opacity: .3 }
      .ui .tint { fill-opacity: .42 }
      .ui .tint-strong { fill-opacity: .3 }
    }

    /* Scene loops, 5s each. The resting style is the last frame, so no script, reduced motion and the
       first paint all show the feature done. The script adds .is-live only when motion is allowed. */
    .ui .pk-card, .ui .sv-a, .ui .sv-b, .ui .la-pop, .ui .la-btn, .ui .cb-pal, .ui .cb-type, .ui .cb-caret, .ui .pn-fly { transform-box: view-box }
    .ui .pk-card { transform-origin: 162px 68px }
    .ui .sv-a { transform-origin: 120px 0 }
    .ui .la-pop { transform-origin: 250px 228px }
    .ui .la-btn { transform-origin: 315px 90px }
    .ui .cb-pal { transform-origin: 200px 144px }
    .ui .cb-type { transform-origin: 130px 0 }
    .ui .pn-row, .ui .pn-fly, .ui .pn-key { opacity: 0 }
    .tour.is-live .is-active .ui * { animation-duration: 5s; animation-iteration-count: infinite; animation-timing-function: var(--ease) }
    .tour.is-live .is-active .sp-side, .tour.is-live .is-active .sp-tint { animation-name: sp-fill }
    .tour.is-live .is-active .sp-tabs { animation-name: sp-tabs }
    .tour.is-live .is-active .sp-dot { animation-name: sp-dot }
    .tour.is-live .is-active .pk-card { animation-name: pk-card }
    .tour.is-live .is-active .pk-dim { animation-name: pk-dim }
    .tour.is-live .is-active .pk-link { animation-name: pk-link }
    .tour.is-live .is-active .sv-a { animation-name: sv-a }
    .tour.is-live .is-active .sv-b { animation-name: sv-b }
    .tour.is-live .is-active .sv-fav { animation-name: sv-fav }
    .tour.is-live .is-active .la-pop { animation-name: la-pop }
    .tour.is-live .is-active .la-btn { animation-name: la-btn }
    .tour.is-live .is-active .la-link { animation-name: pk-link }
    .tour.is-live .is-active .cb-pal { animation-name: cb-pal }
    .tour.is-live .is-active .cb-type { animation-name: cb-type }
    .tour.is-live .is-active .cb-caret { animation-name: cb-caret }
    .tour.is-live .is-active .cb-r1 { animation-name: cb-r1 }
    .tour.is-live .is-active .cb-r2 { animation-name: cb-r2 }
    .tour.is-live .is-active .cb-r3 { animation-name: cb-r3 }
    .tour.is-live .is-active .pn-row { animation-name: pn-row }
    .tour.is-live .is-active .pn-fly { animation-name: pn-fly }
    .tour.is-live .is-active .pn-tile { animation-name: pn-tile }
    .tour.is-live .is-active .pn-key { animation-name: pn-key }
    @keyframes sp-fill { 0%, 28% { fill: var(--rb-coral) } 34%, 61% { fill: var(--rb-violet) } 67%, 100% { fill: var(--rb-blue) } }
    @keyframes sp-tabs { 0%, 28%, 36%, 61%, 69%, 100% { transform: none; opacity: 1 } 31%, 64% { transform: translateX(-10px); opacity: 0 } 32%, 65% { transform: translateX(10px); opacity: 0 } }
    @keyframes sp-dot { 0%, 28% { transform: translateX(11px) } 34%, 61% { transform: translateX(22px) } 67%, 100% { transform: none } }
    @keyframes pk-card { 0%, 14% { transform: scale(.05); opacity: 0 } 28%, 86% { transform: none; opacity: 1 } 96%, 100% { transform: scale(.05); opacity: 0 } }
    @keyframes pk-dim { 0%, 14% { opacity: 0 } 28%, 86% { opacity: 1 } 96%, 100% { opacity: 0 } }
    @keyframes pk-link { 0%, 4% { opacity: .35 } 10%, 100% { opacity: 1 } }
    @keyframes sv-a { 0%, 18% { transform: scaleX(2.008) } 34%, 88% { transform: none } 100% { transform: scaleX(2.008) } }
    @keyframes sv-b { 0%, 18% { transform: translateX(125px); opacity: 0 } 34%, 88% { transform: none; opacity: 1 } 100% { transform: translateX(125px); opacity: 0 } }
    @keyframes sv-fav { 0%, 22% { opacity: 0 } 32%, 88% { opacity: 1 } 100% { opacity: 0 } }
    @keyframes la-pop { 0%, 16% { transform: translateY(14px) scale(.7); opacity: 0 } 30%, 88% { transform: none; opacity: 1 } 98%, 100% { transform: scale(.94); opacity: 0 } }
    @keyframes la-btn { 0%, 52%, 64%, 100% { transform: none } 58% { transform: scale(1.18) } }
    @keyframes cb-pal { 0%, 4% { transform: scale(.94); opacity: 0 } 12%, 90% { transform: none; opacity: 1 } 100% { transform: scale(.98); opacity: 0 } }
    @keyframes cb-type { 0%, 14% { transform: scaleX(0); animation-timing-function: steps(6, end) } 42%, 100% { transform: none } }
    @keyframes cb-caret { 0%, 14% { transform: translateX(-62px); animation-timing-function: steps(6, end) } 42%, 100% { transform: none } }
    @keyframes cb-r1 { 0%, 44% { opacity: 0; transform: translateY(4px) } 50%, 100% { opacity: 1; transform: none } }
    @keyframes cb-r2 { 0%, 49% { opacity: 0; transform: translateY(4px) } 55%, 100% { opacity: 1; transform: none } }
    @keyframes cb-r3 { 0%, 54% { opacity: 0; transform: translateY(4px) } 60%, 100% { opacity: 1; transform: none } }
    @keyframes pn-row { 0%, 22% { opacity: 1 } 34%, 92% { opacity: 0 } 100% { opacity: 1 } }
    @keyframes pn-fly { 0%, 22% { opacity: 0; transform: translate(-59px, 31.5px) } 26% { opacity: 1; transform: translate(-59px, 31.5px) } 48% { opacity: 1; transform: none } 52%, 100% { opacity: 0; transform: none } }
    @keyframes pn-tile { 0%, 46% { opacity: 0 } 52%, 92% { opacity: 1 } 100% { opacity: 0 } }
    @keyframes pn-key { 0%, 8% { opacity: 0 } 14%, 34% { opacity: 1 } 42%, 100% { opacity: 0 } }

    .welcome-keys, .welcome-links { margin: 1.25rem 0 0; padding: 0; list-style: none }
    .welcome-keys { display: grid; grid-template-columns: repeat(auto-fill, minmax(14rem, 1fr)); gap: .75rem 1.5rem }
    .welcome-keys li { display: flex; align-items: baseline; gap: .75rem; font-size: 1rem }
    .welcome kbd { min-width: 4.5rem; padding: .05em .4em; border: 1px solid color-mix(in srgb, currentColor 18%, transparent); border-bottom-width: 2px; border-radius: .375rem; background: color-mix(in srgb, currentColor 6%, transparent); font-family: inherit; font-size: 1rem; font-weight: 500; text-align: center; white-space: nowrap }
    .welcome-cards kbd { min-width: 0 }
    .welcome-more { margin-top: 1rem; font-size: 1rem }
    .welcome-links { display: grid; grid-template-columns: repeat(auto-fill, minmax(17rem, 1fr)); gap: 1rem 1.5rem }
    .welcome-links a { font-weight: 600 }
    .welcome-links p { margin-top: .25rem; color: var(--quiet); font-size: 1rem }

    /* Phones: the stage sits on top and stays in view while the list scrolls under it. */
    @media (max-width: 48rem) {
      .welcome .welcome-hero { grid-template-columns: minmax(0, 1fr) }
      .welcome .hero-art { justify-self: center; max-width: 15rem }
      .welcome h1 { font-size: 3rem }
      .tour { grid-template-columns: minmax(0, 1fr); gap: 1rem }
      /* Below the sticky top bar, which is 4.25rem tall here, flush so no text shows between them. */
      .tour-stage { order: -1; top: 4.25rem; z-index: 1; border-radius: 1.25rem }
      .scene { padding: .75rem 1rem }
      .welcome-cards > li { padding: .875rem 1rem 1rem }
    }
    @media (prefers-reduced-motion: reduce) {
      .welcome-cards > li, .tour-stage { animation: none }
      .tour-stage::before, .scene, .welcome-cards > li { transition: none }
    }
'''

# The tour script. Hover, click or focus picks a feature. It advances on its own every 5 seconds until the
# visitor touches the tour, and stays still under reduced motion.
SCRIPT = """<script>
  (() => {
    const tour = document.querySelector('.tour')
    const items = [...tour.querySelectorAll('.welcome-cards > li')]
    const scenes = [...tour.querySelectorAll('.scene')]
    const stage = tour.querySelector('.tour-stage')
    const still = matchMedia('(prefers-reduced-motion: reduce)').matches
    const buttons = items.map((item) => {
      const heading = item.querySelector('h3')
      const button = document.createElement('button')
      button.type = 'button'
      button.textContent = heading.textContent
      heading.replaceChildren(button)
      return button
    })
    let current = -1, timer = 0
    const show = (index) => {
      if (index === current) return
      current = index
      items.forEach((item, i) => item.classList.toggle('is-active', i === index))
      buttons.forEach((button, i) => button.setAttribute('aria-pressed', String(i === index)))
      scenes.forEach((scene, i) => scene.classList.toggle('is-active', i === index))
      stage.style.setProperty('--turn', String(index * 24))
    }
    const stop = () => clearInterval(timer)
    items.forEach((item, i) => {
      item.addEventListener('pointerenter', (event) => { if (event.pointerType === 'mouse') { stop(); show(i) } })
      item.addEventListener('focusin', () => { stop(); show(i) })
      buttons[i].addEventListener('click', () => { stop(); show(i) })
    })
    show(0)
    if (still) return
    tour.classList.add('is-live')
    timer = setInterval(() => { if (!document.hidden) show((current + 1) % items.length) }, 5000)
  })()
</script>"""


# Scene art: a mini myArc window drawn in SVG. Shapes carry classes only, so the page CSS paints them for
# light and dark and animates them. Coordinates are in a 400 x 260 box.
def _tabs(x, y, width, count, active=0, start=0):
    hues = ['blue', 'coral', 'violet', 'pink']
    rows = ''
    for i in range(count):
        top = y + i * 15
        rows += (f'<rect class="{"ui-sel" if i == active else "ui-row"}" x="{x}" y="{top}" width="{width}" height="11" rx="4"/>'
                 f'<rect class="f-{hues[(i + start) % 4]}" x="{x + 4}" y="{top + 3}" width="5" height="5" rx="1.5"/>'
                 f'<rect class="ui-line" x="{x + 13}" y="{top + 4.5}" width="{width * (.62 - .1 * (i % 2)):.0f}" height="2" rx="1"/>')
    return rows


def _page(x, y, width, hue='blue', tint=''):
    return (f'<rect class="ui-ink" x="{x}" y="{y}" width="{width * .5:.0f}" height="7" rx="3"/>'
            f'<rect class="ui-line" x="{x}" y="{y + 15}" width="{width * .86:.0f}" height="3" rx="1.5"/>'
            f'<rect class="ui-line" x="{x}" y="{y + 23}" width="{width * .7:.0f}" height="3" rx="1.5"/>'
            f'<rect class="tint f-{hue} {tint}" x="{x}" y="{y + 36}" width="{width:.0f}" height="58" rx="6"/>'
            f'<rect class="ui-line" x="{x}" y="{y + 104}" width="{width * .8:.0f}" height="3" rx="1.5"/>'
            f'<rect class="ui-line" x="{x}" y="{y + 112}" width="{width * .55:.0f}" height="3" rx="1.5"/>')


def _lights(x, y):
    return f'<circle class="tl-r" cx="{x}" cy="{y}" r="2.6"/><circle class="tl-y" cx="{x + 9}" cy="{y}" r="2.6"/><circle class="tl-g" cx="{x + 18}" cy="{y}" r="2.6"/>'


def _window(x, y, width, height, side=86, hue='blue', pane=None, tabs=None, side_class=''):
    """A window frame with a tinted sidebar, traffic lights, an address pill and tab rows."""
    pane_x = x + side + 4
    out = (f'<rect class="ui-win" x="{x}" y="{y}" width="{width}" height="{height}" rx="12"/>'
           f'<rect class="side f-{hue} {side_class}" x="{x + 5}" y="{y + 5}" width="{side - 5}" height="{height - 10}" rx="8"/>'
           + _lights(x + 15, y + 15) +
           f'<rect class="ui-row" x="{x + 11}" y="{y + 26}" width="{side - 17}" height="10" rx="5"/>')
    out += tabs if tabs is not None else _tabs(x + 11, y + 46, side - 17, 4)
    out += f'<rect class="ui-pane" x="{pane_x}" y="{y + 5}" width="{x + width - 5 - pane_x}" height="{height - 10}" rx="8"/>'
    out += pane if pane is not None else _page(pane_x + 14, y + 20, x + width - pane_x - 33, hue)
    return out


def _art(kind, n):
    lift = f'filter="url(#lift-{n})"'
    shadow = f'<defs><filter id="lift-{n}" x="-20%" y="-20%" width="140%" height="150%"><feDropShadow dx="0" dy="8" stdDeviation="9" flood-color="#101c2b" flood-opacity=".28"/></filter></defs>'
    if kind == 'spaces':
        # Spaces swap: the sidebar takes the next Space's colour and its tabs slide in. Two more Spaces stack behind.
        dots = ''.join(f'<circle class="ui-line" cx="{60 + i * 11}" cy="226" r="3"/>' for i in range(3))
        body = ('<rect class="f-violet" x="12" y="14" width="80" height="196" rx="10" opacity=".45"/>'
                '<rect class="f-coral" x="25" y="28" width="80" height="196" rx="10" opacity=".6"/>'
                f'<g {lift}>' + _window(38, 42, 300, 196, hue='blue', side_class='sp-side',
                                        tabs='<g class="sp-tabs">' + _tabs(49, 88, 69, 4) + '</g>',
                                        pane=_page(142, 62, 177, 'blue', 'sp-tint')) + '</g>'
                + dots + '<circle class="f-blue sp-dot" cx="60" cy="226" r="3"/>'
                '<circle class="f-coral" cx="112" cy="57" r="5"/><circle class="ui-pane" cx="112" cy="57" r="2"/>')
    elif kind == 'peek':
        # A link on the page grows into a Peek card over the dimmed page.
        card = (f'<g class="pk-card" {lift}><rect class="ui-pane" x="150" y="62" width="150" height="148" rx="10"/>'
                '<rect class="ui-row" x="160" y="71" width="44" height="8" rx="4"/><circle class="f-blue" cx="287" cy="75" r="4"/>'
                + _page(162, 90, 126, 'pink') + '</g>')
        pane = _page(132, 44, 220, 'blue') + '<rect class="f-blue pk-link" x="132" y="67" width="62" height="3" rx="1.5"/>'
        body = _window(30, 30, 340, 210, hue='violet', pane=pane) + '<rect class="ui-dim pk-dim" x="120" y="35" width="245" height="200" rx="8"/>' + card
    elif kind == 'split':
        # One page narrows to half and a second tab slides in beside it. The sidebar row gains the second favicon.
        pane = ('<rect class="ui-pane sv-a" x="120" y="35" width="122" height="200" rx="8"/>' + _page(132, 50, 98, 'coral')
                + '<g class="sv-b"><rect class="ui-pane" x="246" y="35" width="119" height="200" rx="8"/>' + _page(258, 50, 95, 'blue') + '</g>')
        tabs = ('<rect class="ui-sel" x="41" y="76" width="69" height="11" rx="4"/>'
                '<rect class="f-coral" x="45" y="79" width="5" height="5" rx="1.5"/><rect class="f-blue sv-fav" x="53" y="79" width="5" height="5" rx="1.5"/>'
                '<rect class="ui-line" x="62" y="80.5" width="34" height="2" rx="1"/>' + _tabs(41, 91, 69, 3, active=-1, start=1))
        body = (f'<g {lift}><rect class="ui-win" x="30" y="30" width="340" height="210" rx="12"/>'
                '<rect class="side f-coral" x="35" y="35" width="81" height="200" rx="8"/>' + _lights(45, 45)
                + '<rect class="ui-row" x="41" y="56" width="69" height="10" rx="5"/>' + tabs + pane + '</g>')
    elif kind == 'little':
        # A link in another app (a dark chat window) pops a Little Arc window over it.
        bubbles = ''.join(f'<rect class="{cls}" x="{bx}" y="{by}" width="{bw}" height="16" rx="8"/>'
                          for cls, bx, by, bw in (('ui-app-row', 118, 64, 120), ('ui-app-row', 168, 88, 130), ('f-blue la-link', 118, 112, 96), ('ui-app-row', 118, 136, 80)))
        rows = ''.join(f'<rect class="ui-app-row" x="44" y="{60 + i * 20}" width="52" height="12" rx="4"/>' for i in range(5))
        app = (f'<rect class="ui-app" x="30" y="30" width="290" height="180" rx="12"/>{_lights(44, 44)}{rows}'
               '<rect class="ui-app-row" x="106" y="40" width="1" height="160"/>' + bubbles)
        small = (f'<g class="la-pop" {lift}><rect class="ui-win" x="150" y="78" width="200" height="150" rx="11"/>' + _lights(162, 90)
                 + '<rect class="ui-row" x="190" y="85" width="92" height="10" rx="5"/>'
                 '<g class="la-btn"><rect class="f-blue" x="290" y="84" width="50" height="12" rx="6"/><rect class="glyph-fill" x="300" y="89" width="22" height="2" rx="1"/><path class="glyph" d="M327 87.5l3 2.5-3 2.5"/></g>'
                 '<rect class="ui-pane" x="156" y="102" width="188" height="120" rx="7"/>' + _page(168, 114, 164, 'coral') + '</g>')
        body = app + small
    elif kind == 'command':
        # The command bar opens, a word types out and the results arrive.
        rows = ''
        for i, (hue, width) in enumerate((('violet', 70), ('blue', 92), ('coral', 58))):
            top = 126 + i * 24
            rows += (f'<g class="cb-r{i + 1}"><rect class="{"tint-strong f-violet" if i == 0 else "ui-row-faint"}" x="110" y="{top}" width="180" height="19" rx="6"/>'
                     f'<rect class="f-{hue}" x="118" y="{top + 5}" width="9" height="9" rx="2.5"/>'
                     f'<rect class="ui-line" x="134" y="{top + 8}" width="{width}" height="3" rx="1.5"/>'
                     f'<rect class="ui-row" x="262" y="{top + 5}" width="20" height="9" rx="3"/></g>')
        palette = (f'<g class="cb-pal" {lift}><rect class="ui-pane" x="100" y="80" width="200" height="128" rx="12"/>'
                   '<rect class="f-blue" x="112" y="94" width="11" height="15" rx="3"/><path class="glyph" d="M120 97.5l-4 8"/>'
                   '<rect class="ui-ink cb-type" x="130" y="99" width="62" height="5" rx="2.5"/>'
                   '<rect class="f-blue cb-caret" x="194" y="94" width="2" height="15" rx="1"/>'
                   '<rect class="ui-line" x="100" y="117" width="200" height="1"/>' + rows + '</g>')
        body = _window(30, 30, 340, 210, hue='pink') + '<rect class="ui-dim" x="30" y="30" width="340" height="210" rx="12"/>' + palette
    else:
        # Cmd-D: the selected tab flies up out of the list into a Favorite tile.
        tiles = ''
        for i, hue in enumerate(('blue', 'coral', 'violet', 'pink', 'coral', 'blue')):
            tx, ty = 41 + (i % 3) * 27, 72 + (i // 3) * 22
            tile = f'<rect class="ui-sel" x="{tx}" y="{ty}" width="23" height="18" rx="5"/><circle class="f-{hue}" cx="{tx + 11.5}" cy="{ty + 9}" r="4"/>'
            tiles += f'<rect class="ui-slot" x="{tx}" y="{ty}" width="23" height="18" rx="5"/><g class="pn-tile">{tile}</g>' if i == 5 else tile
        tabs = (tiles + '<rect class="ui-line" x="41" y="124" width="77" height="1"/>'
                '<g class="pn-row">' + _tabs(41, 132, 77, 1) + '</g>' + _tabs(41, 147, 77, 2, active=-1, start=1)
                + '<circle class="f-blue pn-fly" cx="106.5" cy="103" r="4"/>')
        key = '<g class="pn-key"><rect class="ui-pane" x="300" y="48" width="44" height="22" rx="6" stroke="currentColor" stroke-opacity=".15"/><text class="key" x="322" y="62.5" text-anchor="middle">⌘D</text></g>'
        body = f'<g {lift}>' + _window(30, 30, 340, 210, side=94, hue='blue', tabs=tabs) + key + '</g>'
    return f'<svg class="ui" viewBox="0 0 400 260" aria-hidden="true" focusable="false">{shadow}{body}</svg>'


def keys(text):
    """Wrap each run of Mac key glyphs in kbd."""
    return re.sub(r'[⌘⌃⌥⇧]+[A-Z0-9=–]+', lambda match: f'<kbd>{match[0]}</kbd>', escape(text))


def content():
    cards, scenes = '', ''
    for n, (title, text, link, art) in enumerate(CARDS):
        docs = f'<a class="tour-docs" href="{escape(link, quote=True)}">How it works<span class="visually-hidden">: {escape(title)}</span></a>' if link else ''
        active = ' class="is-active"' if n == 0 else ''
        cards += f'<li{active} style="--i: {n}"><h3>{escape(title)}</h3><p>{keys(text)}</p>{docs}</li>'
        scenes += f'<div class="scene{" is-active" if n == 0 else ""}">{_art(art, n)}</div>'
    shortcuts = ''.join(f'<li><kbd>{key}</kbd><span>{escape(label)}</span></li>' for key, label in KEYS)
    links = ''.join(f'<li><a href="{escape(href, quote=True)}">{escape(label)}</a><p>{escape(text)}</p></li>' for label, href, text in LINKS)
    # @@RIBBON@@ is the homepage hero ribbon; embed() inlines the one shared copy.
    return (f'<header class="post-header welcome-hero"><div class="welcome-hero-copy"><h1 class="rise">{TITLE}</h1><p class="welcome-intro rise">{escape(INTRO)}</p></div>@@RIBBON@@</header>'
            '<div class="welcome-body">'
            f'<h2 class="visually-hidden">The basics</h2><div class="tour"><ul class="welcome-cards">{cards}</ul>'
            f'<div class="tour-stage" aria-hidden="true"><span class="glass"></span>{scenes}</div></div>{SCRIPT}'
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
