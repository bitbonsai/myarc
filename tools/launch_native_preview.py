#!/usr/bin/env python3
"""Launch the current native build in a disposable or persistent review profile."""

import argparse
import json
import os
from pathlib import Path
import subprocess
import sys
import tempfile


def running_review_pid(executable: Path, profile: Path):
    processes = subprocess.run(
        ['ps', '-axo', 'pid=,comm='], capture_output=True, text=True, check=True
    )
    for row in processes.stdout.splitlines():
        fields = row.strip().split(None, 1)
        if len(fields) != 2 or fields[1] != str(executable):
            continue
        command = subprocess.run(
            ['ps', '-ww', '-p', fields[0], '-o', 'command='],
            capture_output=True, text=True,
        )
        if f'--user-data-dir={profile} ' in command.stdout.strip() + ' ':
            return fields[0]
    return None


def main(argv=None) -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument(
        '--review', nargs='?', const='native-preview-review', metavar='PROFILE_NAME',
        help='reuse a marked native-preview-* cache profile; quit it before relaunching',
    )
    args = parser.parse_args(argv)
    review = args.review is not None
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
        marker_text = 'Owned synthetic M0 browser test profile\n'
        if review:
            if (not args.review.startswith('native-preview-')
                    or Path(args.review).name != args.review
                    or len(args.review.encode('utf-8')) > 128):
                raise OSError('Review profile must name a direct native-preview-* cache directory')
            profile = work / args.review
            if profile.is_symlink():
                raise OSError(f'Review profile must not be a symlink: {profile}')
            fresh = not profile.exists()
            if not fresh:
                marker = profile / '.myarc-m0-test-profile'
                if not profile.is_dir() or marker.is_symlink() or not marker.is_file():
                    raise OSError(f'Refusing unmarked review profile: {profile}')
                with marker.open('rb') as source:
                    if source.read(len(marker_text) + 1) != marker_text.encode('utf-8'):
                        raise OSError(f'Refusing unmarked review profile: {profile}')
                if (profile / 'Default').is_symlink():
                    raise OSError(f'Refusing symlinked review Default: {profile}')
            if pid := running_review_pid(executable, profile):
                raise OSError(
                    f'Review preview is already running (PID {pid}). '
                    'Quit its window before relaunching the current build.'
                )
            if fresh:
                profile.mkdir(mode=0o700)
        else:
            profile = Path(tempfile.mkdtemp(prefix='native-preview-', dir=work)).resolve()
            fresh = True
        print(f'{"Review" if review else "Disposable"} profile: {profile}', flush=True)
        pid_file = profile / 'preview.pid'
        print('Keep important data in your normal browser. UI remains unverified.', flush=True)
        if fresh:
            (profile / '.myarc-m0-test-profile').write_text(marker_text, encoding='utf-8')
            (profile / 'Default').mkdir()
            (profile / 'Default/Preferences').write_text(
                json.dumps({'vertical_tabs': {'enabled': True}}) + '\n', encoding='utf-8'
            )
        output_file = (
            tempfile.NamedTemporaryFile(
                mode='w', prefix='preview-', suffix='.log', dir=profile,
                encoding='utf-8', delete=False,
            )
            if review else (profile / 'preview.log').open('x', encoding='utf-8')
        )
        with output_file as output:
            log = Path(output.name)
            print(f'Browser log: {log}\nPID file: {pid_file}', flush=True)
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
                    *(['--profile-directory=Default'] if review else []),
                    *(['--restore-last-session'] if not fresh else ['about:blank']),
                ],
                stdin=subprocess.DEVNULL,
                stdout=output,
                stderr=subprocess.STDOUT,
                start_new_session=True,
            )
        print(f'Started process (UI unverified): {process.pid}', flush=True)
        pid_file.write_text(f'{process.pid}\n', encoding='utf-8')
    except (OSError, subprocess.SubprocessError) as error:
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
