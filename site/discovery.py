"""Export the same visible content for readers and machine clients."""
from urllib.parse import urljoin

from bs4 import BeautifulSoup
from markdownify import markdownify


def markdown_page(html, url):
    soup = BeautifulSoup(html, 'html.parser')
    main = soup.find('main')
    if main is None:
        raise ValueError(f'Missing main content: {url}')
    for element in main.select('[aria-hidden="true"], .mark'):
        element.decompose()
    for image in list(main.find_all('img')):
        if not image.get('alt') or image.get('src', '').startswith('data:'):
            image.decompose()
        else:
            image['src'] = urljoin(url, image['src'])
    for link in main.find_all('a', href=True):
        link['href'] = urljoin(url, link['href'])
    for br in main.select('h1 br'):
        br.replace_with(' ')
    for element in main.find_all('dt'):
        element.name = 'h3'
    for element in main.find_all('dd'):
        element.name = 'p'
    content = markdownify(str(main), heading_style='ATX', bullets='-', escape_misc=True).strip()
    return f'HTML: {url}\n\n{content}\n'


def build_discovery(root, homepage, posts):
    documents = [('https://myarc.dev/', root / 'index.md', homepage)]
    documents.append(('https://myarc.dev/blog/', root / 'blog/index.md', (root / 'blog/index.html').read_text()))
    if (root / 'download/index.html').is_file():
        documents.append(('https://myarc.dev/download/', root / 'download/index.md', (root / 'download/index.html').read_text()))
    for post in posts:
        folder = root / 'blog' / post.slug
        documents.append((post.url, folder / 'index.md', (folder / 'index.html').read_text()))
    full = []
    for url, target, html in documents:
        markdown = markdown_page(html, url)
        target.write_text(markdown)
        full.append(markdown)
    index = '# myArc\n\n> A pre-release open-source browser project inspired by Arc, built on Chromium with a Rust-owned workspace core.\n\n'
    index += 'A complete myArc browser and importer remain unimplemented. Native integration is experimental. Arc reference screenshots illustrate the target appearance. Local ownership and optional AI are product requirements, not evidence of a finished release.\n\n'
    index += '## Project\n\n- [Overview](https://myarc.dev/index.md): Markdown equivalent of the homepage.\n- [Progress journal](https://myarc.dev/blog/index.md): Published development updates.\n- [Download](https://myarc.dev/download/index.md): Pre-alpha nightly status, requirements and known limitations.\n- [Repository](https://github.com/bitbonsai/myarc): Public source and plans.\n\n## Posts\n\n'
    for post in posts:
        index += f'- [{post.title}]({post.url}index.md): {post.summary}\n'
    index += '\n## Optional\n\n- [Complete published text](https://myarc.dev/llms-full.txt): Homepage, journal index and approved posts, generated from their HTML.\n- [Sitemap](https://myarc.dev/sitemap.xml): Canonical HTML routes.\n'
    (root / 'llms.txt').write_text(index)
    (root / 'llms-full.txt').write_text('# myArc published content\n\n' + '\n\n---\n\n'.join(full))
