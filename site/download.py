"""Build the alpha downloads page and release notes from site/release.json."""
from datetime import date, datetime, timezone
from email.utils import format_datetime
from html import escape
from xml.sax.saxutils import quoteattr
from pathlib import Path
import json
import re

from blog import page_shell

URL = 'https://myarc.dev/download/'
REPO = 'https://github.com/bitbonsai/myarc'
KEYS = {'published', 'version', 'date', 'dmg_url', 'size_bytes', 'sha256', 'release_url', 'min_macos', 'proprietary_codecs'}
# Present from the first release with Sparkle, whose appcast needs the DMG's
# EdDSA signature from tools/package_release.sh.
OPTIONAL = {'ed_signature'}
DEFAULTS = {'release_url': REPO + '/releases', 'min_macos': '26', 'proprietary_codecs': False}
TITLE = 'myArc Alpha'
STATUS = 'Alpha build, for trying the Arc-like shell. Not yet for daily or sensitive browsing.'
LEDE = "Arc's workflow on Chromium with ungoogled-chromium privacy patches and Chrome extensions intact."
SUMMARY = 'Download myArc Alpha: Arc-style browsing on Chromium with ungoogled-chromium privacy patches. macOS available now, Linux and Windows coming soon.'
APP = '/Applications/myArc.app'

PROMISES = [
    'No account or sign-in',
    'Ad blocking on, AI off by default',
    'Imports your Arc Spaces',
    'GPL-3.0, data stays on your machine',
]

CHECK_ICON = (
    '<svg class="dl-check" viewBox="0 0 16 16" fill="none" stroke="currentColor" stroke-width="2" '
    'stroke-linecap="round" stroke-linejoin="round" aria-hidden="true" focusable="false">'
    '<path d="M3.5 8.5l3 3 6-6.5"/></svg>'
)

HIGHLIGHTS = [
    ('Spaces and Profiles', 'Glass sidebar, shared Favorites, pinned tabs and folders.'),
    ('Command bar', 'Press ⌘L or ⌘T to open sites, run actions or switch Spaces.'),
    ('Peek and Split View', 'Open a link in a card over the page, or put two tabs side by side.'),
    ('Little Arc', 'Links from other apps open in a small window, or press ⌥⌘N anywhere.'),
    ('Import from Arc', 'Bring over Spaces, folders and pinned tabs in Arc\'s order.'),
    ('Ad blocking and agents', 'uBlock Origin Lite built in, plus opt-in CLI and MCP control.'),
]

DOWNLOAD_ICON = (
    '<svg class="dl-icon" viewBox="0 0 20 20" fill="none" stroke="currentColor" stroke-width="2" '
    'stroke-linecap="round" stroke-linejoin="round" aria-hidden="true" focusable="false">'
    '<path d="M10 3.5v9.5m0 0l-3.75-3.75M10 13l3.75-3.75M4 16.5h12"/></svg>'
)

CHEVRON_ICON = (
    '<svg class="dl-chevron" viewBox="0 0 16 16" fill="none" stroke="currentColor" stroke-width="1.75" '
    'stroke-linecap="round" stroke-linejoin="round" aria-hidden="true" focusable="false">'
    '<path d="M4.5 6.25L8 9.75l3.5-3.5"/></svg>'
)


def read_release(root):
    """Return the validated release when it is published, else None."""
    path = Path(root) / 'release.json'
    if not path.is_file():
        return None
    try:
        data = json.loads(path.read_text())
    except json.JSONDecodeError:
        raise ValueError('release.json is not valid JSON') from None
    if not isinstance(data, dict) or not KEYS <= set(data) <= KEYS | OPTIONAL:
        raise ValueError('release.json needs exactly: ' + ', '.join(sorted(KEYS)) + ', and optionally ed_signature')
    if type(data['published']) is not bool:
        raise ValueError('release.json: published must be true or false')
    if not data['published']:
        return None
    validate(data)
    return data


