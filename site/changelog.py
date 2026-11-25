"""Read site/changelog.md and render the homepage's Recent updates dialog."""
from datetime import date
from html import escape
from pathlib import Path
import re

from markdown_it import MarkdownIt

REPO = 'https://github.com/bitbonsai/myarc'
HEADING = re.compile(r'## v(\d{4}\.\d{2}\.\d{2}(?:\.\d+)?) \((\d{4}-\d{2}-\d{2})\)')
MD = MarkdownIt('commonmark', {'html': False, 'maxNesting': 20})
MAX_ENTRIES = 200
MAX_BODY = 600


def load_changelog(root):
    """Return [(version, date, body markdown)], newest first, from changelog.md."""
    path = Path(root) / 'changelog.md'
    if not path.is_file():
        return []
    entries = []
    for block in re.split(r'^(?=## )', path.read_text(), flags=re.M)[1:]:
        heading, _, body = block.partition('\n')
        match = HEADING.fullmatch(heading.strip())
        if not match:
            raise ValueError(f'changelog.md: heading must look like "## v2026.11.25 (2026-11-25)", got {heading.strip()!r}')
        try:
            day = date.fromisoformat(match[2])
        except ValueError:
            raise ValueError(f'changelog.md: {heading.strip()} has an invalid date') from None
        body = ' '.join(body.split())
        if not body or len(body) > MAX_BODY:
            raise ValueError(f'changelog.md: v{match[1]} needs one paragraph of 1 to {MAX_BODY} characters')
        entries.append((match[1], day, body))
    if len(entries) > MAX_ENTRIES:
        raise ValueError(f'changelog.md: at most {MAX_ENTRIES} entries')
    if len({version for version, _, _ in entries}) != len(entries):
        raise ValueError('changelog.md: each version appears once')
    if [day for _, day, _ in entries] != sorted((day for _, day, _ in entries), reverse=True):
        raise ValueError('changelog.md: entries go newest first')
    return entries


def check_release(entries, release):
    """A published release needs the newest entry, so no build ships without its notes."""
    if release and (not entries or entries[0][0] != release['version']):
        raise ValueError(f'changelog.md: the first entry must be v{release["version"]}, the version in release.json')


def dialog(entries, release):
    """The Recent updates dialog for a published release, else nothing."""
    if not release:
        return ''
    items = ''.join(
        f'<li><strong>v{escape(version)} ({day:%B} {day.day}, {day.year}):</strong> {MD.renderInline(body)}</li>'
        for version, day, body in entries)
    close = ('<form method="dialog"><button class="works-close" aria-label="Close"><svg viewBox="0 0 24 24" fill="none" '
             'stroke="currentColor" stroke-width="2" stroke-linecap="round" stroke-linejoin="round" aria-hidden="true" '
             'focusable="false"><path d="M18 6 6 18"/><path d="m6 6 12 12"/></svg></button></form>')
    return (f'<dialog id="changelog" class="works changelog" aria-labelledby="changelog-title">{close}'
            f'<h2 id="changelog-title">Recent updates <span class="version-pill">v{escape(release["version"])}</span></h2>'
            f'<ul class="changelog-list">{items}</ul>'
            f'<div class="works-actions"><a class="works-link" href="{REPO}#try-it">What works today</a>'
            f'<a href="{REPO}/releases">All releases on GitHub →</a></div></dialog>')
