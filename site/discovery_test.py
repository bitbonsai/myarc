"""Markdown fidelity and draft exclusion use the real rendering/export pipeline."""
from pathlib import Path
import tempfile
import unittest

from PIL import Image
from blog import build_blog
from discovery import build_discovery, markdown_page

TEMPLATE = (Path(__file__).parent / 'index.template.html').read_text()


class DiscoveryTests(unittest.TestCase):
    def test_markdown_keeps_content_and_uses_absolute_links(self):
        html = '<main><h1>myArc,<br>forever yours.</h1><p>Real <strong>content</strong>.</p><img alt="Brand" src="data:image/png;base64,eA=="><img alt="Target" src="target.webp"><a href="index.html">HTML</a><p><code>source.png</code></p></main>'
        text = markdown_page(html, 'https://myarc.dev/blog/example/')
        self.assertIn('# myArc, forever yours.', text)
        self.assertIn('**content**', text)
        self.assertIn('https://myarc.dev/blog/example/target.webp', text)
        self.assertIn('`source.png`', text)
        self.assertNotIn('data:', text)

    def test_exports_only_published_content_and_tracks_unpublishing(self):
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            folder = root / 'posts/2026-10-25-public-post'
            folder.mkdir(parents=True)
            source = folder / 'index.md'
            source.write_text('---\ntitle: Published finding\nsummary: Verified result\ndraft: false\n---\nReal **evidence**.\n\n![Target](screen.png)')
            Image.new('RGB', (40, 20), 'purple').save(folder / 'screen.png')
            draft = root / 'posts/2026-10-24-draft'
            draft.mkdir()
            (draft / 'index.md').write_text('---\ntitle: Unapproved private note\ndraft: true\n---\nUnapproved private body')
            posts = build_blog(root, TEMPLATE, lambda html, prefix: html)
            homepage = '<main><h1>myArc</h1><p>Browser project.</p></main>'
            build_discovery(root, homepage, posts)
            exported = (root / 'blog' / folder.name / 'index.md').read_text()
            self.assertIn('**evidence**', exported)
            self.assertIn(posts[0].cover['url'], exported)
            self.assertNotIn('screen.png', exported)
            full = (root / 'llms-full.txt').read_text()
            self.assertNotIn('Unapproved private', full)
            self.assertIn('Published finding', full)
            self.assertIn('Published finding', (root / 'llms.txt').read_text())
            source.write_text(source.read_text().replace('draft: false', 'draft: true'))
            posts = build_blog(root, TEMPLATE, lambda html, prefix: html)
            build_discovery(root, homepage, posts)
            self.assertFalse((root / 'blog' / folder.name / 'index.md').exists())
            self.assertNotIn('Published finding', (root / 'llms-full.txt').read_text())
            self.assertNotIn('Published finding', (root / 'llms.txt').read_text())


if __name__ == '__main__':
    unittest.main()