def validate(data):
    for key in ('version', 'date', 'dmg_url', 'sha256', 'release_url', 'min_macos'):
        if not isinstance(data[key], str) or not data[key].strip():
            raise ValueError(f'release.json: {key} is required when published')
    try:
        date.fromisoformat(data['date'])
    except ValueError:
        raise ValueError('release.json: date must be YYYY-MM-DD') from None
    if not data['dmg_url'].startswith(REPO + '/releases/download/') or not data['dmg_url'].endswith('.dmg') or re.search(r'[\s"<>]', data['dmg_url']):
        raise ValueError(f'release.json: dmg_url must be a .dmg asset under {REPO}/releases/download/')
    if not data['release_url'].startswith(REPO + '/releases') or re.search(r'[\s"<>]', data['release_url']):
        raise ValueError(f'release.json: release_url must be under {REPO}/releases')
    if not re.fullmatch(r'[0-9a-f]{64}', data['sha256']):
        raise ValueError('release.json: sha256 must be 64 lowercase hex characters')
    if type(data['size_bytes']) is not int or data['size_bytes'] <= 0:
        raise ValueError('release.json: size_bytes must be a positive integer')
    if not re.fullmatch(r'\d+(\.\d+)*', data['min_macos']):
        raise ValueError('release.json: min_macos must look like 26 or 26.1')
    if type(data['proprietary_codecs']) is not bool:
        raise ValueError('release.json: proprietary_codecs must be true or false')
    if 'ed_signature' in data:
        if not re.fullmatch(r'[A-Za-z0-9+/]{86}==', str(data['ed_signature'])):
            raise ValueError('release.json: ed_signature must be the 88-character output of sign_update -p')
        bundle_version(data['version'])


def bundle_version(version):
    """Return the CFBundleVersion tools/package_release.sh writes for a release label."""
    match = re.fullmatch(r'(\d{4})\.(\d{2})\.(\d{2})(?:\.(\d+))?', version)
    if not match:
        raise ValueError('release.json: version must be YYYY.MM.DD or YYYY.MM.DD.N for an update')
    return f'{match[1]}{match[2]}{match[3]}.{match[4] or 0}'


def appcast(release):
    """Sparkle appcast with the published release, or an empty feed before one is signed."""
    item = ''
    if release and 'ed_signature' in release:
        day = date.fromisoformat(release['date'])
        published = format_datetime(datetime(day.year, day.month, day.day, tzinfo=timezone.utc))
        item = f"""    <item>
      <title>myArc {escape(release['version'])}</title>
      <pubDate>{published}</pubDate>
      <link>{escape(release['release_url'])}</link>
      <sparkle:version>{bundle_version(release['version'])}</sparkle:version>
      <sparkle:shortVersionString>{escape(release['version'])}</sparkle:shortVersionString>
      <sparkle:minimumSystemVersion>{escape(release['min_macos'])}</sparkle:minimumSystemVersion>
      <enclosure url={quoteattr(release['dmg_url'])} length="{release['size_bytes']}" type="application/octet-stream" sparkle:edSignature={quoteattr(release['ed_signature'])}/>
    </item>
"""
    return f"""<?xml version="1.0" encoding="utf-8"?>
<rss version="2.0" xmlns:sparkle="http://www.andymatuschak.org/xml-namespaces/sparkle">
  <channel>
    <title>myArc</title>
    <link>https://myarc.dev/appcast.xml</link>
{item}  </channel>
</rss>
"""


def facts(release):
    day = date.fromisoformat(release['date'])
    return {'Version': release['version'], 'Date': f'{day:%B} {day.day}, {day.year}', 'Size': f'{release["size_bytes"] / 1e6:.1f} MB', 'SHA-256': release['sha256']}


def requirements(release):
    minimum = (release or DEFAULTS)['min_macos']
    return ['An Apple silicon Mac. Intel Macs are not supported.',
            f'macOS {minimum} or later. The glass frame requires macOS 26. It has been tested only on macOS 26.7.']


def codecs(release):
    if (release or DEFAULTS)['proprietary_codecs']:
        return 'H.264 and AAC are included, so most other web video plays.'
    return 'H.264 and AAC are not included, so some web video will not play.'


def limitations(release):
    return ['Built from a Chromium 158 development revision, not a stable release.',
            'myArc checks for a new alpha build once a day and asks before installing it, and Check for Updates… in the myArc menu checks right away. Alpha builds get no separate security updates.' if 'ed_signature' in (release or {}) else 'No automatic updates and no security updates. Install each new build by hand.',
            'Builds are signed with an Apple Developer ID that later builds keep. macOS may ask once for your login password so it can read the saved-data key; choose Always Allow, and newer builds stop asking.',
            'Recent tabs and navigation can be lost after a crash or force quit.',
            'Saved-password and credential guarantees are not verified yet.',
            'RAR downloads are not scanned by Safe Browsing, because the UnRAR license conflicts with GPL-3.0.',
            'No Widevine, so Netflix, Spotify and Disney+ will not play.',
            codecs(release),
            'myArc is an independent project, not affiliated with Arc or The Browser Company.']


