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
SUMMARY = 'Quick start for myArc: Spaces, pinned tabs, the command bar, Peek, Split View, Little Arc and the shortcuts to learn first.'

# (title, text, a second line of detail, docs link or None, art).
# Only features with their own docs page or section link out.
CARDS = [
    ('Spaces and Profiles', 'A Space holds the tabs for one part of your life, like Work or Travel. A Profile keeps your logins apart, and several Spaces can share one.', '⌃1 to ⌃8 jump to a Space, and ⌥⌘→ moves to the next one.', '/docs/spaces-and-profiles/', 'spaces'),
    ('Pinned tabs and Favorites', 'Press ⌘D to pin a tab to its Space. Favorites show in every Space of a Profile, and closing either one keeps its tile.', 'Press ⌘D again to unpin it. To remove a Favorite, use its right-click menu.', '/docs/spaces-and-profiles/#pinned-tabs-and-favorites', 'pinned'),
    ('Command bar', '⌘T opens it for a new tab and ⌘L on the current address. Type an action like pin or split to run it, or press Tab to list only actions.', 'Tab again leaves the actions list. The arrow keys move through suggestions, Enter opens one and Esc closes the bar.', None, 'command'),
    ('Peek', 'A link from a pinned tab or Favorite to another site opens in a card over the page. Press ⌘O to keep it as a tab.', 'Esc or a click outside closes it, and ⇧⌘T brings it back.', '/docs/peek/', 'peek'),
    ('Split View', 'Press ⌃⇧= to put two tabs side by side. The pair shows as one row in the sidebar and comes back after a restart.', '⌃⇧- separates the pair again and keeps both tabs.', None, 'split'),
    ('Little Arc', 'Links from other apps open in a small window, and ⌥⌘N opens one from anywhere. Press ⌘O to move the page into your current Space.', '⌥⌘O lets you pick another Space instead.', None, 'little'),
]

KEYS = [
    ('⌘T', 'New tab'),
    ('⌘L', 'Edit the address'),
    ('⌘D', 'Pin or unpin the tab'),
    ('⌃1–9', 'Switch Spaces'),
    ('⌥⌘N', 'Open Little Arc'),
    ('⌃⇧=', 'Open a Split View'),
]

LINKS = [
    ('Import from Arc', '/docs/import-from-arc/', 'Bring over your Spaces, folders and pinned tabs.'),
    ('Docs', '/docs/', 'How everything works, and what is still missing.'),
    ('Release notes', '/download/', 'What works in this build, and the latest download.'),
    ('GitHub', REPO, 'The code, the plans and the roadmap.'),
    ('Report an issue', REPO + '/issues', 'Found a bug? Tell me what happened.'),
]

