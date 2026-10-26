"""Launcher checks use synthetic files and a Popen double, never native Chromium."""

from contextlib import redirect_stderr, redirect_stdout
import io
import json
import os
from pathlib import Path
import stat
import subprocess
import tempfile
import unittest
from unittest.mock import Mock, patch

import launch_native_preview as launcher


class NativePreviewTests(unittest.TestCase):
    def setUp(self):
        temporary = tempfile.TemporaryDirectory(prefix='myarc preview ')
        self.addCleanup(temporary.cleanup)
        self.home = Path(temporary.name)
        self.work = self.home / '.cache/myarc-m0'
        self.executable = (
            self.work / 'chromium/src/out/MyarcM0/Chromium.app/Contents/MacOS/Chromium'
        )
        self.executable.parent.mkdir(parents=True)
        self.executable.write_text('Synthetic executable, never run\n', encoding='utf-8')
        self.executable.chmod(0o700)
        home_patch = patch.object(Path, 'home', return_value=self.home)
        home_patch.start()
        self.addCleanup(home_patch.stop)
        process_patch = patch.object(launcher.subprocess, 'Popen', return_value=Mock(pid=43210))
        self.popen = process_patch.start()
        self.addCleanup(process_patch.stop)
        ps_patch = patch.object(
            launcher.subprocess, 'run',
            return_value=subprocess.CompletedProcess([], 0, stdout='', stderr=''),
        )
        self.ps = ps_patch.start()
        self.addCleanup(ps_patch.stop)
        self.stdout = io.StringIO()
        self.stderr = io.StringIO()

    def run_launcher(self, args=None):
        with redirect_stdout(self.stdout), redirect_stderr(self.stderr):
            return launcher.main([] if args is None else args)

    def profiles(self):
        return sorted(self.work.glob('native-preview-*'))

    def test_exact_invocation_and_profile_seeded_before_launch(self):
        def start(command, **kwargs):
            profile = Path(command[1].split('=', 1)[1])
            self.assertEqual(profile.parent, self.work.resolve())
            self.assertEqual(
                json.loads((profile / 'Default/Preferences').read_text()),
                {'vertical_tabs': {'enabled': True}},
            )
            self.assertEqual(
                (profile / '.myarc-m0-test-profile').read_text(),
                'Owned synthetic M0 browser test profile\n',
            )
            self.assertEqual(command, [
                str(self.executable), f'--user-data-dir={profile}',
                '--myarc-visual-parity', '--myarc-workspace-sidebar',
                '--enable-features=GlassFrame', '--disable-features=InitialWebUI',
                '--no-first-run', '--no-default-browser-check', 'about:blank',
            ])
            self.assertEqual(kwargs, {
                'stdin': subprocess.DEVNULL, 'stdout': kwargs['stdout'],
                'stderr': subprocess.STDOUT, 'start_new_session': True,
            })
            self.assertEqual(Path(kwargs['stdout'].name), profile / 'preview.log')
            kwargs['stdout'].write('Synthetic browser output\n')
            return Mock(pid=43210)

        self.popen.side_effect = start
        self.assertEqual(self.run_launcher(), 0)
        self.popen.assert_called_once()
        profile, = self.profiles()
        self.assertEqual((profile / 'preview.pid').read_text(), '43210\n')
        self.assertEqual((profile / 'preview.log').read_text(), 'Synthetic browser output\n')
        for path in (profile, profile / 'preview.log', profile / 'preview.pid'):
            self.assertIn(str(path), self.stdout.getvalue())
        self.assertIn('Started process (UI unverified): 43210', self.stdout.getvalue())

    def test_private_permissions_and_umask_restored(self):
        previous = os.umask(0o022)
        try:
            self.assertEqual(self.run_launcher(), 0)
            observed = os.umask(0o022)
            self.assertEqual(observed, 0o022)
        finally:
            os.umask(previous)
        profile, = self.profiles()
        self.assertEqual(stat.S_IMODE(profile.stat().st_mode), 0o700)
        self.assertEqual(stat.S_IMODE((profile / 'Default').stat().st_mode), 0o700)
        for relative in ('.myarc-m0-test-profile', 'Default/Preferences', 'preview.log', 'preview.pid'):
            self.assertEqual(stat.S_IMODE((profile / relative).stat().st_mode), 0o600)

    def test_each_run_is_fresh_and_existing_profiles_untouched(self):
        existing = self.work / 'qa-profile/Default/Preferences'
        existing.parent.mkdir(parents=True)
        existing.write_text('Existing synthetic QA sentinel\n', encoding='utf-8')
        self.assertEqual(self.run_launcher(), 0)
        self.assertEqual(self.run_launcher(), 0)
        self.assertEqual(len(self.profiles()), 2)
        self.assertNotEqual(self.popen.call_args_list[0].args, self.popen.call_args_list[1].args)
        self.assertEqual(existing.read_text(), 'Existing synthetic QA sentinel\n')
        self.assertFalse((self.work / 'Default').exists())

    def test_missing_binary_fails_before_profile_creation(self):
        self.executable.unlink()
        self.assertEqual(self.run_launcher(), 1)
        self.popen.assert_not_called()
        self.assertEqual(self.profiles(), [])
        self.assertIn(str(self.executable), self.stderr.getvalue())
        self.assertIn('browser/README.md', self.stderr.getvalue())

    def test_nonexecutable_binary_rejected(self):
        self.executable.chmod(0o600)
        self.assertEqual(self.run_launcher(), 1)
        self.popen.assert_not_called()
        self.assertEqual(self.profiles(), [])

    def test_directory_instead_of_binary_rejected(self):
        self.executable.unlink()
        self.executable.mkdir()
        self.assertEqual(self.run_launcher(), 1)
        self.popen.assert_not_called()
        self.assertEqual(self.profiles(), [])

    def test_arguments_cannot_select_profile_or_inject_flags(self):
        for args in (['--user-data-dir=/tmp/existing'], ['--remote-debugging-port=9222'], ['about:blank']):
            with self.subTest(args=args), self.assertRaises(SystemExit) as raised:
                self.run_launcher(args)
            self.assertEqual(raised.exception.code, 2)
        self.popen.assert_not_called()
        self.assertEqual(self.profiles(), [])

    def test_review_reuses_marked_profile_without_replacing_saved_state(self):
        self.assertEqual(self.run_launcher(['--review']), 0)
        profile, = self.profiles()
        self.assertEqual(profile.name, 'native-preview-review')
        first = self.popen.call_args.args[0]
        self.assertIn('--profile-directory=Default', first)
        self.assertIn('about:blank', first)
        preferences = profile / 'Default/Preferences'
        preferences.write_text('{"vertical_tabs":{"enabled":false},"sentinel":1}\n')
        state = profile / 'Default/myarc-workspace-v1.json'
        state.write_text('Synthetic saved-state sentinel\n')
        first_log, = profile.glob('preview-*.log')
        first_log.write_text('Existing log sentinel\n')
        self.assertEqual(self.run_launcher(['--review']), 0)
        self.assertEqual(self.profiles(), [profile])
        second = self.popen.call_args.args[0]
        self.assertEqual(first[1], second[1])
        self.assertIn('--restore-last-session', second)
        self.assertNotIn('about:blank', second)
        self.assertEqual(preferences.read_text(), '{"vertical_tabs":{"enabled":false},"sentinel":1}\n')
        self.assertEqual(state.read_text(), 'Synthetic saved-state sentinel\n')
        self.assertEqual(first_log.read_text(), 'Existing log sentinel\n')
        self.assertEqual(len(list(profile.glob('preview-*.log'))), 2)
        for log in profile.glob('preview-*.log'):
            self.assertEqual(stat.S_IMODE(log.stat().st_mode), 0o600)

    def test_review_can_reuse_existing_disposable_profile_without_moving_data(self):
        self.assertEqual(self.run_launcher(), 0)
        profile, = self.profiles()
        state = profile / 'Default/myarc-workspace-v1.json'
        state.write_text('Existing synthetic Space Pin\n')
        self.assertEqual(self.run_launcher(['--review', profile.name]), 0)
        self.assertEqual(self.profiles(), [profile])
        self.assertEqual(state.read_text(), 'Existing synthetic Space Pin\n')
        self.assertIn(f'--user-data-dir={profile}', self.popen.call_args.args[0])
        self.assertIn('--restore-last-session', self.popen.call_args.args[0])

    def test_review_rejects_paths_and_nonpreview_names(self):
        for name in ('', '/tmp/native-preview-real', '../native-preview-other', 'qa-profile',
                     'native-preview-' + 'x' * 128):
            with self.subTest(name=name):
                self.assertEqual(self.run_launcher(['--review', name]), 1)
        self.popen.assert_not_called()
        self.assertEqual(self.profiles(), [])

    def test_review_refuses_unmarked_or_invalid_marker_without_writing(self):
        profile = self.work / 'native-preview-review'
        profile.mkdir()
        sentinel = profile / 'sentinel'
        sentinel.write_text('Existing directory\n')
        self.assertEqual(self.run_launcher(['--review']), 1)
        marker = profile / '.myarc-m0-test-profile'
        marker.write_text('Different owner\n')
        self.assertEqual(self.run_launcher(['--review']), 1)
        self.assertEqual(sentinel.read_text(), 'Existing directory\n')
        self.assertEqual(marker.read_text(), 'Different owner\n')
        self.assertFalse((profile / 'Default').exists())
        self.popen.assert_not_called()

    def test_review_rejects_binary_and_oversized_markers(self):
        profile = self.work / 'native-preview-review'
        profile.mkdir()
        marker = profile / '.myarc-m0-test-profile'
        for content in (b'\xff' * 38, b'Owned synthetic M0 browser test profile\n' + b'x' * 10000):
            with self.subTest(length=len(content)):
                marker.write_bytes(content)
                self.assertEqual(self.run_launcher(['--review']), 1)
                self.assertEqual(marker.read_bytes(), content)
        self.popen.assert_not_called()

    def test_review_rejects_symlinked_default_and_marker(self):
        self.assertEqual(self.run_launcher(['--review']), 0)
        profile, = self.profiles()
        other = self.work / 'qa-profile'
        other.mkdir()
        (other / 'sentinel').write_text('Untouched\n')
        (profile / 'Default').rename(profile / 'saved-default')
        (profile / 'Default').symlink_to(other, target_is_directory=True)
        self.popen.reset_mock()
        self.assertEqual(self.run_launcher(['--review']), 1)
        (profile / 'Default').unlink()
        (profile / 'saved-default').rename(profile / 'Default')
        marker = profile / '.myarc-m0-test-profile'
        marker.rename(profile / 'saved-marker')
        marker.symlink_to(profile / 'saved-marker')
        self.assertEqual(self.run_launcher(['--review']), 1)
        self.popen.assert_not_called()
        self.assertEqual((other / 'sentinel').read_text(), 'Untouched\n')
        self.assertEqual(len(list(other.iterdir())), 1)

    def test_review_refuses_symlink_to_other_profile(self):
        other = self.work / 'qa-profile'
        other.mkdir()
        (self.work / 'native-preview-review').symlink_to(other, target_is_directory=True)
        self.assertEqual(self.run_launcher(['--review']), 1)
        self.assertEqual(list(other.iterdir()), [])
        self.popen.assert_not_called()

    def test_review_refuses_live_exact_profile_without_reusing_old_process(self):
        self.assertEqual(self.run_launcher(['--review']), 0)
        profile, = self.profiles()
        before = {p.name: p.read_bytes() for p in profile.iterdir() if p.is_file()}
        self.popen.reset_mock()
        self.ps.side_effect = [
            subprocess.CompletedProcess([], 0, stdout=f'123 {self.executable}\n'),
            subprocess.CompletedProcess([], 0, stdout=f'{self.executable} --user-data-dir={profile}\n'),
        ]
        self.assertEqual(self.run_launcher(['--review']), 1)
        self.popen.assert_not_called()
        self.assertEqual(before, {p.name: p.read_bytes() for p in profile.iterdir() if p.is_file()})
        self.assertIn('Quit its window', self.stderr.getvalue())

    def test_review_ignores_other_executables_and_profile_name_prefix_matches(self):
        profile = self.work / 'native-preview-review'
        self.ps.side_effect = [
            subprocess.CompletedProcess([], 0, stdout=f'12 /unrelated\n34 {self.executable}\n'),
            subprocess.CompletedProcess([], 0, stdout=f'{self.executable} --user-data-dir={profile}-other\n'),
        ]
        self.assertEqual(self.run_launcher(['--review']), 0)
        self.popen.assert_called_once()
        self.assertEqual(self.ps.call_count, 2)

    def test_review_process_inspection_failure_does_not_launch(self):
        self.ps.side_effect = subprocess.CalledProcessError(1, 'ps')
        self.assertEqual(self.run_launcher(['--review']), 1)
        self.popen.assert_not_called()
        self.assertEqual(self.profiles(), [])

    def test_help_does_not_create_profile_or_launch(self):
        with self.assertRaises(SystemExit) as raised:
            self.run_launcher(['--help'])
        self.assertEqual(raised.exception.code, 0)
        self.popen.assert_not_called()
        self.assertEqual(self.profiles(), [])

    def test_process_error_retains_private_profile_and_restores_umask(self):
        self.popen.side_effect = OSError('Synthetic process failure')
        previous = os.umask(0o022)
        try:
            self.assertEqual(self.run_launcher(), 1)
            observed = os.umask(0o022)
            self.assertEqual(observed, 0o022)
        finally:
            os.umask(previous)
        profile, = self.profiles()
        self.assertTrue((profile / 'preview.log').is_file())
        self.assertFalse((profile / 'preview.pid').exists())
        self.assertIn(str(profile), self.stdout.getvalue())
        self.assertIn('Synthetic process failure', self.stderr.getvalue())
        self.assertNotIn('Started process', self.stdout.getvalue())

    def test_profile_creation_error_does_not_launch(self):
        with patch.object(launcher.tempfile, 'mkdtemp', side_effect=OSError('Synthetic disk failure')):
            self.assertEqual(self.run_launcher(), 1)
        self.popen.assert_not_called()
        self.assertEqual(self.profiles(), [])
        self.assertIn('Synthetic disk failure', self.stderr.getvalue())


if __name__ == '__main__':
    unittest.main()
