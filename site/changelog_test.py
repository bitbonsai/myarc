"""The Recent updates dialog lists changelog.md, and a release can't ship without its entry."""
from pathlib import Path
import tempfile
import unittest

from bs4 import BeautifulSoup

from changelog import check_release, dialog, load_changelog
from download import apply_release

TEMPLATE = (Path(__file__).parent / 'index.template.html').read_text()
RELEASE = {'version': '2099.01.02.1'}
TEXT = '''# Changelog

Intro.

## v2099.01.02.1 (2099-01-02)

Fixed `New Space` clicks
([#1](https://github.com/bitbonsai/myarc/issues/1)). <b>raw</b>

## v2099.01.01 (2099-01-01)

First build.
'''


def load(text):
    with tempfile.TemporaryDirectory() as directory:
        (Path(directory) / 'changelog.md').write_text(text)
        return load_changelog(directory)


class ChangelogTests(unittest.TestCase):
    def test_entries_render_newest_first_without_raw_html(self):
        entries = load(TEXT)
        self.assertEqual([version for version, _, _ in entries], ['2099.01.02.1', '2099.01.01'])
        page = BeautifulSoup(dialog(entries, RELEASE), 'html.parser')
        self.assertEqual('Recent updates v2099.01.02.1', page.h2.get_text(' ', strip=True))
        first = page.select('.changelog-list li')[0]
        self.assertEqual('v2099.01.02.1 (January 2, 2099):', first.strong.get_text())
        self.assertEqual('New Space', first.code.get_text())
        self.assertEqual('https://github.com/bitbonsai/myarc/issues/1', first.a['href'])
        self.assertIsNone(first.b)
        self.assertEqual(['works-link'], page.select_one('.works-actions a')['class'])

    def test_bad_files_are_refused(self):
        self.assertEqual(load('# Changelog\n'), [])
        for text in ('## 2099.01.02 (2099-01-02)\n\nNo v.\n', '## v2099.01.02 (2099-13-02)\n\nBad date.\n',
                     '## v2099.01.02 (2099-01-02)\n\n', '## v2099.01.02 (2099-01-02)\n\n' + 'x' * 601 + '\n',
                     '## v2099.01.01 (2099-01-01)\n\nOld.\n\n## v2099.01.02 (2099-01-02)\n\nNew.\n',
                     '## v2099.01.02 (2099-01-02)\n\nA.\n\n## v2099.01.02 (2099-01-02)\n\nB.\n'):
            with self.assertRaises(ValueError, msg=text):
                load(text)

    def test_release_needs_the_newest_entry(self):
        entries = load(TEXT)
        check_release(entries, RELEASE)
        check_release([], None)
        for release, items in (({'version': '2099.01.01'}, entries), (RELEASE, [])):
            with self.assertRaises(ValueError):
                check_release(items, release)

    def test_version_link_opens_the_dialog_only_when_published(self):
        sample = {**RELEASE, 'release_url': 'https://github.com/bitbonsai/myarc/releases/tag/x'}
        on = BeautifulSoup(apply_release(TEMPLATE, sample, dialog(load(TEXT), sample)), 'html.parser')
        self.assertEqual('v2099.01.02.1', on.select_one('.cta .changelog-link').get_text())
        self.assertIsNotNone(on.select_one('dialog#changelog'))
        self.assertIsNone(on.select_one('.cta .works-link'))
        off = apply_release(TEMPLATE, None, dialog(load(TEXT), sample))
        self.assertNotIn('id="changelog"', off)
        self.assertIn('class="works-link"', off)


if __name__ == '__main__':
    unittest.main()