CSS = '''
    .journal.welcome { max-width: 68rem }
    .welcome { --rb-blue: #1a85ff; --rb-violet: #7c6cff; --rb-pink: #f25cbf; --rb-coral: #ff7a5c }
    .welcome h2 { margin-top: 4.5rem; font-size: 1.5rem }

    /* Hero. Title on the left, the homepage ribbon on the right over a soft glow in its own colours. */
    .welcome-top { flex-wrap: nowrap; justify-content: space-between }
    .welcome-nav { display: flex; align-items: center; gap: 1.75rem }
    .welcome-nav a { display: inline-flex; align-items: center; min-height: 2.75rem; color: var(--text); font-weight: 500; text-decoration: none }
    .welcome-nav a:hover, .welcome-nav a:focus-visible { color: var(--link-solid) }
    .welcome-nav .github-icon { width: 1.375rem; height: 1.375rem }

    .welcome .welcome-hero { position: relative; display: grid; grid-template-columns: minmax(0, 1fr) minmax(0, 22rem); align-items: center; gap: 2rem 3rem; margin-bottom: 1rem }
    .welcome-hero::before { content: ''; position: absolute; z-index: -1; top: -6rem; right: -8rem; width: 44rem; height: 34rem; pointer-events: none; filter: blur(3rem); opacity: .5;
      background: radial-gradient(40% 45% at 35% 40%, rgb(26 133 255 / .45), transparent 70%), radial-gradient(35% 40% at 65% 35%, rgb(124 108 255 / .4), transparent 70%), radial-gradient(40% 40% at 55% 70%, rgb(255 122 92 / .35), transparent 70%) }
    .welcome h1 { font-size: 4.25rem; max-width: 12ch }
    .welcome-intro { margin-top: 1.25rem; max-width: 40ch; color: var(--quiet); font-size: 1.25rem; line-height: 1.5 }
    .welcome .hero-art { --shift: 0%; max-width: 22rem; justify-self: end }

    /* The basics as a bento grid, ranked by daily use: four big tiles in two rows, with Split View and Little Arc
       stacked in a narrow column on the right. Each tile is one rounded gradient
       card: its looping scene fills it and only the title sits on top. Hover, focus or a tap grows a tile, the
       grid reflows, and then a shade rises over the lower art with the rest of the text. Leaving runs it backwards:
       text and shade go first, then the tracks move. One tile always has focus. */
    .welcome-cards { display: grid; grid-template-columns: 1fr 1fr 1fr 1fr 1.1fr; grid-template-rows: 22rem 22rem; gap: 1rem; margin: 1.5rem 0 0; padding: 0; list-style: none }
    .welcome-cards.is-live { transition: grid-template-columns .55s var(--ease) .22s, grid-template-rows .55s var(--ease) .22s }
    .welcome-cards.is-live[data-focus="0"] { grid-template-columns: 1.3fr 1.3fr .8fr .8fr .8fr; grid-template-rows: 25rem 19.5rem }
    .welcome-cards.is-live[data-focus="1"] { grid-template-columns: .8fr .8fr 1.3fr 1.3fr .8fr; grid-template-rows: 25rem 19.5rem }
    .welcome-cards.is-live[data-focus="2"] { grid-template-columns: 1.3fr 1.3fr .8fr .8fr .8fr; grid-template-rows: 19.5rem 25rem }
    .welcome-cards.is-live[data-focus="3"] { grid-template-columns: .8fr .8fr 1.3fr 1.3fr .8fr; grid-template-rows: 19.5rem 25rem }
    .welcome-cards.is-live[data-focus="4"] { grid-template-columns: .7fr .7fr .7fr .7fr 1.9fr; grid-template-rows: 25rem 19.5rem }
    .welcome-cards.is-live[data-focus="5"] { grid-template-columns: .7fr .7fr .7fr .7fr 1.9fr; grid-template-rows: 19.5rem 25rem }
    .welcome-cards > li { min-width: 0; min-height: 0; animation: rise .9s var(--ease) both; animation-delay: calc(.35s + var(--i) * .07s) }
    .welcome-cards > .wide { grid-column: span 2 }
    .welcome-cards > .side { grid-area: 1 / 5 }
    .welcome-cards > .side + .side { grid-area: 2 / 5 }
    .tile { position: relative; isolation: isolate; contain: layout paint; display: block; height: 100%; overflow: hidden; border-radius: 1.5rem; outline: 0;
      --base: color-mix(in srgb, var(--page) 95%, rgb(var(--link-rgb))); --scrim: 65%;
      background: var(--base); box-shadow: inset 0 0 0 .0625rem rgb(255 255 255 / .35), 0 1.25rem 2.5rem -1.75rem rgb(16 28 43 / .4) }
    .tile:focus-visible { outline: .1875rem solid var(--link-solid); outline-offset: .25rem }
    /* The mesh is painted once and never moves. The spotlight is a fixed-size glow that only translates. */
    .tile::before { content: ''; position: absolute; z-index: -2; inset: 0; pointer-events: none;
      background: radial-gradient(55% 60% at var(--m1, 14% 18%), rgb(26 133 255 / .55), transparent 70%), radial-gradient(50% 55% at var(--m2, 88% 12%), rgb(124 108 255 / .45), transparent 70%), radial-gradient(55% 60% at var(--m3, 80% 70%), rgb(255 122 92 / .45), transparent 70%), radial-gradient(50% 55% at var(--m4, 18% 66%), rgb(242 92 191 / .4), transparent 70%) }
    .welcome-cards > li:nth-child(2) .tile { --m1: 84% 16%; --m2: 12% 60%; --m3: 66% 72%; --m4: 36% 8% }
    .welcome-cards > li:nth-child(3n) .tile { --m1: 50% 70%; --m2: 8% 12%; --m3: 96% 30%; --m4: 60% 6% }
    .welcome-cards > li:nth-child(3n + 4) .tile { --m1: 90% 60%; --m2: 30% 10%; --m3: 10% 70% }
    .tile::after { content: ''; position: absolute; z-index: -1; top: -11rem; left: -11rem; width: 22rem; height: 22rem; border-radius: 50%; pointer-events: none; opacity: 0;
      background: radial-gradient(closest-side, rgb(255 255 255 / .45), transparent); transform: translate3d(var(--x, 50%), var(--y, 40%), 0); transition: opacity .3s ease-in-out }
    .tile.is-focus::after { opacity: 1; will-change: transform }
    .tile-art { position: absolute; inset: 0; transition: opacity .3s ease-in-out }
    .welcome-cards.is-js .tile:not(.is-focus) .tile-art { opacity: .8 }
    /* The art fills whatever box the track leaves it and keeps its proportions inside. */
    .tile-art .ui { position: absolute; top: .875rem; left: 1rem; width: calc(100% - 2rem); height: calc(100% - .875rem) }
    /* With script the art keeps one layout size and a transform fits it to the box, so a reflow scales a cached
       layer instead of redrawing the SVG every frame. It is a layer only while the grid moves. */
    .welcome-cards.is-sized .ui { left: 50%; width: 640px; height: 416px; transform-origin: 50% 0; transform: translateX(-50%) scale(.4) }
    .welcome-cards.is-moving .ui { will-change: transform }
    /* The copy sits on the art's lower edge. Without script every tile shows all of it over the shade. */
    .tile-copy { position: absolute; z-index: 1; right: 0; bottom: 0; left: 0; padding: 0 1.25rem 1.125rem }
    /* The shade blurs the art behind the copy and tints it with 72% of the tile's base. A mask fades both in over the 3rem above the copy. */
    .tile-copy::before { content: ''; position: absolute; z-index: -1; right: 0; bottom: 0; left: 0; height: calc(100% + 3rem); pointer-events: none;
      background: color-mix(in srgb, var(--base) 72%, transparent);
      -webkit-backdrop-filter: blur(1.25rem) saturate(1.3); backdrop-filter: blur(1.25rem) saturate(1.3);
      -webkit-mask-image: linear-gradient(to bottom, transparent, #000 3rem); mask-image: linear-gradient(to bottom, transparent, #000 3rem) }
    .welcome-cards h3 { position: relative; font-size: 1.25rem; line-height: 1.25 }
    /* A resting tile's scrim, behind the title only. 65% of the base keeps the title at 4.8:1 even over pure black or white. */
    .welcome-cards h3::before { content: ''; position: absolute; z-index: -1; inset: -2.25rem -1.25rem -1.125rem; pointer-events: none; opacity: 0;
      background: linear-gradient(to bottom, transparent, color-mix(in srgb, var(--base) var(--scrim), transparent) 2.25rem) }
    .tile-more { padding-top: .25rem }
    .welcome-cards p { color: var(--quiet); font-size: 1rem; line-height: 1.45 }
    .welcome-cards p + p { margin-top: .25rem }
    .tile-docs { display: inline-block; margin-top: .25rem; color: var(--link-solid); font-size: 1rem }
    /* With script a resting tile shows only its title on the bottom edge. The detail below it opens from zero height, so the title rises with it and never leaves the tile.
       The detail stays in the accessibility tree and its link stays focusable; focusing it opens the tile. The
       way out has no delay and the way in waits for the reflow, so a change of focus reads out, move, in. A
       transition caught midway reverses from where it is. */
    .is-js .tile-detail { display: grid; grid-template-rows: 0fr; transition: grid-template-rows .35s var(--ease) .15s }
    .is-js .tile-more { min-height: 0; overflow: clip; overflow-clip-margin: .25rem }
    .is-js h3::before { opacity: 1; transition: opacity .25s ease-in-out .05s }
    .is-js .tile-copy::before { opacity: 0; transform: translateY(2rem); transition: opacity .25s ease-in-out .05s, transform .35s var(--ease) .05s }
    .is-js .tile-detail { pointer-events: none }
    .is-js .tile-more > * { opacity: 0; transform: translateY(.75rem); transition: opacity .18s ease-in-out, transform .18s ease-in }
    .is-js .is-focus .tile-detail { grid-template-rows: 1fr; transition-delay: .62s }
    .is-js .is-focus h3::before { opacity: 0; transition-delay: .62s }
    .is-js .is-focus .tile-copy::before { opacity: 1; transform: none; transition: opacity .4s ease-in-out .6s, transform .5s var(--ease) .6s }
    .is-js .is-focus .tile-detail { pointer-events: auto }
    .is-js .is-focus .tile-more > * { opacity: 1; transform: none; transition: opacity .35s ease-in-out .7s, transform .45s var(--ease) .7s }
    .is-js .is-focus .tile-more > :nth-child(2) { transition-delay: .77s }
    .is-js .is-focus .tile-more > :nth-child(3) { transition-delay: .84s }
    .is-instant, .is-instant * { transition: none !important }
    .ui { display: block; overflow: visible }

    /* Mini-UI paint. Light first, dark below. */
    .ui .ui-win { fill: #fbfaf7; stroke: rgb(24 36 50 / .08) }
    .ui .ui-shadow { fill: rgb(16 28 43 / .07) }
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
      .tile { --base: color-mix(in srgb, var(--page) 96%, rgb(var(--link-rgb))); box-shadow: inset 0 0 0 .0625rem rgb(255 255 255 / .1), 0 1.25rem 2.5rem -1.75rem rgb(0 0 0 / .7) }
      .tile::after { background: radial-gradient(closest-side, rgb(143 177 255 / .22), transparent) }
      .ui .ui-shadow { fill: rgb(0 0 0 / .14) }
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
       first paint all show the feature done. The script marks a tile .is-playing only while it is on screen, the tab is
       visible and motion is allowed. Once a tile has focus it is the only one playing, in a slower take. */
    .ui .pk-card, .ui .sv-a, .ui .sv-b, .ui .la-pop, .ui .la-btn, .ui .cb-pal, .ui .cb-type, .ui .cb-caret, .ui .pn-fly { transform-box: view-box }
    .ui .pk-card { transform-origin: 162px 68px }
    .ui .sv-a { transform-origin: 120px 0 }
    .ui .sv-b { transform-origin: 365px 0 }
    .ui .la-pop { transform-origin: 250px 228px }
    .ui .la-btn { transform-origin: 315px 90px }
    .ui .cb-pal { transform-origin: 200px 144px }
    .ui .cb-type { transform-origin: 130px 0 }
    .ui .pn-row, .ui .pn-fly, .ui .sp-c, .ui .sp-v, .ui .kc { opacity: 0 }
    .is-playing .ui * { animation-duration: 5s; animation-iteration-count: infinite; animation-timing-function: var(--ease) }
    .is-playing.is-focus .ui * { animation-duration: 7s }
    .is-playing :is(.sp-b, .sp-c, .sp-v, .sp-tabs, .sp-dot, .pk-card, .pk-dim, .pk-link, .sv-a, .sv-b, .sv-fav, .la-pop, .la-btn, .la-link, .cb-pal, .cb-type, .cb-caret, .cb-r1, .cb-r2, .cb-r3, .pn-row, .pn-fly, .pn-tile) { will-change: transform, opacity }
    .is-playing.is-focus .kc { will-change: opacity }
    .is-playing .sp-b { animation-name: sp-b }
    .is-playing .sp-c { animation-name: sp-c }
    .is-playing .sp-v { animation-name: sp-v }
    .is-playing .sp-tabs { animation-name: sp-tabs }
    .is-playing .sp-dot { animation-name: sp-dot }
    .is-playing .pk-card { animation-name: pk-card }
    .is-playing .pk-dim { animation-name: pk-dim }
    .is-playing .pk-link { animation-name: pk-link }
    .is-playing .sv-a { animation-name: sv-a }
    .is-playing .sv-b { animation-name: sv-b }
    .is-playing .sv-fav { animation-name: sv-fav }
    .is-playing .la-pop { animation-name: la-pop }
    .is-playing .la-btn { animation-name: la-btn }
    .is-playing .la-link { animation-name: pk-link }
    .is-playing .cb-pal { animation-name: cb-pal }
    .is-playing .cb-type { animation-name: cb-type }
    .is-playing .cb-caret { animation-name: cb-caret }
    .is-playing .cb-r1 { animation-name: cb-r1 }
    .is-playing .cb-r2 { animation-name: cb-r2 }
    .is-playing .cb-r3 { animation-name: cb-r3 }
    .is-playing .pn-row { animation-name: pn-row }
    .is-playing .pn-fly { animation-name: pn-fly }
    .is-playing .pn-tile { animation-name: pn-tile }
    .is-playing.is-focus .kc-sp1 { animation-name: kc-sp1 }
    .is-playing.is-focus .kc-sp2 { animation-name: kc-sp2 }
    .is-playing.is-focus .kc-pk { animation-name: kc-pk }
    .is-playing.is-focus .kc-sv { animation-name: kc-sv }
    .is-playing.is-focus .kc-la { animation-name: kc-la }
    .is-playing.is-focus .kc-cb { animation-name: kc-cb }
    .is-playing.is-focus .kc-pn { animation-name: kc-pn }
    @keyframes sp-b { 0%, 61% { opacity: 0 } 67%, 100% { opacity: 1 } }
    @keyframes sp-c { 0%, 28% { opacity: 1 } 34%, 100% { opacity: 0 } }
    @keyframes sp-v { 0%, 28% { opacity: 0 } 34%, 61% { opacity: 1 } 67%, 100% { opacity: 0 } }
    @keyframes sp-tabs { 0%, 28%, 36%, 61%, 69%, 100% { transform: none; opacity: 1 } 31%, 64% { transform: translateX(-10px); opacity: 0 } 32%, 65% { transform: translateX(10px); opacity: 0 } }
    @keyframes sp-dot { 0%, 28% { transform: translateX(11px) } 34%, 61% { transform: translateX(22px) } 67%, 100% { transform: none } }
    @keyframes pk-card { 0%, 4% { transform: scale(.05); opacity: 0 } 18%, 88% { transform: none; opacity: 1 } 98%, 100% { transform: scale(.05); opacity: 0 } }
    @keyframes pk-dim { 0%, 4% { opacity: 0 } 18%, 88% { opacity: 1 } 98%, 100% { opacity: 0 } }
    @keyframes pk-link { 0%, 4% { opacity: .35 } 10%, 100% { opacity: 1 } }
    @keyframes sv-a { 0%, 18% { transform: scaleX(2.008) } 34%, 88% { transform: none } 100% { transform: scaleX(2.008) } }
    @keyframes sv-b { 0%, 18% { transform: scaleX(0); opacity: 0 } 21% { opacity: 1 } 34%, 88% { transform: none; opacity: 1 } 97% { opacity: 1 } 100% { transform: scaleX(0); opacity: 0 } }
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
    @keyframes kc-sp1 { 0%, 20% { opacity: 0 } 24%, 34% { opacity: 1 } 40%, 100% { opacity: 0 } }
    @keyframes kc-sp2 { 0%, 53% { opacity: 0 } 57%, 67% { opacity: 1 } 73%, 100% { opacity: 0 } }
    @keyframes kc-pk { 0%, 72% { opacity: 0 } 76%, 88% { opacity: 1 } 94%, 100% { opacity: 0 } }
    @keyframes kc-sv { 0%, 8% { opacity: 0 } 12%, 30% { opacity: 1 } 38%, 100% { opacity: 0 } }
    @keyframes kc-la { 0%, 48% { opacity: 0 } 52%, 64% { opacity: 1 } 70%, 100% { opacity: 0 } }
    @keyframes kc-cb { 0%, 6% { opacity: 1 } 12%, 90% { opacity: 0 } 96%, 100% { opacity: 1 } }
    @keyframes kc-pn { 0%, 8% { opacity: 0 } 14%, 34% { opacity: 1 } 42%, 100% { opacity: 0 } }

    .welcome-keys, .welcome-links { margin: 1.25rem 0 0; padding: 0; list-style: none }
    .welcome-keys { display: grid; grid-template-columns: repeat(auto-fill, minmax(14rem, 1fr)); gap: .75rem 1.5rem }
    .welcome-keys li { display: flex; align-items: baseline; gap: .75rem; font-size: 1rem }
    .welcome kbd { min-width: 4.5rem; padding: .05em .4em; border: 1px solid color-mix(in srgb, currentColor 18%, transparent); border-bottom-width: 2px; border-radius: .375rem; background: color-mix(in srgb, currentColor 6%, transparent); font-family: inherit; font-size: 1rem; font-weight: 500; text-align: center; white-space: nowrap }
    .welcome-cards kbd { min-width: 0; color: var(--text) }
    .welcome-more { margin-top: 1rem; font-size: 1rem }
    .welcome-links { display: grid; grid-template-columns: repeat(auto-fill, minmax(17rem, 1fr)); gap: 1rem 1.5rem }
    .welcome-links a { font-weight: 600 }
    .welcome-links p { margin-top: .25rem; color: var(--quiet); font-size: 1rem }

    /* Below 75rem five columns get too narrow for the shrunk tiles, so two columns. A focused tile just gets
       taller. Without the growth (no script or reduced motion) every tile is tall enough for all its text. */
    @media (max-width: 75rem) {
      .welcome-cards, .welcome-cards.is-live[data-focus] { grid-template-columns: 1fr 1fr; grid-template-rows: none }
      .welcome-cards > .wide, .welcome-cards > .side, .welcome-cards > .side + .side { grid-area: auto }
      .tile { height: 21rem }
      .welcome-cards.is-live .tile { height: 14rem; transition: height .45s var(--ease) .22s }
      .welcome-cards.is-live .tile.is-focus { height: 23rem }
    }
    /* Phones: one column. */
    @media (max-width: 40rem) {
      .welcome-cards, .welcome-cards.is-live[data-focus] { grid-template-columns: minmax(0, 1fr) }
      .welcome-cards.is-live .tile { height: 13rem }
      .welcome-cards.is-live .tile.is-focus { height: 27rem }
    }
    @media (max-width: 48rem) {
      .welcome-top { justify-content: space-between }
      .welcome-nav { gap: 1.25rem }
      .welcome .welcome-hero { grid-template-columns: minmax(0, 1fr) }
      .welcome .hero-art { justify-self: center; max-width: 15rem }
      .welcome h1 { font-size: 3rem }
    }
    @media (prefers-reduced-motion: reduce) {
      .welcome-cards > li { animation: none }
      .tile-art, .tile::after, .is-js .tile-detail, .is-js h3::before { transition: none !important }
      /* No movement: the title jumps and the shade and detail only fade. */
      .is-js .tile-copy::before, .is-js .tile-more > * { transform: none !important; transition: opacity .2s ease-in-out !important }
      .tile.is-focus { outline: .1875rem solid var(--link-solid); outline-offset: .25rem }
    }
'''

