#!/usr/bin/env python3
"""Verify that the Chromium patch contains the current myarc-owned sources."""

import json
from pathlib import Path
import subprocess
import tempfile

ROOT = Path(__file__).resolve().parents[1]
expected = {
    'myarc/LICENSE': ROOT / 'LICENSE',
    'myarc/workspace/src/lib.rs': ROOT / 'crates/workspace/src/lib.rs',
    **{
        f'myarc/{name}': ROOT / 'browser/foundation' / name
        for name in (
            'BUILD.gn',
            'bridge.rs',
            'bridge_check.cc',
            'native_button.cc',
            'native_button.h',
        )
    },
}
pin = json.loads((ROOT / 'browser/upstream.json').read_text())
with tempfile.TemporaryDirectory(prefix='myarc-patch-') as directory:
    target = Path(directory)
    for patch in pin['patches']:
        subprocess.run(
            ['git', 'apply', '--include=myarc/*', str(ROOT / 'browser' / patch)],
            cwd=target,
            check=True,
        )
    actual = {
        path.relative_to(target).as_posix(): path
        for path in target.rglob('*')
        if path.is_file()
    }
    assert actual.keys() == expected.keys(), 'Foundation patch source inventory differs'
    for name, source in expected.items():
        assert actual[name].read_bytes() == source.read_bytes(), f'Patch is stale: {name}'
print(f'Foundation patch matches {len(expected)} myarc-owned files')
