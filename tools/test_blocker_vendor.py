#!/usr/bin/env python3
"""Checks the vendored uBlock Origin Lite release against the code using it.

  python3 tools/test_blocker_vendor.py
"""

import base64
import hashlib
import json
from pathlib import Path
import re
import unittest
import zipfile

ROOT = Path(__file__).resolve().parents[1]
FOUNDATION = ROOT / 'browser/foundation'
VENDOR = FOUNDATION / 'blocker'
FIXTURE = ROOT / 'tests/fixtures/blocker'


def bundled_zip():
    sources = re.search(
        r'bundle_data\("blocker_bundle_data"\) \{\s*sources = \[ "([^"]+)" \]',
        (FOUNDATION / 'BUILD.gn').read_text())
    return FOUNDATION / sources.group(1)


class VendoredBlockerTest(unittest.TestCase):
    def setUp(self):
        self.zip_path = bundled_zip()
        self.archive = zipfile.ZipFile(self.zip_path)
        self.manifest = json.loads(self.archive.read('manifest.json'))

    def tearDown(self):
        self.archive.close()

    def test_hashes_match_sha256sums(self):
        lines = (VENDOR / 'SHA256SUMS').read_text().split('\n')
        listed = dict(reversed(line.split('  ', 1)) for line in lines if line)
        self.assertIn(self.zip_path.name, listed)
        for name, digest in listed.items():
            with self.subTest(name=name):
                data = (VENDOR / name).read_bytes()
                self.assertEqual(hashlib.sha256(data).hexdigest(), digest)

    def test_zip_name_matches_manifest_version(self):
        version = self.manifest['version']
        self.assertEqual(self.zip_path.name, f'uBOLite_{version}.chromium.zip')
        readme = (VENDOR / 'README.md').read_text()
        self.assertIn(version, readme)

    def test_license_is_the_release_license(self):
        self.assertEqual(self.archive.read('LICENSE.txt'),
                         (VENDOR / 'LICENSE.txt').read_bytes())
        self.assertIn(b'GNU GENERAL PUBLIC LICENSE',
                      (VENDOR / 'LICENSE.txt').read_bytes()[:200])

    def test_extension_id_comes_from_the_public_key(self):
        source = (FOUNDATION / 'blocker.cc').read_text()
        block = re.search(r'kBlockerPublicKey\[\] =\s*((?:"[^"]*"\s*)+);',
                          source).group(1)
        key = base64.b64decode(''.join(re.findall(r'"([^"]*)"', block)))
        digest = hashlib.sha256(key).hexdigest()[:32]
        derived = ''.join(chr(ord('a') + int(c, 16)) for c in digest)
        declared = re.search(r'kBlockerExtensionId\[\] =\s*"([a-p]{32})"',
                             (FOUNDATION / 'blocker.h').read_text()).group(1)
        self.assertEqual(derived, declared)
        self.assertIn(f'EXT={declared}', (ROOT / 'tools/check_blocker.sh').read_text())

    def test_no_update_url_so_no_update_checks(self):
        self.assertNotIn('update_url', self.manifest)

    def test_fixture_tracker_matches_an_enabled_rule(self):
        resources = self.manifest['declarative_net_request']['rule_resources']
        enabled = {r['id']: r['path'].lstrip('/') for r in resources if r['enabled']}
        self.assertIn('easyprivacy', enabled)
        rules = json.loads(self.archive.read(enabled['easyprivacy']))
        matching = [
            r for r in rules
            if r['action']['type'] == 'block'
            and r['condition'] == {'urlFilter': '/cogi-etracker.js'}
        ]
        self.assertEqual(len(matching), 1)
        self.assertIn("'/cogi-etracker.js", (FIXTURE / 'index.html').read_text()
                      .replace('`', "'"))


if __name__ == '__main__':
    unittest.main()