# The bento script. One tile always has focus: the first until hover, keyboard focus or a tap moves it, and then
# the last one. Tiles loop only while on screen in a visible tab, and only the focused one plays.
SCRIPT = """<script>
  (() => {
    const grid = document.querySelector('.welcome-cards')
    const tiles = [...grid.querySelectorAll('.tile')]
    const still = matchMedia('(prefers-reduced-motion: reduce)').matches
    const seen = new Set()
    let focus = 0, lock = 0, tapped = -1
    const sync = () => {
      tiles.forEach((tile, i) => {
        tile.classList.toggle('is-focus', i === focus)
        tile.classList.toggle('is-playing', !still && !document.hidden && seen.has(tile) && i === focus)
      })
      grid.dataset.focus = String(focus)
    }
    // A lock after each change, as long as the out, move and in take, keeps a resting cursor from flipping focus
    // while the grid reflows under it.
    const set = (index) => { if (index !== focus) { focus = index; lock = performance.now() + 800; sync() } }
    const aim = (tile, x, y) => { tile.style.setProperty('--x', x + 'px'); tile.style.setProperty('--y', y + 'px') }
    tiles.forEach((tile, i) => {
      tile.addEventListener('pointermove', (event) => {
        if (event.pointerType !== 'mouse') return
        const box = tile.getBoundingClientRect()
        aim(tile, event.clientX - box.left, event.clientY - box.top)
        if (i !== focus && performance.now() > lock) set(i)
      })
      tile.addEventListener('pointerdown', (event) => { tapped = event.pointerType === 'mouse' ? -2 : focus })
      tile.addEventListener('pointerup', (event) => {
        if (event.pointerType !== 'mouse' && !event.target.closest('a')) set(i)
        tapped = -1
      })
      // focusin, not focus, so tabbing to the docs link inside a resting tile opens it too.
      tile.addEventListener('focusin', () => {
        if (tapped !== -1 || i === focus) return
        aim(tile, tile.offsetWidth / 2, tile.offsetHeight / 3)
        set(i)
      })
    })
    document.addEventListener('visibilitychange', sync)
    const io = new IntersectionObserver((entries) => {
      entries.forEach((entry) => entry.isIntersecting ? seen.add(entry.target) : seen.delete(entry.target))
      sync()
    })
    tiles.forEach((tile) => io.observe(tile))
    // Fit each 640 x 416 drawing to its box, the same contain-fit the SVG does on its own without script. Spare
    // height centres it in the space above a resting title.
    const fit = new ResizeObserver((entries) => entries.forEach(({ target, contentRect: { width, height } }) => {
      const scale = Math.max(.1, Math.min((width - 32) / 640, (height - 14) / 416))
      const y = Math.max(0, (height - 70 - 416 * scale) / 2)
      target.firstElementChild.style.transform = 'translate(-50%, ' + y.toFixed(1) + 'px) scale(' + scale.toFixed(4) + ')'
    }))
    tiles.forEach((tile) => fit.observe(tile.querySelector('.tile-art')))
    grid.classList.add('is-sized')
    let moving = 0
    const layout = (event) => !event.pseudoElement && (event.target === grid || event.target.classList.contains('tile'))
    grid.addEventListener('transitionrun', (event) => { if (layout(event)) { clearTimeout(moving); grid.classList.add('is-moving') } })
    grid.addEventListener('transitionend', (event) => { if (layout(event)) moving = setTimeout(() => grid.classList.remove('is-moving'), 100) })
    // The first focus lands without animating, then transitions turn on.
    grid.classList.add('is-js', 'is-instant')
    if (!still) grid.classList.add('is-live')
    sync()
    requestAnimationFrame(() => requestAnimationFrame(() => grid.classList.remove('is-instant')))
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


def _chip(x, y, text, name):
    """A key chip for the keystroke behind the scene's action."""
    width = 24 + 10 * len(text)
    return (f'<g class="kc kc-{name}"><rect class="ui-pane" x="{x}" y="{y}" width="{width}" height="22" rx="6" stroke="currentColor" stroke-opacity=".15"/>'
            f'<text class="key" x="{x + width / 2:g}" y="{y + 14.5:g}" text-anchor="middle">{text}</text></g>')


