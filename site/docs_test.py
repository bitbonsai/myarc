"""User docs render with the site shell, group by area and reach the sitemap and llms.txt."""
from pathlib import Path
import json
import tempfile
import unittest

from bs4 import BeautifulSoup
from blog import build_blog
from discovery import build_discovery
from docs import URL, build_docs, load_docs

TEMPLATE = (Path(__file__).parent / 'index.template.html').read_text()


def write(root, slug, meta, body='Some **text**.\n\n## Section\n\nMore.\n'):
    folder = root / 'docs-src' / slug
    folder.mkdir(parents=True, exist_ok=True)
    (folder / 'index.md').write_text('---\n' + meta + '\n---\n' + body)


class DocsTests(unittest.TestCase):
    def setUp(self):
        self.directory = tempfile.TemporaryDirectory()
        self.root = Path(self.directory.name)

    def tearDown(self):
        self.directory.cleanup()

    def build(self):
        docs = load_docs(self.root)
        posts = build_blog(self.root, TEMPLATE, lambda html, prefix: html, [URL] + [doc.url for doc in docs])
        build_docs(self.root, TEMPLATE, lambda html, prefix: html, docs)
        build_discovery(self.root, '<main><h1>myArc</h1></main>', posts, docs)
        return docs

    def test_index_groups_by_area_and_pages_use_the_shell(self):
        write(self.root, 'later', 'title: Later\nsummary: Second page\narea: Getting started\norder: 2')
        write(self.root, 'first', 'title: First\nsummary: First page\narea: Getting started\norder: 1')
        write(self.root, 'help-page', 'title: Help page\nsummary: Help summary\narea: Help\norder: 1',
              '| Key | Action |\n| --- | --- |\n| Cmd+S | Hide |\n')
        docs = self.build()
        self.assertEqual(['first', 'later', 'help-page'], [doc.slug for doc in docs])
        index = BeautifulSoup((self.root / 'docs/index.html').read_text(), 'html.parser')
        self.assertEqual(['Getting started', 'Help', 'Contributing'], [h2.get_text() for h2 in index.select('.docs-index h2')])
        self.assertEqual(['first/', 'later/', 'help-page/'], [a['href'] for a in index.select('.docs-index li a')])
        self.assertEqual('page', index.select_one('a.docs-link')['aria-current'])
        self.assertIsNone(index.select_one('a.blog-link[aria-current]'))
        page = BeautifulSoup((self.root / 'docs/help-page/index.html').read_text(), 'html.parser')
        self.assertEqual('https://myarc.dev/docs/help-page/', page.select_one('link[rel="canonical"]')['href'])
        self.assertEqual('Help page', page.h1.get_text())
        self.assertIsNotNone(page.select_one('.post-body table td'))
        graph = json.loads(page.select_one('script[type="application/ld+json"]').string)['@graph']
        self.assertEqual(['WebPage', 'BreadcrumbList'], [node['@type'] for node in graph])
        sitemap = (self.root / 'sitemap.xml').read_text()
        self.assertIn('<loc>https://myarc.dev/docs/</loc>', sitemap)
        self.assertIn('<loc>https://myarc.dev/docs/help-page/</loc>', sitemap)
        llms = (self.root / 'llms.txt').read_text()
        self.assertIn('## Docs', llms)
        self.assertIn('[Help page](https://myarc.dev/docs/help-page/index.md): Help summary', llms)
        self.assertTrue((self.root / 'docs/help-page/index.md').read_text().startswith('HTML: https://myarc.dev/docs/help-page/'))

    def test_rejects_bad_metadata_headings_and_images(self):
        for meta, body in (('title: T\nsummary: S\narea: Elsewhere\norder: 1', 'Body'),
                           ('title: T\nsummary: S\narea: Help\norder: first', 'Body'),
                           ('title: T\nsummary: S\narea: Help', 'Body'),
                           ('title: T\nsummary: S\narea: Help\norder: 1', '# Second title'),
                           ('title: T\nsummary: S\narea: Help\norder: 1', '![Shot](shot.png)')):
            write(self.root, 'page', meta, body)
            with self.assertRaises(ValueError, msg=meta + body):
                load_docs(self.root)

    def test_raw_html_is_text(self):
        write(self.root, 'page', 'title: T\nsummary: S\narea: Help\norder: 1', '<script>alert(1)</script>')
        self.assertNotIn('<script>', load_docs(self.root)[0].html)


if __name__ == '__main__':
    unittest.main()
