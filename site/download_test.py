"""The downloads page links a release asset only when release.json is published and valid."""
from pathlib import Path
import json
import tempfile
import unittest

import xml.etree.ElementTree as ET

from download import appcast, apply_release, build_download, bundle_version, content, limitations, read_release, release_notes

TEMPLATE = (Path(__file__).parent / 'index.template.html').read_text()
SAMPLE = {'published': True, 'version': '0.0.0-fake.1', 'date': '2099-01-02', 'size_bytes': 123456789,
          'dmg_url': 'https://github.com/bitbonsai/myarc/releases/download/fake-tag/myArc-Fake.dmg',
          'sha256': 'ab' * 32, 'release_url': 'https://github.com/bitbonsai/myarc/releases/tag/fake-tag',
          'min_macos': '26', 'proprietary_codecs': True}


def load(data):
    with tempfile.TemporaryDirectory() as directory:
        if data is not None:
            (Path(directory) / 'release.json').write_text(json.dumps(data))
        return read_release(directory)


class DownloadTests(unittest.TestCase):
    def test_absent_or_unpublished_release_has_no_download(self):
        self.assertIsNone(load(None))
        self.assertIsNone(load({**SAMPLE, 'published': False, 'sha256': '', 'size_bytes': 0}))
        html = content(None)
        self.assertIn('First alpha build coming soon', html)
        self.assertNotIn('.dmg', html)
        self.assertNotIn('class="button"', html)
        self.assertNotIn('First launch', html)

    def test_published_release_must_be_complete(self):
        self.assertEqual(load(SAMPLE), SAMPLE)
        for change in ({'sha256': 'AB' * 32}, {'sha256': ''}, {'dmg_url': 'https://example.com/a.dmg'},
                       {'dmg_url': SAMPLE['dmg_url'][:-4] + '.zip'}, {'date': '2099-13-01'}, {'size_bytes': 0},
                       {'min_macos': 'latest'}, {'proprietary_codecs': 'yes'}, {'published': 'true'}):
            with self.assertRaises(ValueError, msg=change):
                load({**SAMPLE, **change})
        with self.assertRaises(ValueError):
            load({**SAMPLE, 'extra': 1})

    def test_appcast_lists_only_a_signed_release(self):
        sparkle = '{http://www.andymatuschak.org/xml-namespaces/sparkle}'
        self.assertEqual(ET.fromstring(appcast(SAMPLE)).findall('channel/item'), [])
        self.assertEqual(ET.fromstring(appcast(None)).findall('channel/item'), [])
        signed = {**SAMPLE, 'version': '2099.01.02.3', 'ed_signature': 'A' * 86 + '=='}
        self.assertEqual(load(signed), signed)
        item = ET.fromstring(appcast(signed)).find('channel/item')
        self.assertEqual(item.find(sparkle + 'version').text, '20990102.3')
        enclosure = item.find('enclosure')
        self.assertEqual(enclosure.get('url'), SAMPLE['dmg_url'])
        self.assertEqual(enclosure.get('length'), str(SAMPLE['size_bytes']))
        self.assertEqual(enclosure.get(sparkle + 'edSignature'), signed['ed_signature'])
        self.assertEqual(bundle_version('2026.11.20'), '20261120.0')
        for change in ({'ed_signature': 'short=='}, {'version': '0.0.0-fake.1'}):
            with self.assertRaises(ValueError, msg=change):
                load({**signed, **change})

    def test_published_page_shows_values_and_steps(self):
        html = content(SAMPLE)
        self.assertIn(f'<a class="button" href="{SAMPLE["dmg_url"]}">Download for macOS (Apple silicon)</a>', html)
        for value in ('0.0.0-fake.1', 'January 2, 2099', '123.5 MB', SAMPLE['sha256'], 'shasum -a 256 ~/Downloads/myArc-Fake.dmg',
                      'Apple has notarized each build'):
            self.assertIn(value, html)
        self.assertIn('H.264 and AAC are included', html)
        self.assertIn('H.264 and AAC are not included', content({**SAMPLE, 'proprietary_codecs': False}))

    def test_release_notes_share_the_page_warnings(self):
        notes = release_notes(SAMPLE)
        self.assertIn(SAMPLE['sha256'], notes)
        self.assertIn('Apple has notarized each build', notes)
        self.assertNotIn('xattr', notes)
        for line in limitations(SAMPLE):
            self.assertIn(line, notes)

    def test_template_links_download_only_when_published(self):
        off, on = apply_release(TEMPLATE, None), apply_release(TEMPLATE, SAMPLE)
        self.assertNotIn('/download/', off)
        self.assertIn('Follow development', off)
        self.assertNotIn('<!--', off.split('<body>')[1])
        self.assertEqual(on.count('href="/download/"'), 4)
        self.assertIn(f"Download myArc Alpha {SAMPLE['version']}", on)
        self.assertIn('Download alpha', on)
        self.assertIn('See what works', on)

    def test_page_uses_site_shell_and_marks_itself_current(self):
        with tempfile.TemporaryDirectory() as directory:
            build_download(directory, apply_release(TEMPLATE, SAMPLE), lambda html, prefix: html, SAMPLE)
            page = (Path(directory) / 'download/index.html').read_text()
        self.assertIn('rel="canonical" href="https://myarc.dev/download/"', page)
        self.assertIn('"@type": "WebPage"', page)
        self.assertIn('class="button download-link" aria-current="page"', page)
        self.assertNotIn('class="blog-link" aria-current', page)
        self.assertIn('<h1>myArc Alpha</h1>', page)


if __name__ == '__main__':
    unittest.main()