def _art(kind, n):
    # Flat stepped shadows, not SVG filters: a filter would re-rasterize every frame its content moves.
    def lift(x, y, width, height, r):
        return (f'<rect class="ui-shadow" x="{x - 3}" y="{y + 5}" width="{width + 6}" height="{height + 6}" rx="{r + 3}"/>'
                f'<rect class="ui-shadow" x="{x - 1}" y="{y + 2}" width="{width + 2}" height="{height + 2}" rx="{r + 1}"/>')
    if kind == 'spaces':
        # Spaces swap: the sidebar takes the next Space's colour and its tabs slide in. Two more Spaces stack behind.
        dots = ''.join(f'<circle class="ui-line" cx="{60 + i * 11}" cy="226" r="3"/>' for i in range(3))
        body = ('<rect class="f-violet" x="12" y="14" width="80" height="196" rx="10" opacity=".45"/>'
                '<rect class="f-coral" x="25" y="28" width="80" height="196" rx="10" opacity=".6"/>'
                + lift(38, 42, 300, 196, 12) + _window(38, 42, 300, 196, hue='blue', side_class='sp-b',
                                                       tabs='<rect class="side f-coral sp-c" x="43" y="47" width="81" height="186" rx="8"/>'
                                                       '<rect class="side f-violet sp-v" x="43" y="47" width="81" height="186" rx="8"/>'
                                                       '<g class="sp-tabs">' + _tabs(49, 88, 69, 4) + '</g>',
                                                       pane=_page(142, 62, 177, 'blue'))
                + dots + '<circle class="f-blue sp-dot" cx="60" cy="226" r="3"/>'
                '<circle class="f-coral" cx="112" cy="57" r="5"/><circle class="ui-pane" cx="112" cy="57" r="2"/>'
                + _chip(286, 50, '⌃3', 'sp1') + _chip(286, 50, '⌃1', 'sp2'))
    elif kind == 'peek':
        # A link on the page grows into a Peek card over the dimmed page.
        card = ('<g class="pk-card">' + lift(150, 62, 150, 148, 10) + '<rect class="ui-pane" x="150" y="62" width="150" height="148" rx="10"/>'
                '<rect class="ui-row" x="160" y="71" width="44" height="8" rx="4"/><circle class="f-blue" cx="287" cy="75" r="4"/>'
                + _page(162, 90, 126, 'pink') + '</g>')
        pane = _page(132, 44, 220, 'blue') + '<rect class="f-blue pk-link" x="132" y="67" width="62" height="3" rx="1.5"/>'
        body = _window(30, 30, 340, 210, hue='violet', pane=pane) + '<rect class="ui-dim pk-dim" x="120" y="35" width="245" height="200" rx="8"/>' + card + _chip(312, 42, '⌘O', 'pk')
    elif kind == 'split':
        # One page narrows to half and a second tab unfolds beside it from the window's right edge. The sidebar row gains the second favicon.
        pane = ('<rect class="ui-pane sv-a" x="120" y="35" width="122" height="200" rx="8"/>' + _page(132, 50, 98, 'coral')
                + '<g class="sv-b"><rect class="ui-pane" x="246" y="35" width="119" height="200" rx="8"/>' + _page(258, 50, 95, 'blue') + '</g>')
        tabs = ('<rect class="ui-sel" x="41" y="76" width="69" height="11" rx="4"/>'
                '<rect class="f-coral" x="45" y="79" width="5" height="5" rx="1.5"/><rect class="f-blue sv-fav" x="53" y="79" width="5" height="5" rx="1.5"/>'
                '<rect class="ui-line" x="62" y="80.5" width="34" height="2" rx="1"/>' + _tabs(41, 91, 69, 3, active=-1, start=1))
        body = (lift(30, 30, 340, 210, 12) + '<g><rect class="ui-win" x="30" y="30" width="340" height="210" rx="12"/>'
                '<rect class="side f-coral" x="35" y="35" width="81" height="200" rx="8"/>' + _lights(45, 45)
                + '<rect class="ui-row" x="41" y="56" width="69" height="10" rx="5"/>' + tabs + pane + '</g>' + _chip(302, 42, '⌃⇧=', 'sv'))
    elif kind == 'little':
        # A link in another app (a dark chat window) pops a Little Arc window over it.
        bubbles = ''.join(f'<rect class="{cls}" x="{bx}" y="{by}" width="{bw}" height="16" rx="8"/>'
                          for cls, bx, by, bw in (('ui-app-row', 118, 64, 120), ('ui-app-row', 168, 88, 130), ('f-blue la-link', 118, 112, 96), ('ui-app-row', 118, 136, 80)))
        rows = ''.join(f'<rect class="ui-app-row" x="44" y="{60 + i * 20}" width="52" height="12" rx="4"/>' for i in range(5))
        app = (f'<rect class="ui-app" x="30" y="30" width="290" height="180" rx="12"/>{_lights(44, 44)}{rows}'
               '<rect class="ui-app-row" x="106" y="40" width="1" height="160"/>' + bubbles)
        small = ('<g class="la-pop">' + lift(150, 78, 200, 150, 11) + '<rect class="ui-win" x="150" y="78" width="200" height="150" rx="11"/>' + _lights(162, 90)
                 + '<rect class="ui-row" x="190" y="85" width="92" height="10" rx="5"/>'
                 '<g class="la-btn"><rect class="f-blue" x="290" y="84" width="50" height="12" rx="6"/><rect class="glyph-fill" x="300" y="89" width="22" height="2" rx="1"/><path class="glyph" d="M327 87.5l3 2.5-3 2.5"/></g>'
                 '<rect class="ui-pane" x="156" y="102" width="188" height="120" rx="7"/>' + _page(168, 114, 164, 'coral') + '</g>')
        body = app + small + _chip(270, 40, '⌘O', 'la')
    elif kind == 'command':
        # The command bar opens, a word types out and the results arrive.
        rows = ''
        for i, (hue, width) in enumerate((('violet', 70), ('blue', 92), ('coral', 58))):
            top = 126 + i * 24
            rows += (f'<g class="cb-r{i + 1}"><rect class="{"tint-strong f-violet" if i == 0 else "ui-row-faint"}" x="110" y="{top}" width="180" height="19" rx="6"/>'
                     f'<rect class="f-{hue}" x="118" y="{top + 5}" width="9" height="9" rx="2.5"/>'
                     f'<rect class="ui-line" x="134" y="{top + 8}" width="{width}" height="3" rx="1.5"/>'
                     f'<rect class="ui-row" x="262" y="{top + 5}" width="20" height="9" rx="3"/></g>')
        palette = ('<g class="cb-pal">' + lift(100, 80, 200, 128, 12) + '<rect class="ui-pane" x="100" y="80" width="200" height="128" rx="12"/>'
                   '<rect class="f-blue" x="112" y="94" width="11" height="15" rx="3"/><path class="glyph" d="M120 97.5l-4 8"/>'
                   '<rect class="ui-ink cb-type" x="130" y="99" width="62" height="5" rx="2.5"/>'
                   '<rect class="f-blue cb-caret" x="194" y="94" width="2" height="15" rx="1"/>'
                   '<rect class="ui-line" x="100" y="117" width="200" height="1"/>' + rows + '</g>')
        body = _window(30, 30, 340, 210, hue='pink') + '<rect class="ui-dim" x="30" y="30" width="340" height="210" rx="12"/>' + palette + _chip(312, 42, '⌘T', 'cb')
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
        body = lift(30, 30, 340, 210, 12) + _window(30, 30, 340, 210, side=94, hue='blue', tabs=tabs) + _chip(300, 48, '⌘D', 'pn')
    return f'<svg class="ui" viewBox="0 0 400 260" aria-hidden="true" focusable="false">{body}</svg>'