def first_launch(release):
    name = release['dmg_url'].rsplit('/', 1)[1]
    return [('Check the download against the SHA-256 listed with it. The two values must match.', f'shasum -a 256 ~/Downloads/{name}'),
            ('Open the DMG and drag myArc to Applications.', None),
            ('Open myArc. Apple has notarized each build, so macOS only asks once whether to open an app downloaded from the internet. Choose Open.', None)]


def release_notes(release):
    name = release['dmg_url'].rsplit('/', 1)[1]
    shown = facts(release)
    out = [f'# myArc Alpha {release["version"]}', '', STATUS, '', '## Download', '',
           f'- File: `{name}` ({shown["Size"]})',
           f'- Released: {shown["Date"]}',
           f'- SHA-256: `{release["sha256"]}`', '']
    out += ['## Requirements', ''] + [f'- {line}' for line in requirements(release)]
    out += ['', '## First launch', '']
    for number, (text, code) in enumerate(first_launch(release), 1):
        out.append(f'{number}. {text}')
        if code:
            out += ['', '   ```sh', f'   {code}', '   ```', '']
    out += ['', '## Known limitations', ''] + [f'- {line}' for line in limitations(release)]
    out += ['', f'Full notes and what works today: {URL}', f'Report problems: {REPO}/issues', '']
    return '\n'.join(out)


def _keys(text):
    return re.sub(r'[⌘⌃⌥⇧]+[A-Z0-9=–→-]+', lambda match: f'<kbd>{match[0]}</kbd>', escape(text))


def _caveats(release):
    updates = (
        'Checks for new alpha builds daily, with no separate security patches.'
        if release and release.get('ed_signature')
        else 'Install each new build by hand, with no automatic or security updates.'
    )
    return [
        ('Chromium 158 dev', updates),
        ('Media playback', f'No Widevine, so Netflix, Spotify and Disney+ will not play. {codecs(release)}'),
        ('Crashes', 'Pins, folders and Spaces survive a force quit. Recent tabs can still be lost.'),
        ('Early guarantees', 'Saved-password and credential checks are not verified yet.'),
    ]


