"""Build the pre-alpha downloads page and release notes from site/release.json."""
from datetime import date
from html import escape
from pathlib import Path
import json
import re

from blog import page_shell

URL = 'https://myarc.dev/download/'
REPO = 'https://github.com/bitbonsai/myarc'
KEYS = {'published', 'version', 'date', 'dmg_url', 'size_bytes', 'sha256', 'release_url', 'min_macos', 'proprietary_codecs'}
DEFAULTS = {'release_url': REPO + '/releases', 'min_macos': '26', 'proprietary_codecs': False}
TITLE = 'myArc pre-alpha nightly'
STATUS = 'Pre-alpha nightly, for trying the Arc-like shell. Not for daily or sensitive browsing.'
SUMMARY = 'Pre-alpha nightly of myArc for macOS on Apple silicon: requirements, first launch, what works, known limitations and how to report problems.'
APP = '/Applications/myArc Nightly.app'

WORKS = [
    'Chromium browsing, with Chromium extensions. A Manifest V3 smoke test passed; wider extension coverage is untested.',
    'A glass sidebar with Favorite tiles, a Space heading, pinned tabs, a New Tab row and temporary tabs below the divider, next to a rounded page card. Only the dark appearance has been compared with Arc.',
    'Creating, switching and renaming Spaces from the sidebar footer. Favorites are shared across the Spaces of one Profile.',
    'Closing a pinned tab keeps the pin.',
    'A command bar: Cmd+L opens it on the current address, arrows, Enter and Escape work, and new windows open it centered.',
    'Shortcuts: Cmd+S hides the sidebar, Cmd+Shift+T reopens a closed tab, Cmd+Shift+C copies the address, Cmd+Shift+N opens a private window and Ctrl+Cmd+N a blank one.',
    'Quitting with Cmd+Q needs no confirmation by default, with a setting to turn the warning on.',
]
MISSING = [
    'Importing from Arc. The importer is not implemented.',
    'Cmd+D to pin a tab (Cmd+D still bookmarks), folders, Auto Archive, the Archive and drag-to-pin.',
    'Peek, Split View and Little Arc. The Peek routing rule exists, but there is no overlay.',
    'Boosts, the Library, download progress in the sidebar and the Mini Player.',
    'CLI and MCP browser control, and the animated onboarding.',
    'Light mode, Reduce Transparency and fullscreen layouts are not verified.',
]


def read_release(root):
    """Return the validated release when it is published, else None."""
    path = Path(root) / 'release.json'
    if not path.is_file():
        return None
    try:
        data = json.loads(path.read_text())
    except json.JSONDecodeError:
        raise ValueError('release.json is not valid JSON') from None
    if not isinstance(data, dict) or set(data) != KEYS:
        raise ValueError('release.json needs exactly: ' + ', '.join(sorted(KEYS)))
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


def facts(release):
    day = date.fromisoformat(release['date'])
    return {'Version': release['version'], 'Date': f'{day:%B} {day.day}, {day.year}', 'Size': f'{release["size_bytes"] / 1e6:.1f} MB', 'SHA-256': release['sha256']}


def requirements(release):
    minimum = (release or DEFAULTS)['min_macos']
    return ['An Apple silicon Mac. Intel Macs are not supported.',
            f'macOS {minimum} or later. The glass frame requires macOS 26. It has been tested only on macOS 26.7.']


def limitations(release):
    codecs = (release or DEFAULTS)['proprietary_codecs']
    return ['Built from a Chromium 158 development revision, not a stable release.',
            'No automatic updates and no security updates. Install each new nightly by hand.',
            'After you install a newer nightly, macOS asks once for your login password so it can read the saved-data key. Choose Always Allow.',
            'The app is ad-hoc signed and not notarized by Apple.',
            'Recent tabs and navigation can be lost after a crash or force quit.',
            'Private window, Profile, cookie and credential guarantees are not verified.',
            'No Widevine, so Netflix, Spotify and Disney+ will not play.',
            'H.264 and AAC are included, so most other web video plays.' if codecs else 'H.264 and AAC are not included, so some web video will not play.',
            'myArc is an independent project, not affiliated with Arc or The Browser Company.']


def first_launch(release):
    name = release['dmg_url'].rsplit('/', 1)[1]
    return [('Check the download against the SHA-256 listed with it. The two values must match.', f'shasum -a 256 ~/Downloads/{name}'),
            ('Open the DMG and drag myArc Nightly to Applications.', None),
            ('macOS blocks the first launch because the app is not notarized. Right-click myArc Nightly, choose Open, then Open again. If macOS only offers Done or Move to Trash, which macOS 15 and later usually does, open System Settings > Privacy & Security, scroll to the myArc Nightly message and choose Open Anyway.', None),
            ('Or remove the quarantine flag in Terminal and open the app normally.', f'xattr -dr com.apple.quarantine "{APP}"')]