def keys(text):
    """Wrap each run of Mac key glyphs in kbd."""
    return re.sub(r'[⌘⌃⌥⇧]+[A-Z0-9=–→-]+', lambda match: f'<kbd>{match[0]}</kbd>', escape(text))


def content():
    cards = ''
    for n, (title, text, more, link, art) in enumerate(CARDS):
        docs = f'<a class="tile-docs" href="{escape(link, quote=True)}">How it works<span class="visually-hidden">: {escape(title)}</span></a>' if link else ''
        size = 'wide' if n < 4 else 'side'
        cards += (f'<li class="{size}" style="--i: {n}"><article class="tile" tabindex="0" aria-labelledby="tile-{art}">'
                  f'<div class="tile-art" aria-hidden="true">{_art(art, n)}</div>'
                  f'<div class="tile-copy"><h3 id="tile-{art}">{escape(title)}</h3><div class="tile-detail"><div class="tile-more"><p>{keys(text)}</p><p>{keys(more)}</p>{docs}</div></div></div></article></li>')
    shortcuts = ''.join(f'<li><kbd>{key}</kbd><span>{escape(label)}</span></li>' for key, label in KEYS)
    links = ''.join(f'<li><a href="{escape(href, quote=True)}">{escape(label)}</a><p>{escape(text)}</p></li>' for label, href, text in LINKS)
    # @@RIBBON@@ is the homepage hero ribbon; embed() inlines the one shared copy.
    return (f'<header class="post-header welcome-hero"><div class="welcome-hero-copy"><h1 class="rise">{TITLE}</h1><p class="welcome-intro rise">{escape(INTRO)}</p></div>@@RIBBON@@</header>'
            '<div class="welcome-body">'
            f'<h2 class="visually-hidden">The basics</h2><ul class="welcome-cards">{cards}</ul>{SCRIPT}'
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
    # The visitor already has myArc, so no site menu or download button: the logo home, Docs and GitHub.
    header = re.search(r'<header class="top">.*?</header>', page, re.S)[0]
    mark = re.search(r'<a class="mark plain"[^>]*>.*?</a>', header, re.S)[0]
    github = re.search(r'<a class="nav-github"[^>]*>(<svg.*?</svg>)</a>', header, re.S)[1]
    slim = (f'<header class="top welcome-top">{mark}<div class="welcome-nav" role="navigation" aria-label="Site">'
            f'<a href="/docs/">Docs</a><a class="welcome-github" href="{REPO}" aria-label="myArc on GitHub">{github}</a></div></header>')
    page = page.replace(header, slim, 1)
    folder = Path(root) / 'welcome'
    folder.mkdir(exist_ok=True)
    # Shared hashed assets live with the blog, which the build always creates first.
    (folder / 'index.html').write_text(embed(page, '../blog/'))
