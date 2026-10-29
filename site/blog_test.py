"""Real Markdown rendering, draft removal and screenshot boundary checks."""
from pathlib import Path
import tempfile
import json
import unittest

from PIL import Image
from bs4 import BeautifulSoup
from blog import build_blog, load_post

TEMPLATE = (Path(__file__).parent / 'index.template.html').read_text()


class BlogTests(unittest.TestCase):
    def setUp(self):
        self.temp = tempfile.TemporaryDirectory()
        self.root = Path(self.temp.name)
        self.folder = self.root / 'posts' / '2026-10-25-test-post'
        self.folder.mkdir(parents=True)
        self.source = self.folder / 'index.md'

    def tearDown(self):
        self.temp.cleanup()

    def post(self, body='A real **Markdown** paragraph.', draft='false', title='Test post'):
        self.source.write_text(f'---\ntitle: "{title}"\nsummary: "A synthetic test post."\ndraft: {draft}\n---\n\n{body}\n')

    def build(self):
        return build_blog(self.root, TEMPLATE, lambda value, prefix: value)

    def test_empty_index_and_default_draft(self):
        self.build()
        self.assertIn('Progress notes will appear here', (self.root / 'blog/index.html').read_text())
        self.source.write_text('---\ntitle: Private draft\n---\nUnapproved text')
        self.assertIsNone(load_post(self.source))
        self.assertEqual([], self.build())
        self.assertNotIn('Unapproved text', (self.root / 'blog/index.html').read_text())

    def test_publish_markdown_screenshot_and_unpublish(self):
        Image.new('RGB', (20, 10), 'blue').save(self.folder / 'screen.png')
        self.post('A **real** paragraph.\n\n## Details\n\n![Synthetic blue screenshot](screen.png)')
        posts = self.build()
        page = self.root / 'blog' / self.folder.name / 'index.html'
        html = page.read_text()
        self.assertEqual(1, len(posts))
        self.assertIn('<strong>real</strong>', html)
        self.assertIn('alt="Synthetic blue screenshot"', html)
        image_name = next(iter(posts[0].images))
        self.assertTrue(image_name.endswith('.webp'))
        self.assertFalse((page.parent / 'screen.png').exists())
        with Image.open(page.parent / image_name) as image:
            self.assertEqual('WEBP', image.format)
            self.assertEqual((20, 10), image.size)
        self.assertIn(posts[0].url, (self.root / 'sitemap.xml').read_text())
        self.assertIn('og:type" content="article"', html)
        self.assertIn('"datePublished": "2026-10-25"', html)
        cover_url = posts[0].cover['url']
        for tag in ('property="og:image"', 'property="og:image:secure_url"', 'name="twitter:image"'):
            self.assertIn(tag + ' content="' + cover_url + '"', html)
        self.assertIn('property="og:image:width" content="20"', html)
        self.assertIn('property="og:image:height" content="10"', html)
        self.assertIn('property="og:image:type" content="image/webp"', html)
        self.assertIn('property="og:image:alt" content="Synthetic blue screenshot"', html)
        self.assertIn('"image": "' + cover_url + '"', html)
        index = (self.root / 'blog/index.html').read_text()
        self.assertIn('class="post-thumb"', index)
        self.assertEqual(2, index.count(f'href="{posts[0].slug}/"'))
        self.assertIn('<p class="post-month">October 2026</p>', index)
        self.assertLess(index.index('<h2>'), index.index('<time class="post-date"'))
        self.assertLess(index.index('<time class="post-date"'), index.index('<p class="post-summary"'))
        self.assertIn('<p class="post-summary">A synthetic test post.</p>', index)
        self.assertNotIn('Progress journal', index)
        self.assertNotIn('Notes on building myArc, with screenshots', index)
        thumbnail = next((self.root / 'blog/_thumbnails').glob(posts[0].slug + '.*.webp'))
        with Image.open(thumbnail) as image:
            self.assertEqual('WEBP', image.format)
            self.assertLessEqual(image.width, 384)
            self.assertLessEqual(image.height, 240)
        self.post(draft='true')
        self.build()
        self.assertFalse(page.exists())
        self.assertFalse(thumbnail.exists())
        self.assertNotIn(posts[0].url, (self.root / 'sitemap.xml').read_text())

    def test_first_image_wins_and_no_image_keeps_brand_preview(self):
        self.post()
        self.build()
        html = (self.root / 'blog' / self.folder.name / 'index.html').read_text()
        self.assertIn('property="og:image" content="https://myarc.dev/og-image.webp"', html)
        self.assertNotIn('class="post-thumb"', (self.root / 'blog/index.html').read_text())
        Image.new('RGB', (40, 20), 'red').save(self.folder / 'first image.png')
        Image.new('RGB', (20, 10), 'blue').save(self.folder / 'second.png')
        self.post('![First & target](first%20image.png)\n\n![Second](second.png)')
        post = self.build()[0]
        self.assertTrue(post.cover['url'].startswith(post.url + 'first%20image.'))
        self.assertTrue(post.cover['url'].endswith('.webp'))
        self.assertEqual('First & target', post.cover['alt'])
        html = (self.root / 'blog' / self.folder.name / 'index.html').read_text()
        self.assertIn('content="First &amp; target"', html)
        self.assertIn('property="og:image:width" content="40"', html)
        previous = next((self.root / 'blog/_thumbnails').glob('*.webp'))
        with Image.open(previous) as image:
            self.assertGreater(image.getpixel((20, 10))[0], 240)
            self.assertLess(image.getpixel((20, 10))[2], 20)
        Image.new('RGB', (40, 20), 'green').save(self.folder / 'first image.png')
        self.build()
        current = next((self.root / 'blog/_thumbnails').glob('*.webp'))
        self.assertNotEqual(previous.name, current.name)
        self.assertFalse(previous.exists())

    def test_thumbnail_override_preserves_article_and_social_cover(self):
        Image.new('RGB', (40, 20), 'red').save(self.folder / 'first.png')
        self.post('![First](first.png)\n\n*First caption.*')
        baseline = self.build()[0]
        page = self.root / 'blog' / self.folder.name / 'index.html'
        original_page = page.read_bytes()
        original_sitemap = (self.root / 'sitemap.xml').read_bytes()
        logo = Image.new('RGBA', (800, 500))
        logo.paste((0, 0, 255, 255), (200, 50, 600, 450))
        logo.save(self.folder / 'logo.png')
        self.source.write_text(self.source.read_text().replace('draft: false', 'draft: false\nthumbnail: logo.png'))
        post = self.build()[0]
        self.assertEqual(baseline.html, post.html)
        self.assertEqual(baseline.images, post.images)
        self.assertEqual(baseline.cover, post.cover)
        self.assertEqual(original_page, page.read_bytes())
        self.assertEqual(original_sitemap, (self.root / 'sitemap.xml').read_bytes())
        self.assertEqual(baseline.source_bytes + (self.folder / 'logo.png').stat().st_size, post.source_bytes)
        thumbnail = next((self.root / 'blog/_thumbnails').glob('*.webp'))
        with Image.open(thumbnail) as image:
            self.assertEqual((384, 240), image.size)
            self.assertEqual('RGBA', image.mode)
            self.assertEqual(0, image.getpixel((0, 0))[3])
            self.assertEqual(255, image.getpixel((192, 120))[3])
            self.assertGreater(image.getpixel((192, 120))[2], 240)
        import hashlib
        self.assertIn(hashlib.sha256(thumbnail.read_bytes()).hexdigest()[:12], thumbnail.name)
        index = BeautifulSoup((self.root / 'blog/index.html').read_text(), 'html.parser')
        self.assertIn('background: transparent', index.style.string.split('.post-thumb img {', 1)[1].split('}', 1)[0])
        self.assertEqual([next(iter(post.images))], [p.name for p in page.parent.glob('*.webp')])

    def test_thumbnail_without_article_images_keeps_brand_social_cover(self):
        Image.new('RGBA', (80, 50), (0, 0, 255, 128)).save(self.folder / 'logo.png')
        self.post()
        self.source.write_text(self.source.read_text().replace('draft: false', 'draft: false\nthumbnail: logo.png'))
        post = self.build()[0]
        self.assertFalse(post.images)
        self.assertIsNone(post.cover)
        page = (self.root / 'blog' / self.folder.name / 'index.html').read_text()
        self.assertIn('property="og:image" content="https://myarc.dev/og-image.webp"', page)
        self.assertNotIn('<dialog', page)
        self.assertIn('class="post-thumb"', (self.root / 'blog/index.html').read_text())

    def test_thumbnail_reuses_screenshot_validation_and_site_budget(self):
        from unittest.mock import patch
        Image.new('RGBA', (80, 50)).save(self.folder / 'logo.png')
        (self.folder / 'fake.png').write_text('<svg/>')
        Image.new('RGB', (2, 2)).save(self.root / 'outside.png')
        (self.folder / 'alias.png').symlink_to(self.root / 'outside.png')
        for value in ('null', 'false', '[]', '""', 'https://example.test/image.png', '../outside.png', 'alias.png', 'fake.png', 'logo.svg'):
            with self.subTest(value=value):
                self.post()
                self.source.write_text(self.source.read_text().replace('draft: false', f'draft: false\nthumbnail: {value}'))
                with self.assertRaises((ValueError, OSError)):
                    load_post(self.source)
        self.post()
        self.source.write_text(self.source.read_text().replace('draft: false', 'draft: false\nthumbnail: logo.png'))
        with patch('blog.MAX_IMAGE_BYTES', 1), self.assertRaisesRegex(ValueError, '10 MiB'):
            load_post(self.source)
        with patch('blog.MAX_TOTAL_IMAGE_BYTES', 1), self.assertRaisesRegex(ValueError, 'site budget'):
            self.build()

    def test_responsive_webp_and_link_rewrite(self):
        Image.new('RGB', (2000, 1300), 'purple').save(self.folder / 'large.png')
        self.post('[![Reference](large.png)](large.png)')
        post = self.build()[0]
        self.assertEqual(2, len(post.images))
        self.assertTrue(all(name.endswith('.webp') for name in post.images))
        self.assertIn('960w', post.html)
        self.assertIn('width="2000"', post.html)
        self.assertNotIn('large.png', post.html)
        self.assertIn('loading="eager"', post.html)
        html = (self.root / 'blog' / self.folder.name / 'index.html').read_text()
        schema = json.loads(html.split('<script type="application/ld+json">', 1)[1].split('</script>', 1)[0])
        self.assertEqual('https://github.com/bitbonsai', schema['@graph'][0]['author']['url'])

    def test_reading_minutes_round_up_and_exclude_image_text(self):
        Image.new('RGB', (20, 10), 'blue').save(self.folder / 'screen.png')
        for words, minutes in ((0, 1), (1, 1), (200, 1), (201, 2), (400, 2), (401, 3)):
            with self.subTest(words=words):
                self.post(' '.join(['word'] * words) + '\n\n![' + 'alt ' * 220 + '](screen.png)\n\n*' + ' '.join(['caption'] * 220) + '*')
                self.assertEqual(minutes, load_post(self.source).reading_minutes)
        self.post('## Heading\n\n' + ' '.join(['word'] * 199))
        self.assertEqual(1, load_post(self.source).reading_minutes)
        self.post('## Two words\n\n' + ' '.join(['word'] * 199))
        self.assertEqual(2, load_post(self.source).reading_minutes)

    def test_author_row_metadata_and_shared_portrait(self):
        self.post(' '.join(['word'] * 201))
        other = self.root / 'posts/2026-10-24-other'
        other.mkdir()
        (other / 'index.md').write_text(self.source.read_text())
        posts = self.build()
        avatars = list((self.root / 'blog/_assets').glob('mauricio-wolff.*.webp'))
        self.assertEqual(1, len(avatars))
        with Image.open(avatars[0]) as image:
            self.assertEqual('WEBP', image.format)
            self.assertEqual((84, 84), image.size)
        author = {'@type': 'Person', 'name': 'Mauricio Wolff', 'url': 'https://github.com/bitbonsai'}
        for post in posts:
            page = self.root / 'blog' / post.slug / 'index.html'
            soup = BeautifulSoup(page.read_text(), 'html.parser')
            row = soup.select_one('.post-author')
            self.assertEqual('Mauricio Wolff', row.a.get_text())
            self.assertEqual(author['url'], row.a['href'])
            self.assertEqual(['author'], row.a['rel'])
            self.assertEqual('2 min read', row.select_one('.post-read-time').text)
            self.assertEqual('', row.img['alt'])
            self.assertEqual(('28', '28'), (row.img['width'], row.img['height']))
            self.assertEqual(avatars[0].resolve(), (page.parent / row.img['src']).resolve())
            self.assertEqual('time', row.find_previous_sibling().name)
            self.assertFalse(post.images)
            self.assertIsNone(post.cover)
            self.assertFalse(soup.select('.post-body img'))
            self.assertFalse(soup.select('dialog, link[rel="preload"][as="image"]'))
            self.assertEqual(author['name'], soup.select_one('meta[name="author"]')['content'])
            schema = json.loads(soup.select_one('script[type="application/ld+json"]').string)['@graph'][0]
            self.assertEqual(author, schema['author'])
            self.assertEqual(author, schema['publisher'])
        index = BeautifulSoup((self.root / 'blog/index.html').read_text(), 'html.parser')
        schema = json.loads(index.select_one('script[type="application/ld+json"]').string)['@graph'][0]
        self.assertEqual(author, schema['publisher'])
        self.assertTrue(all(entry['author'] == author for entry in schema['blogPost']))
        self.assertFalse(index.select('.post-thumb'))

    def test_header_menu_links_home_from_blog_pages(self):
        self.post()
        self.build()
        for page in (self.root / 'blog/index.html', self.root / 'blog' / self.folder.name / 'index.html'):
            header = BeautifulSoup(page.read_text(), 'html.parser').select_one('header.top')
            toggle = header.select_one('button.menu-toggle')
            self.assertEqual(('Menu', 'site-menu', 'site-menu'), (toggle['aria-label'], toggle['aria-controls'], toggle['popovertarget']))
            self.assertEqual(2, len(toggle.select('svg[aria-hidden="true"]')))
            menu = header.select_one('nav#site-menu[popover]')
            self.assertEqual(['/#principles', '/#building', '/blog/', 'https://github.com/bitbonsai/myarc'], [link['href'] for link in menu.select('a')])
            self.assertEqual('/', header.select_one('a.mark')['href'])
            quick = header.select_one('.quick')
            self.assertEqual(['/blog/', 'https://github.com/bitbonsai/myarc'], [link['href'] for link in quick.select('a')])
            self.assertEqual('Follow development', quick.select_one('a.quick-icon')['aria-label'])
            close = menu.select_one('button.menu-close')
            self.assertEqual(('site-menu', 'hide', 'Close menu'), (close['popovertarget'], close['popovertargetaction'], close['aria-label']))
            self.assertNotIn('home-mark', str(header))
        home = BeautifulSoup(TEMPLATE, 'html.parser').select_one('header.top')
        self.assertEqual(['mark', 'plain', 'home-mark'], home.select_one('a.mark')['class'])

    def test_posts_group_by_month_without_repeated_labels(self):
        self.post()
        for slug in ('2026-10-24-earlier', '2026-09-30-older'):
            folder = self.root / 'posts' / slug
            folder.mkdir()
            (folder / 'index.md').write_text(self.source.read_text())
        posts = self.build()
        index = (self.root / 'blog/index.html').read_text()
        self.assertEqual(2, index.count('class="post-month"'))
        self.assertEqual(1, index.count('>October 2026</p>'))
        self.assertEqual(1, index.count('>September 2026</p>'))
        self.assertEqual(['2026-10-25-test-post', '2026-10-24-earlier', '2026-09-30-older'], [post.slug for post in posts])

    def test_caption_is_explicit_and_belongs_to_its_figure(self):
        from bs4 import BeautifulSoup
        Image.new('RGB', (20, 10), 'blue').save(self.folder / 'screen.png')
        self.post('![Alt stays accessible](screen.png)\n\n## A section\n\n*Ordinary emphasis.*\n\n[![Reference](screen.png)](screen.png)\n\n*Reference caption with a [source](https://example.test/).*')
        soup = BeautifulSoup(load_post(self.source).html, 'html.parser')
        figures = soup.find_all('figure')
        self.assertEqual(2, len(figures))
        self.assertIsNone(figures[0].find('figcaption'))
        self.assertEqual('Alt stays accessible', figures[0].img['alt'])
        self.assertEqual('Ordinary emphasis.', soup.select_one('p > em').text)
        self.assertEqual('Reference caption with a source.', figures[1].figcaption.text)
        self.assertIsNone(figures[1].figcaption.find('em'))
        self.assertEqual('https://example.test/', figures[1].figcaption.a['href'])

    def test_adjacent_figures_form_gallery_with_link_fallbacks(self):
        from bs4 import BeautifulSoup
        Image.new('RGB', (20, 10), 'blue').save(self.folder / 'screen.png')
        self.post('![First](screen.png)\n\n*First caption.*\n\n![Second](screen.png)\n\n*Second caption.*\n\n## Separate section\n\n![Third](screen.png)\n\n*Third caption.*')
        post = self.build()[0]
        soup = BeautifulSoup(post.html, 'html.parser')
        self.assertEqual(1, len(soup.select('.image-grid')))
        self.assertEqual(2, len(soup.select('.image-grid > figure')))
        self.assertEqual(3, len(soup.select('figure > a > img')))
        for image in soup.find_all('img'):
            self.assertEqual(image['src'], image.parent['href'])
        page = BeautifulSoup((self.root / 'blog' / self.folder.name / 'index.html').read_text(), 'html.parser')
        self.assertIsNotNone(page.select_one('dialog#image-lightbox'))
        preloads = page.select('link[rel="preload"][as="image"]')
        self.assertEqual(1, len(preloads))
        self.assertEqual(soup.img['src'], preloads[0]['href'])
        self.assertEqual('low', preloads[0]['fetchpriority'])
        script = page.select_one('script[src]')['src']
        self.assertEqual((Path(__file__).parent / 'gallery.js').read_bytes(), (self.root / 'blog' / self.folder.name / script).read_bytes())
        self.assertIsNone(BeautifulSoup((self.root / 'blog/index.html').read_text(), 'html.parser').select_one('script[src]'))

    def test_standalone_portrait_keeps_intrinsic_ratio(self):
        from bs4 import BeautifulSoup
        Image.new('RGB', (100, 150), 'orange').save(self.folder / 'card.png')
        self.post('![Synthetic card](card.png)\n\n*Card caption.*\n\nOpening paragraph.\n\n## Next section')
        soup = BeautifulSoup(load_post(self.source).html, 'html.parser')
        self.assertEqual(['portrait'], soup.figure['class'])
        self.assertEqual('100', soup.img['width'])
        self.assertEqual('150', soup.img['height'])
        self.assertEqual('(max-width: 48rem) 6rem, 9rem', soup.img['sizes'])
        self.assertEqual(soup.img['src'], soup.figure.a['href'])
        self.assertIsNone(soup.select_one('.image-grid'))

    def test_raw_html_and_unsafe_links_are_not_executed(self):
        self.post('<script>alert(1)</script>\n\n[Bad](javascript:alert(1))')
        html = load_post(self.source).html
        self.assertNotIn('<script>', html)
        self.assertNotIn('href="javascript:', html)

    def test_rejects_remote_traversal_symlink_and_missing_alt(self):
        Image.new('RGB', (2, 2)).save(self.root / 'outside.png')
        (self.folder / 'alias.png').symlink_to(self.root / 'outside.png')
        for body in ('![Remote](https://example.test/image.png)', '![Outside](../../outside.png)', '![Alias](alias.png)', '![](alias.png)'):
            with self.subTest(body=body):
                self.post(body)
                with self.assertRaises(ValueError):
                    load_post(self.source)

    def test_rejects_svg_and_fake_image_extension(self):
        (self.folder / 'fake.png').write_text('<svg/>')
        for body in ('![Vector](image.svg)', '![Fake](fake.png)'):
            self.post(body)
            with self.assertRaises((ValueError, OSError)):
                load_post(self.source)

    def test_requires_metadata_and_single_page_heading(self):
        for body in ('# Extra title',):
            self.post(body)
            with self.assertRaises(ValueError):
                load_post(self.source)
        self.post(draft='"false"')
        with self.assertRaises(ValueError):
            load_post(self.source)
        self.source.write_text('---\ntitle: One\ntitle: Two\ndraft: false\n---\nBody')
        with self.assertRaises(ValueError):
            load_post(self.source)