def release_notes(release):
    name = release['dmg_url'].rsplit('/', 1)[1]
    shown = facts(release)
    out = [f'# myArc pre-alpha nightly {release["version"]}', '', STATUS, '', '## Download', '',
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


def item_list(lines):
    return '<ul>' + ''.join(f'<li>{escape(line)}</li>' for line in lines) + '</ul>'


def content(release):
    if release:
        rows = ''.join(f'<li><strong>{label}:</strong> {"<code>" + escape(value) + "</code>" if label == "SHA-256" else escape(value)}</li>' for label, value in facts(release).items())
        box = f'<div class="download-box"><a class="button" href="{escape(release["dmg_url"], quote=True)}">Download for macOS (Apple silicon)</a><ul class="download-facts">{rows}</ul></div>'
    else:
        box = f'<div class="download-box"><h2>First nightly coming soon</h2><p>There is no download yet. The <a href="{escape(DEFAULTS["release_url"], quote=True)}">releases page</a> will list each nightly when it exists.</p></div>'
    html = box + '<h2>System requirements</h2>' + item_list(requirements(release))
    if release:
        steps = ''
        for text, code in first_launch(release):
            steps += f'<li>{escape(text)}' + (f'<pre><code>{escape(code)}</code></pre>' if code else '') + '</li>'
        html += f'<h2>First launch</h2><ol>{steps}</ol>'
    html += '<h2>What works today</h2><p>Each item was seen working in a native run or experiment note. None of it has had release testing.</p>' + item_list(WORKS)
    html += '<h2>What does not work yet</h2>' + item_list(MISSING)
    html += '<h2>Known limitations and safety</h2>' + item_list(limitations(release))
    shown = (release or DEFAULTS)['release_url']
    html += ('<h2>Other ways to get it</h2><ul>'
             f'<li><a href="{escape(shown, quote=True)}">GitHub releases</a> lists every nightly.</li>'
             f'<li><a href="{REPO}">Source on GitHub</a>, with instructions to <a href="{REPO}/blob/main/browser/README.md">build it from source</a>.</li>'
             f'<li><a href="{REPO}/issues">GitHub issues</a> take bug reports. Include the version, your macOS version and the steps that trigger the problem, and leave out private pages.</li></ul>')
    html += ('<h2>License and notices</h2>'
             f'<p>myArc is MIT licensed, see <a href="{REPO}/blob/main/LICENSE">LICENSE</a>. Chromium and other bundled components keep their own licenses. <a href="{REPO}/blob/main/ATTRIBUTION.md">ATTRIBUTION.md</a> records what is known, and the <a href="{REPO}/blob/main/browser/evidence/dependency-licenses.json">dependency license inventory</a> is not yet complete.</p>')
    return f'<header class="post-header"><h1>{TITLE}</h1><p class="download-status">{STATUS}</p></header><div class="post-body download">{html}</div>'


CSS = '''
    .download-status { margin-top: 1rem; color: var(--quiet) }
    .download .download-box { padding: 1.5rem; border-radius: 1rem; background: rgb(var(--link-rgb) / .08) }
    .download .download-box h2 { margin-top: 0; font-size: 1.5rem }
    .download .download-box p { margin-top: .75rem }
    .download .download-facts { margin-top: 1.25rem; padding: 0; list-style: none; font-size: 1rem }
    .download .download-facts code { overflow-wrap: anywhere }
    .download ol { padding-left: 1.5rem }
    .download li > pre { margin-top: .75rem }
'''


def apply_release(template, release):
    """Add the header link and homepage call to action only for a published release."""
    link = '<a class="download-link" href="/download/">Download</a>' if release else ''
    template = template.replace('<!--download-nav-->', link)
    if release:
        cta = '<a class="button" href="/download/">Download pre-alpha</a>'
        template = re.sub(r'<!--cta-->.*?<!--/cta-->', lambda _: cta, template, count=1, flags=re.S)
    return re.sub(r'<!--/?cta-->', '', template)


def build_download(root, template, embed, release):
    page = page_shell(template, TITLE, SUMMARY, URL, content(release))
    schema = {'@context': 'https://schema.org', '@graph': [
        {'@type': 'WebPage', 'name': TITLE, 'description': SUMMARY, 'url': URL, 'inLanguage': 'en', 'isPartOf': {'@type': 'WebSite', 'name': 'myArc', 'url': 'https://myarc.dev/'}},
        {'@type': 'BreadcrumbList', 'itemListElement': [{'@type': 'ListItem', 'position': 1, 'name': 'myArc', 'item': 'https://myarc.dev/'}, {'@type': 'ListItem', 'position': 2, 'name': 'Download', 'item': URL}]}]}
    page = re.sub(r'(<script type="application/ld\+json">).*?(</script>)', lambda match: match[1] + json.dumps(schema).replace('<', '\\u003c') + match[2], page, count=1, flags=re.S)
    page = page.replace(' aria-current="page"', '').replace('</style>', CSS + '  </style>', 1)
    if release:
        page = page.replace('class="download-link"', 'class="download-link" aria-current="page"')
    folder = Path(root) / 'download'
    folder.mkdir(exist_ok=True)
    # Shared hashed assets live with the blog, which the build always creates first.
    (folder / 'index.html').write_text(embed(page, '../blog/'))

