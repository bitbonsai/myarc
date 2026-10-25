#!/usr/bin/env python3
"""Launch the already-built native experiment in a fresh disposable profile."""

import argparse
import json
import os
from pathlib import Path
import subprocess
import sys
import tempfile


def main(argv=None) -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.parse_args(argv)
    work = Path.home() / '.cache/myarc-m0'
    executable = work / 'chromium/src/out/MyarcM0/Chromium.app/Contents/MacOS/Chromium'
    if not executable.is_file() or not os.access(executable, os.X_OK):
        print(
            f'Native Chromium executable missing or not executable: {executable}\n'
            'Ask the build owner to finish/verify browser/README.md before retrying.\n'
            'This launcher does not build or install Chromium.',
            file=sys.stderr,
        )
        return 1

    previous_umask = os.umask(0o077)
    try:
        profile = Path(tempfile.mkdtemp(prefix='native-preview-', dir=work)).resolve()
        print(f'Disposable profile: {profile}', flush=True)
        log = profile / 'preview.log'
        pid_file = profile / 'preview.pid'
        print(f'Browser log: {log}\nPID file: {pid_file}', flush=True)
        print('Keep important data in your normal browser. UI remains unverified.', flush=True)
        (profile / '.myarc-m0-test-profile').write_text(
            'Owned synthetic M0 browser test profile\n', encoding='utf-8'
        )
        (profile / 'Default').mkdir()
        (profile / 'Default/Preferences').write_text(
            json.dumps({'vertical_tabs': {'enabled': True}}) + '\n', encoding='utf-8'
        )
        with log.open('x', encoding='utf-8') as output:
            process = subprocess.Popen(
                [
                    str(executable),
                    f'--user-data-dir={profile}',
                    '--myarc-visual-parity',
                    '--myarc-workspace-sidebar',
                    '--enable-features=GlassFrame',
                    '--disable-features=InitialWebUI',
                    '--no-first-run',
                    '--no-default-browser-check',
                    'about:blank',
                ],
                stdin=subprocess.DEVNULL,
                stdout=output,
                stderr=subprocess.STDOUT,
                start_new_session=True,
            )
        print(f'Started process (UI unverified): {process.pid}', flush=True)
        pid_file.write_text(f'{process.pid}\n', encoding='utf-8')
    except OSError as error:
        print(
            f'Could not prepare/start native preview: {error}\n'
            'Any printed profile/log is retained. Check its log and directory access.\n'
            'If a PID was printed, that process may still be running.',
            file=sys.stderr,
        )
        return 1
    finally:
        os.umask(previous_umask)
    return 0


if __name__ == '__main__':
    raise SystemExit(main())