class PublisherTests(unittest.TestCase):
    def test_only_artifacts_are_selected(self):
        from publish import artifacts, blob_sha
        self.assertEqual('e69de29bb2d1d6434b8b29ae775ad8c2e48c5391', blob_sha(b''))
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            for name in ('index.html', 'og-image.png', 'og-image.webp', 'favicon.png', 'favicon.ico', 'robots.txt', 'sitemap.xml', 'index.md', 'llms.txt', 'llms-full.txt'):
                (root / name).write_bytes(b'synthetic artifact')
            (root / 'blog').mkdir()
            (root / 'blog/index.html').write_text('synthetic index')
            (root / 'download').mkdir()
            for name in ('index.html', 'index.md'):
                (root / 'download' / name).write_text('synthetic download')
            (root / 'release.json').write_text('{}')
            (root / 'posts').mkdir()
            (root / 'posts/private.md').write_text('unapproved source')
            files = artifacts(root)
            self.assertIn('blog/index.html', files)
            self.assertIn('download/index.html', files)
            self.assertNotIn('release.json', files)
            self.assertFalse(any('posts/' in name for name in files))
            self.assertEqual(b'myarc.dev\n', files['CNAME'])
            (root / 'favicon.png').unlink()
            (root / 'favicon.png').symlink_to(root / 'posts/private.md')
            with self.assertRaises(ValueError):
                artifacts(root)


if __name__ == '__main__':
    unittest.main()