def content(release):
    minimum = (release or DEFAULTS)['min_macos']
    promises = ''.join(f'<li>{CHECK_ICON}<span>{escape(item)}</span></li>' for item in PROMISES)
    hero_copy = (
        '<div class="dl-hero-copy">'
        f'<h1 class="rise">{TITLE}</h1>'
        f'<p class="dl-lede rise">{escape(LEDE)}</p>'
        f'<ul class="dl-promises rise">{promises}</ul>'
        '</div>'
    )
    upcoming = (
        '<div class="dl-upcoming" aria-label="Upcoming platforms">'
        '<button type="button" class="dl-os-btn" disabled><span>Linux</span><span class="dl-soon">Soon</span></button>'
        '<button type="button" class="dl-os-btn" disabled><span>Windows</span><span class="dl-soon">Soon</span></button>'
        '</div>'
    )
    if release:
        shown = facts(release)
        name = release['dmg_url'].rsplit('/', 1)[1]
        cmd = f'shasum -a 256 ~/Downloads/{name}'
        card = (
            '<div class="download-box rise">'
            '<div class="dl-card-head">'
            f'<span class="dl-version">v{escape(shown["Version"])}</span>'
            f'<span class="dl-date">{escape(shown["Date"])}</span>'
            '</div>'
            f'<a class="button dl-cta" href="{escape(release["dmg_url"], quote=True)}">{DOWNLOAD_ICON}Download for macOS</a>'
            f'<p class="dl-submeta">{escape(shown["Size"])} · Apple silicon · macOS {escape(minimum)}+ · Notarized</p>'
            f'{upcoming}'
            '<details class="dl-verify">'
            f'<summary><span>Verify download (SHA-256)</span>{CHEVRON_ICON}</summary>'
            '<div class="dl-verify-body">'
            f'<p class="dl-hash-label">SHA-256</p><code class="dl-hash">{escape(shown["SHA-256"])}</code>'
            f'<div class="dl-cmd"><pre><code>{escape(cmd)}</code></pre>'
            f'<button type="button" class="dl-copy" data-copy="{escape(cmd, quote=True)}">Copy</button></div>'
            '</div></details>'
            '</div>'
        )
    else:
        card = (
            '<div class="download-box dl-coming rise">'
            '<h2>First alpha build coming soon</h2>'
            f'<p>There is no download yet. The <a href="{escape(DEFAULTS["release_url"], quote=True)}">releases page</a> will list each build when it exists.</p>'
            f'{upcoming}'
            '</div>'
        )

    sections = f'<section class="dl-hero">{hero_copy}{card}</section>'

    if release:
        steps = [
            ('1', 'Download the DMG', f'Apple silicon Mac on macOS {minimum} or later. Intel Macs will not run it.'),
            ('2', 'Drag to Applications', 'Open the disk image and drop myArc into Applications.'),
            ('3', 'Open and allow once', 'Apple has notarized each build, so macOS asks once. Click Open, then Always Allow if it asks for your login keychain.'),
        ]
        step_items = ''.join(
            f'<li><h3><span class="dl-step-num" aria-hidden="true">{num}.</span> {escape(title)}</h3><p>{escape(desc)}</p></li>'
            for num, title, desc in steps
        )
        sections += (
            '<section class="dl-section">'
            '<div class="dl-section-head"><h2>First launch on macOS</h2>'
            '<a class="dl-more" href="/docs/install/">Install and update guide &rarr;</a></div>'
            f'<ol class="dl-steps">{step_items}</ol>'
            '</section>'
        )

    works_items = ''.join(
        f'<li><strong>{escape(title)}.</strong> {_keys(desc)}</li>'
        for title, desc in HIGHLIGHTS
    )
    caveat_items = ''.join(
        f'<li><strong>{escape(title)}.</strong> {escape(desc)}</li>'
        for title, desc in _caveats(release)
    )
    sections += (
        '<section class="dl-section dl-split">'
        '<div class="dl-panel">'
        '<div class="dl-panel-head"><h2>Ready to try</h2><a class="dl-more" href="/docs/">Browse docs &rarr;</a></div>'
        f'<ul class="dl-list">{works_items}</ul>'
        '</div>'
        '<div class="dl-panel dl-panel-quiet">'
        '<div class="dl-panel-head"><h2>Keep in mind</h2><a class="dl-more" href="/docs/known-limitations/">All limitations &rarr;</a></div>'
        f'<ul class="dl-list">{caveat_items}</ul>'
        '</div>'
        '</section>'
    )

    shown_releases = (release or DEFAULTS)['release_url']
    sections += (
        '<section class="dl-section dl-footer-bar">'
        '<ul class="dl-links">'
        '<li><a href="https://myarc.slirc.app">Feedback and chat</a><p>Join myarc.slirc.app to share what works or feels rough.</p></li>'
        f'<li><a href="{REPO}/issues">Report a problem</a><p>Include your myArc and OS versions, and leave out private pages.</p></li>'
        f'<li><a href="{escape(shown_releases, quote=True)}">GitHub releases</a><p>Every published build.</p></li>'
        f'<li><a href="{REPO}/blob/main/browser/README.md">Build from source</a><p>Clone <a href="{REPO}">bitbonsai/myarc</a> and compile it yourself.</p></li>'
        '</ul>'
        '<p class="dl-legal">'
        f'GPL-3.0 (<a href="{REPO}/blob/main/LICENSE">LICENSE</a>). '
        f'Bundled components keep their own licenses (<a href="{REPO}/blob/main/ATTRIBUTION.md">ATTRIBUTION.md</a>, '
        f'<a href="{REPO}/blob/main/browser/evidence/dependency-licenses.json">dependency inventory</a>). '
        'Independent project, separate from Arc and The Browser Company.'
        '</p>'
        '</section>'
    )
    return sections


COPY_SCRIPT = """<script>
  (() => {
    document.querySelectorAll('.dl-copy').forEach((button) => {
      if (!navigator.clipboard) { button.hidden = true; return }
      let timer = 0
      button.addEventListener('click', async () => {
        try {
          await navigator.clipboard.writeText(button.dataset.copy)
          button.textContent = 'Copied'
          clearTimeout(timer)
          timer = setTimeout(() => { button.textContent = 'Copy' }, 1800)
        } catch {}
      })
    })
  })()
</script>"""


