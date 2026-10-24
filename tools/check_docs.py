#!/usr/bin/env python3
"""Check inline relative Markdown link paths in this planning repository.

This intentionally does not validate remote URLs, fragments, reference-style
links, browser behavior, secret safety, or security claims.
"""

from pathlib import Path
import re
import sys
from urllib.parse import unquote, urlsplit


def main() -> int:
    root = Path(__file__).resolve().parents[1]
    documents = sorted(
        path for path in root.rglob('*.md')
        if '.git' not in path.relative_to(root).parts
    )
    failures = []
    checked = 0
    for document in documents:
        source = document.read_text(encoding='utf-8')
        for match in re.finditer(r'!?\[[^\]\n]*\]\(([^)\n]+)\)', source):
            destination = match.group(1).strip().strip('<>')
            parsed = urlsplit(destination)
            if parsed.scheme or parsed.netloc or not parsed.path:
                continue
            checked += 1
            target = (document.parent / unquote(parsed.path)).resolve()
            line = source.count('\n', 0, match.start()) + 1
            if not target.is_relative_to(root) or not target.exists():
                failures.append(
                    f'{document.relative_to(root)}:{line}: {destination}'
                )
    if failures:
        print('Broken or out-of-repository document paths:', file=sys.stderr)
        print('\n'.join(failures), file=sys.stderr)
        return 1
    print(f'Checked {checked} relative link paths in {len(documents)} Markdown files')
    return 0


if __name__ == '__main__':
    raise SystemExit(main())
