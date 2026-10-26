"""Print draft GitHub prerelease notes from release.json."""
from pathlib import Path
import json
import sys

from download import release_notes, validate

try:
    data = json.loads((Path(__file__).resolve().parent / 'release.json').read_text())
    validate(data)
except (ValueError, KeyError, OSError) as error:
    raise SystemExit(f'Release notes failed: {error}') from None
sys.stdout.write(release_notes(data))