CSS = '''
    .journal.download-page { max-width: 64rem; padding: 3rem 0 5rem }
    .download-page { --dl-surface: color-mix(in srgb, var(--page) 92%, rgb(var(--link-rgb))); --dl-border: color-mix(in srgb, currentColor 12%, transparent) }

    /* Hero: editorial title left, tactile release card right over a subtle ribbon wash. */
    .dl-hero { position: relative; display: grid; grid-template-columns: minmax(0, 1.15fr) minmax(0, 1fr); align-items: start; gap: 2.5rem 3.5rem }
    .dl-hero::before { content: ''; position: absolute; z-index: -1; top: -4rem; right: -4rem; width: 36rem; height: 28rem; pointer-events: none; filter: blur(3.5rem); opacity: .45;
      background: radial-gradient(42% 45% at 35% 40%, rgb(26 133 255 / .42), transparent 70%), radial-gradient(36% 42% at 68% 35%, rgb(124 108 255 / .36), transparent 70%), radial-gradient(40% 42% at 55% 72%, rgb(255 122 92 / .3), transparent 70%) }
    .download-page h1 { font-size: 4rem; line-height: 1.02; letter-spacing: -.045em; max-width: 14ch }
    .dl-lede { margin-top: 1.25rem; max-width: 42ch; color: var(--quiet); font-size: 1.25rem; line-height: 1.48 }
    .dl-promises { display: grid; grid-template-columns: repeat(2, minmax(0, 1fr)); gap: .625rem 1.25rem; margin-top: 1.5rem; padding: 0; list-style: none }
    .dl-promises li { display: flex; align-items: baseline; gap: .5rem; font-size: 1rem; font-weight: 500; color: var(--text) }
    .dl-check { width: .9375rem; height: .9375rem; flex: none; color: var(--link-solid); transform: translateY(.125rem) }

    /* Release card */
    .download-box { padding: 1.75rem; border-radius: 1.5rem; border: 1px solid var(--dl-border);
      background: color-mix(in srgb, var(--page) 82%, transparent);
      -webkit-backdrop-filter: blur(1.25rem); backdrop-filter: blur(1.25rem);
      box-shadow: inset 0 1px 0 rgb(255 255 255 / .55), 0 1.25rem 2.75rem -1.5rem rgb(16 28 43 / .22) }
    .dl-coming h2 { font-size: 1.5rem }
    .dl-coming p { margin-top: .75rem; color: var(--quiet) }
    .dl-card-head { display: flex; flex-wrap: wrap; align-items: baseline; justify-content: space-between; gap: .25rem 1rem }
    .dl-version { font-size: 1.375rem; font-weight: 600; letter-spacing: -.02em; font-variant-numeric: tabular-nums }
    .dl-date { color: var(--quiet); font-size: 1rem; white-space: nowrap }
    .dl-cta { display: flex; justify-content: center; width: 100%; margin-top: 1.25rem; padding: 1rem 1.5rem; font-size: 1.125rem }
    .dl-icon { width: 1.25rem; height: 1.25rem; flex: none }
    .dl-submeta { margin-top: .75rem; text-align: center; color: var(--quiet); font-size: .9375rem }
    .dl-upcoming { display: grid; grid-template-columns: repeat(2, minmax(0, 1fr)); gap: .625rem; margin-top: 1rem }
    .dl-os-btn { display: flex; align-items: center; justify-content: space-between; gap: .5rem; width: 100%; padding: .6875rem 1rem; border-radius: 6rem; border: 1px dashed color-mix(in srgb, currentColor 28%, transparent); background: color-mix(in srgb, currentColor 4%, transparent); color: var(--quiet); font: inherit; font-size: .9375rem; font-weight: 500; cursor: not-allowed }
    .dl-soon { font-size: .875rem; font-weight: 500; opacity: .72 }

    /* Progressive SHA-256 verification disclosure */
    .dl-verify { margin-top: 1.25rem; padding-top: 1rem; border-top: 1px solid var(--dl-border) }
    .dl-verify summary { display: flex; align-items: center; justify-content: space-between; gap: .75rem; list-style: none; cursor: pointer; color: var(--quiet); font-size: .9375rem; font-weight: 500; transition: color .15s ease }
    .dl-verify summary::-webkit-details-marker { display: none }
    .dl-verify summary:hover, .dl-verify summary:focus-visible { color: var(--text) }
    .dl-chevron { width: 1rem; height: 1rem; flex: none; transition: transform .2s var(--ease) }
    .dl-verify[open] .dl-chevron { transform: rotate(180deg) }
    .dl-verify-body { margin-top: .875rem }
    .dl-hash-label { color: var(--quiet); font-size: .875rem; font-weight: 600; letter-spacing: .04em; text-transform: uppercase }
    .dl-hash { display: block; margin-top: .25rem; font-family: inherit; font-size: .9375rem; line-height: 1.45; word-break: break-all; color: var(--text) }
    .dl-cmd { display: flex; align-items: center; gap: .625rem; margin-top: .75rem; padding: .5rem .5rem .5rem 1rem; border-radius: .875rem; background: var(--panel); color: var(--ivory) }
    .dl-cmd pre { flex: 1; min-width: 0; margin: 0; padding: .375rem 0; overflow-x: auto; background: none; color: inherit; font-family: inherit; font-size: .9375rem; line-height: 1.45 }
    .dl-cmd code { font-family: inherit; white-space: nowrap }
    .dl-copy { flex: none; padding: .375rem .75rem; border: 1px solid rgb(255 255 255 / .22); border-radius: 999rem; background: rgb(255 255 255 / .1); color: var(--ivory); font: inherit; font-size: .875rem; font-weight: 500; cursor: pointer; transition: background-color .15s ease }
    .dl-copy:hover, .dl-copy:focus-visible { background: rgb(255 255 255 / .2) }

    /* Sections below hero */
    .dl-section { margin-top: 4rem }
    .dl-section-head, .dl-panel-head { display: flex; flex-wrap: wrap; align-items: baseline; justify-content: space-between; gap: .5rem 1rem }
    .download-page h2 { font-size: 1.75rem; letter-spacing: -.03em; line-height: 1.2 }
    .dl-more { font-size: 1rem; font-weight: 500; white-space: nowrap }

    /* 3-step horizontal install strip (unboxed editorial columns, matching the homepage rhythm). */
    .dl-steps { display: grid; grid-template-columns: repeat(3, minmax(0, 1fr)); gap: 1.75rem 2.5rem; margin: 1.75rem 0 0; padding: 1.75rem 0 0; list-style: none; border-top: 1px solid var(--dl-border) }
    .dl-step-num { color: var(--quiet); font-weight: 500; font-variant-numeric: tabular-nums }
    .dl-steps h3 { font-size: 1.25rem; letter-spacing: -.02em }
    .dl-steps p { margin-top: .5rem; color: var(--quiet); font-size: 1rem; line-height: 1.5 }

    /* Two-column Ready to try / Keep in mind */
    .dl-split { display: grid; grid-template-columns: repeat(2, minmax(0, 1fr)); gap: 1.5rem }
    .dl-panel { padding: 1.75rem; border-radius: 1.5rem; border: 1px solid var(--dl-border); background: color-mix(in srgb, var(--page) 78%, transparent) }
    .dl-panel-quiet { background: color-mix(in srgb, var(--page) 92%, rgb(var(--link-rgb) / .2)) }
    .dl-list { margin: 1.25rem 0 0; padding: 0; list-style: none }
    .dl-list li { font-size: 1rem; line-height: 1.5; color: var(--quiet) }
    .dl-list li + li { margin-top: .875rem; padding-top: .875rem; border-top: 1px solid color-mix(in srgb, currentColor 10%, transparent) }
    .dl-list strong { color: var(--text); font-weight: 600 }
    .download-page kbd { padding: .05em .35em; border: 1px solid color-mix(in srgb, currentColor 18%, transparent); border-bottom-width: 2px; border-radius: .375rem; background: color-mix(in srgb, currentColor 6%, transparent); font-family: inherit; font-size: .9375rem; font-weight: 500; color: var(--text); white-space: nowrap }

    /* Bottom links + compact legal */
    .dl-footer-bar { padding-top: 2.5rem; border-top: 1px solid var(--dl-border) }
    .dl-links { display: grid; grid-template-columns: repeat(4, minmax(0, 1fr)); gap: 1.25rem 2rem; padding: 0; list-style: none }
    .dl-links > li > a { font-weight: 600; font-size: 1.0625rem }
    .dl-links p { margin-top: .25rem; color: var(--quiet); font-size: 1rem; line-height: 1.45 }
    .dl-legal { margin-top: 2rem; color: var(--quiet); font-size: .9375rem; line-height: 1.55; max-width: 72ch }

    @media (prefers-color-scheme: dark) {
      .download-page { --dl-border: rgb(255 255 255 / .12) }
      .download-box { background: color-mix(in srgb, var(--panel) 84%, transparent);
        box-shadow: inset 0 1px 0 rgb(255 255 255 / .12), 0 1.5rem 3rem -1.5rem rgb(0 0 0 / .75) }
      .dl-os-btn { background: rgb(255 255 255 / .03) }
      .dl-panel { background: color-mix(in srgb, var(--panel) 55%, transparent) }
      .dl-panel-quiet { background: color-mix(in srgb, var(--panel) 72%, rgb(var(--link-rgb) / .14)) }
      .dl-cmd { background: #09111b; border: 1px solid rgb(255 255 255 / .08) }
    }

    @media (max-width: 56rem) {
      .dl-hero { grid-template-columns: minmax(0, 1fr); gap: 2rem }
      .dl-hero::before { right: -2rem; width: 26rem; height: 22rem }
      .dl-steps, .dl-split { grid-template-columns: minmax(0, 1fr) }
      .dl-links { grid-template-columns: repeat(2, minmax(0, 1fr)) }
    }

    @media (max-width: 48rem) {
      .journal.download-page { padding: 2rem 0 4rem }
      .download-page h1 { font-size: 2.75rem }
      .dl-lede { font-size: 1.125rem }
      .dl-promises, .dl-links { grid-template-columns: minmax(0, 1fr) }
      .download-box, .dl-panel { padding: 1.375rem }
      .dl-section { margin-top: 3rem }
    }

    @media (prefers-reduced-motion: reduce) {
      .dl-chevron, .dl-verify summary, .dl-copy { transition: none }
    }
'''


