#!/usr/bin/env python3
"""Exercise the synthetic motion study through agent-browser, on loopback only."""

from datetime import datetime, timezone
from functools import partial
import hashlib
from http.server import SimpleHTTPRequestHandler, ThreadingHTTPServer
import json
from pathlib import Path
import shutil
import subprocess
import sys
from threading import Thread
from time import sleep

ROOT = Path(__file__).resolve().parents[1]
PATTERNS = ('/[a]gent-browser-darwin-arm64', '[G]oogle Chrome for Testing')


class QuietHandler(SimpleHTTPRequestHandler):
    def log_message(self, format, *args):
        pass


def main():
    if not shutil.which('agent-browser') or not shutil.which('ffprobe'):
        raise SystemExit('Install agent-browser, its Chrome, and ffmpeg/ffprobe before running this check.')
    stamp = datetime.now(timezone.utc).strftime('%Y%m%dT%H%M%S%fZ')
    output = ROOT / 'out' / 'onboarding' / stamp
    output.mkdir(parents=True, exist_ok=False)
    checks = []
    recording = False
    session = 'myarc-study-' + stamp.lower()
    handler = partial(QuietHandler, directory=str(ROOT / 'design' / 'onboarding'))
    server = ThreadingHTTPServer(('127.0.0.1', 0), handler)
    thread = Thread(target=server.serve_forever, daemon=True)
    thread.start()
    manifest = {
        'started': stamp, 'status': 'failed', 'checks': checks,
        'sources': {
            str(path.relative_to(ROOT)): hashlib.sha256(path.read_bytes()).hexdigest()
            for path in [*sorted((ROOT / 'design' / 'onboarding').glob('*.mjs')),
                         ROOT / 'design/onboarding/index.html', ROOT / 'design/onboarding/study.css',
                         Path(__file__).resolve()]
        },
    }

    def browser(*args, source=None):
        result = subprocess.run(
            ['agent-browser', '--session', session, '--json', *args],
            input=source, text=True, capture_output=True, timeout=60,
        )
        with (output / 'commands.log').open('a') as log:
            log.write(json.dumps(list(args)) + '\n' + result.stdout + result.stderr + '\n')
        result.check_returncode()
        response = json.loads(result.stdout)
        if not response.get('success'):
            raise RuntimeError(response.get('error', 'agent-browser failed'))
        if recording and args[0] in ('click', 'press', 'fill', 'check'):
            sleep(0.4)  # Let reviewers see recorded actions; never delays the study UI.
        return response.get('data', {})

    def check(expression, label):
        code = f'if (!({expression})) throw new Error({json.dumps(label)}); true'
        browser('eval', '--stdin', source=code)
        checks.append(label)
        print('PASS ' + label, flush=True)

    def click(action):
        browser('click', f'[data-action="{action}"]')

    def scene(name):
        check(f'document.querySelector("#study").dataset.scene === {json.dumps(name)}', 'scene: ' + name)

    def settled():
        browser('eval', '--stdin', source='(async () => { await Promise.all(document.getAnimations().map(animation => animation.finished)); return true })()')

    def video(name):
        data = json.loads(subprocess.check_output([
            'ffprobe', '-v', 'error', '-count_frames', '-show_entries',
            'format=duration:stream=nb_read_frames,width,height', '-of', 'json',
            str(output / name),
        ], text=True))
        if float(data['format']['duration']) < 2 or int(data['streams'][0]['nb_read_frames']) < 10:
            raise AssertionError('Recording too short or empty: ' + name)
        manifest.setdefault('videos', {})[name] = data
        checks.append('validated multi-frame recording: ' + name)

    try:
        manifest['agentBrowser'] = subprocess.check_output(['agent-browser', '--version'], text=True).strip()
        browser('--profile', str(output / 'profile'), 'open', f'http://127.0.0.1:{server.server_port}')
        browser('set', 'viewport', '1440', '1000')
        manifest['userAgent'] = browser('eval', 'navigator.userAgent')['result']
        browser('record', 'start', str(output / 'normal.webm'))
        recording = True
        browser('click', '#restart')
        settled()
        browser('screenshot', str(output / 'welcome.png'))
        scene('welcome')
        click('begin')
        scene('source')
        click('review')
        scene('preview')
        check('document.querySelector(".summary").textContent.includes("18 synthetic pins")', 'preview reports fixture counts')
        click('apply')
        scene('reveal')
        settled()
        browser('screenshot', str(output / 'reveal.png'))
        click('practice')
        check('document.activeElement.id === "scene-title"', 'scene change focuses heading')
        browser('click', '[data-action="pin"][data-id="pin-0"]')
        check('document.querySelector(".page h2").textContent === "Field notes"', 'pin opens local sample content')
        check('document.activeElement.dataset.id === "pin-0"', 'pin action retains keyboard focus')
        click('next')
        browser('click', '[data-action="space"][data-id="weekend"]')
        check('document.querySelector(".profile-name").textContent === "Personal Profile"', 'two Spaces retain shared Profile label')
        check('document.querySelector(".space-name").textContent === "Weekend"', 'Space switch changes visible workspace')
        click('next')
        browser('press', 'Meta+t')
        check('document.querySelector("dialog").open && document.activeElement.id === "command-query"', 'Cmd+T opens focused command dialog')
        browser('fill', '#command-query', 'Field notes')
        browser('click', '.command-result:first-child')
        check('!document.querySelector("dialog").open && document.activeElement.dataset.action === "pin"', 'search result opens sample and restores focus')
        click('next')
        scene('complete')
        check('document.querySelector(".page h2").textContent === "Field notes"', 'handoff preserves selected sample page')
        click('focusWorkspace')
        check('document.activeElement.classList.contains("space")', 'handoff offers useful workspace focus')
        browser('screenshot', str(output / 'handoff.png'))
        browser('record', 'stop')
        recording = False
        video('normal.webm')

        browser('record', 'start', str(output / 'reduced.webm'))
        recording = True
        browser('set', 'media', 'light', 'reduced-motion')
        check('matchMedia("(prefers-reduced-motion: reduce)").matches && document.getAnimations().length === 0', 'OS reduced motion honored')
        click('fresh')
        scene('reveal')
        check('document.getAnimations().length === 0', 'empty reveal has no spatial motion under OS preference')
        click('practice')
        check('document.querySelectorAll("button.pin").length === 0', 'empty sample supports guidance without fake pins')
        click('next')
        click('skip')
        scene('complete')
        browser('screenshot', str(output / 'reduced.png'))
        browser('record', 'stop')
        recording = False
        video('reduced.webm')
        browser('reload')
        check('matchMedia("(prefers-reduced-motion: reduce)").matches && document.getAnimations().length === 0', 'OS reduced motion honored on first frame after reload')

        browser('click', '#restart')
        click('begin')
        browser('check', 'input[value="failure"]')
        click('review')
        click('apply')
        scene('preview')
        check('document.querySelector("[role=alert]").textContent.includes("unchanged")', 'failed sample never enters reveal')
        click('apply')
        scene('reveal')
        check('!document.querySelector("[role=alert]")', 'retry clears error and applies sample')

        browser('click', '#restart')
        click('begin')
        browser('check', 'input[value="large"]')
        click('review')
        click('back')
        check('document.querySelector("input[value=large]").checked', 'Back retains selected sample')
        click('review')
        click('apply')
        check('document.querySelectorAll(".pin").length === 12', 'large fixture bounds initial visible sample to 12 pins')
        browser('select', '#motion', 'full')
        check('document.querySelectorAll(".pin").length === 12 && document.getAnimations().length <= 9', 'large reveal bounds animated elements to eight pins plus artwork')
        browser('select', '#motion', 'reduce')
        check('document.getAnimations().length === 0', 'motion preference can change during reveal')
        click('practice')
        click('more')
        check('document.querySelectorAll(".pin").length === 24', 'more pins reveals next bounded page')
        click('command')
        browser('fill', '#command-query', 'no-such-synthetic-pin')
        check('document.querySelector("#command-results").textContent.includes("No matching")', 'command search exposes empty result')
        browser('press', 'Escape')
        check('!document.querySelector("dialog").open && document.activeElement.dataset.action === "command"', 'Escape closes dialog and restores opener focus')
        browser('set', 'viewport', '390', '844')
        check('document.documentElement.scrollWidth <= innerWidth', 'mobile viewport has no horizontal overflow')
        browser('screenshot', str(output / 'mobile.png'), '--full')
        browser('set', 'offline', 'on')
        browser('click', '#restart')
        click('fresh')
        click('skip')
        scene('complete')
        check('localStorage.length === 0 && sessionStorage.length === 0', 'study writes no browser storage')
        browser('set', 'offline', 'off')
        browser('reload')
        scene('welcome')
        check('!document.querySelector("dialog").open', 'reload resets study instead of repeating sample application')
        snapshot = browser('snapshot', '-i')
        (output / 'accessibility.json').write_text(json.dumps(snapshot, indent=2) + '\n')
        errors = browser('errors')
        if errors.get('errors'):
            raise AssertionError('Browser page errors: ' + json.dumps(errors))
        checks.append('no browser page errors')
        manifest['status'] = 'passed'
    finally:
        try:
            browser('close')
        finally:
            server.shutdown()
            server.server_close()
            for pattern in PATTERNS:
                subprocess.run(['pkill', '-f', pattern], capture_output=True)
            remaining = [pattern for pattern in PATTERNS if subprocess.run(['pgrep', '-f', pattern], capture_output=True).returncode != 1]
            if remaining:
                manifest['status'] = 'failed'
                manifest['cleanupError'] = 'Browser helper processes remain or pgrep failed'
            manifest['finished'] = datetime.now(timezone.utc).isoformat()
            manifest['artifacts'] = {
                path.name: {'bytes': path.stat().st_size, 'sha256': hashlib.sha256(path.read_bytes()).hexdigest()}
                for path in output.iterdir() if path.is_file()
            }
            (output / 'manifest.json').write_text(json.dumps(manifest, indent=2) + '\n')
            print(f'{len(checks)} checks; status {manifest["status"]}; artifacts {output}', flush=True)
            if remaining:
                raise RuntimeError(manifest['cleanupError'])


if __name__ == '__main__':
    main()
