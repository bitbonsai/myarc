#!/usr/bin/env python3
"""Export the Chromium `myarc` branch as one patch per upstream file.

The branch in the external checkout is the source of truth. Its base is the
revision in browser/upstream.json. myarc-owned sources are not exported: the
checkout links `src/myarc` to this repository's browser/foundation directory.

  export_patches.py [--src SRC]          write browser/patches/chromium/*
  export_patches.py --check [--src SRC]  fail if exports are stale or do not
                                         apply to a clean base tree
  export_patches.py --apply SRC          apply exports to a fresh checkout at
                                         the base and create the symlink
"""

import argparse
import json
import os
from pathlib import Path
import subprocess
import sys
import tempfile

ROOT = Path(__file__).resolve().parents[1]
PATCHES = ROOT / 'browser/patches/chromium'
FOUNDATION = ROOT / 'browser/foundation'
BRANCH = 'myarc'
DEFAULT_SRC = Path.home() / '.cache/myarc-m0/chromium/src'
DIFF = [
    '-c', 'diff.noprefix=false', '-c', 'diff.mnemonicPrefix=false',
    '-c', 'diff.suppressBlankEmpty=false', '-c', 'diff.interHunkContext=0',
    'diff', '--no-color', '--no-ext-diff', '--no-renames', '--binary',
    '--full-index', '--diff-algorithm=myers', '--indent-heuristic', '-U3',
    '--src-prefix=a/', '--dst-prefix=b/',
]


def git(src, *args, env=None):
    return subprocess.run(
        ['git', '-C', str(src), *args], check=True, capture_output=True, env=env
    ).stdout


def base_revision():
    return json.loads((ROOT / 'browser/upstream.json').read_text())['revision']


def exports(src, base):
    names = git(src, 'diff', '--name-only', '-z', '--no-renames', base, BRANCH,
                '--', '.', ':(exclude)myarc')
    return {
        name: git(src, *DIFF, base, BRANCH, '--', name)
        for name in sorted(names.decode().split('\0')) if name
    }


def on_disk():
    return {
        path.relative_to(PATCHES).as_posix().removesuffix('.patch'): path
        for path in PATCHES.rglob('*.patch')
    }


def write(src, base):
    wanted = exports(src, base)
    for name, path in on_disk().items():
        if name not in wanted:
            path.unlink()
    for name, patch in wanted.items():
        path = PATCHES / f'{name}.patch'
        path.parent.mkdir(parents=True, exist_ok=True)
        if not path.exists() or path.read_bytes() != patch:
            path.write_bytes(patch)
    for directory in sorted(PATCHES.rglob('*'), reverse=True):
        if directory.is_dir() and not any(directory.iterdir()):
            directory.rmdir()
    print(f'Exported {len(wanted)} patches against {base[:10]}')


def apply_check(src, base, paths):
    with tempfile.TemporaryDirectory(prefix='myarc-index-') as directory:
        env = {**os.environ, 'GIT_INDEX_FILE': f'{directory}/index'}
        git(src, 'read-tree', base, env=env)
        failures = []
        for path in paths:
            result = subprocess.run(
                ['git', '-C', str(src), 'apply', '--cached', '--check', str(path)],
                capture_output=True, text=True, env=env,
            )
            if result.returncode:
                failures.append(result.stderr.strip())
        return failures


def check(src, base):
    wanted = exports(src, base)
    present = on_disk()
    stale = sorted(
        name for name in wanted.keys() | present.keys()
        if name not in present or name not in wanted
        or present[name].read_bytes() != wanted[name]
    )
    failures = apply_check(src, base, [present[name] for name in sorted(present)])
    for name in stale:
        print(f'stale: {name}.patch', file=sys.stderr)
    for failure in failures:
        print(failure, file=sys.stderr)
    if stale or failures:
        print('Run python3 tools/export_patches.py', file=sys.stderr)
        return 1
    print(f'{len(wanted)} patches match {BRANCH} and apply to {base[:10]}')
    return 0


def link(src):
    target = src / 'myarc'
    if target.is_symlink() and target.resolve() == FOUNDATION.resolve():
        return
    if target.exists() or target.is_symlink():
        raise SystemExit(f'{target} exists; move it aside first')
    target.symlink_to(FOUNDATION)
    exclude = Path(git(src, 'rev-parse', '--git-path', 'info/exclude').decode().strip())
    exclude = exclude if exclude.is_absolute() else src / exclude
    lines = exclude.read_text().splitlines() if exclude.exists() else []
    if '/myarc' not in lines:
        exclude.parent.mkdir(parents=True, exist_ok=True)
        exclude.write_text('\n'.join([*lines, '/myarc']) + '\n')


def apply(src, base):
    head = git(src, 'rev-parse', 'HEAD').decode().strip()
    if head != base:
        raise SystemExit(f'{src} is at {head}, expected base {base}')
    patches = [str(path) for _, path in sorted(on_disk().items())]
    subprocess.run(['git', '-C', str(src), 'apply', '--index', *patches], check=True)
    link(src)
    print(f'Applied {len(patches)} patches and linked myarc -> {FOUNDATION}')
    print(f'Next: git -C {src} switch -c {BRANCH} && git -C {src} commit')


def main():
    parser = argparse.ArgumentParser(description=__doc__.splitlines()[0])
    mode = parser.add_mutually_exclusive_group()
    mode.add_argument('--check', action='store_true')
    mode.add_argument('--apply', metavar='SRC', type=Path)
    parser.add_argument('--src', type=Path, default=DEFAULT_SRC)
    args = parser.parse_args()
    base = base_revision()
    if args.apply:
        apply(args.apply.resolve(), base)
        return 0
    if args.check:
        return check(args.src, base)
    write(args.src, base)
    return 0


if __name__ == '__main__':
    raise SystemExit(main())