def apply_release(template, release):
    """Add the header link and homepage call to action only for a published release."""
    link = '<a class="button download-link" href="/download/">Download alpha</a>' if release else ''
    template = template.replace('<!--download-nav-->', link)
    big = f'<a class="button stacked" href="/download/">Download alpha <small>v{escape(release["version"])}</small></a>' if release else ''
    works = big
    template = template.replace('<!--works-download-->', works)
    latest = f'<a href="/download/">Download myArc Alpha {release["version"]} &rarr;</a> ' if release else ''
    template = template.replace('<!--latest-release-->', latest)
    if release:
        template = re.sub(r'<!--cta-->.*?<!--/cta-->', lambda _: big, template, count=1, flags=re.S)
    return re.sub(r'<!--/?cta-->', '', template)


def build_download(root, template, embed, release):
    page = page_shell(template, TITLE, SUMMARY, URL, content(release))
    schema = {'@context': 'https://schema.org', '@graph': [
        {'@type': 'WebPage', 'name': TITLE, 'description': SUMMARY, 'url': URL, 'inLanguage': 'en', 'isPartOf': {'@type': 'WebSite', 'name': 'myArc', 'url': 'https://myarc.dev/'}},
        {'@type': 'BreadcrumbList', 'itemListElement': [{'@type': 'ListItem', 'position': 1, 'name': 'myArc', 'item': 'https://myarc.dev/'}, {'@type': 'ListItem', 'position': 2, 'name': 'Download', 'item': URL}]}]}
    page = re.sub(r'(<script type="application/ld\+json">).*?(</script>)', lambda match: match[1] + json.dumps(schema).replace('<', '\\u003c') + match[2], page, count=1, flags=re.S)
    page = page.replace(' aria-current="page"', '').replace('<main id="main" class="journal">', '<main id="main" class="journal download-page">', 1)
    page = page.replace('</style>', CSS + '  </style>', 1)
    if release:
        page = page.replace('class="button download-link"', 'class="button download-link" aria-current="page"')
        page = page.replace('</body>', COPY_SCRIPT + '</body>', 1)
    folder = Path(root) / 'download'
    folder.mkdir(exist_ok=True)
    # Shared hashed assets live with the blog, which the build always creates first.
    (folder / 'index.html').write_text(embed(page, '../blog/'))
