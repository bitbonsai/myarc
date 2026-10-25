# Writing progress posts

Each post has its own folder:

```text
site/posts/2026-10-25-post-slug/
  index.md
  screenshot.png
```

Start `index.md` with YAML front matter:

```markdown
---
title: "Post title"
summary: "A short description for the index and social previews."
draft: true
---

Write the post in Markdown.

## Section heading

![Describe what the screenshot demonstrates][screen]

[screen]: screenshot.png
```

The folder supplies the date and URL: `/blog/2026-10-25-post-slug/`. Use lowercase hyphenated slugs. The title comes from metadata, so body sections start at `##`.

## Approval and privacy

Posts are excluded unless `draft: false` is explicitly set. Set that only after the content and screenshots are approved for publication. Draft status excludes a post from the website; it does not make files committed to a public repository private. Keep private notes outside this repository.

Public screenshots must use synthetic tabs, invented data and disposable profiles. Check browser chrome, tab titles and account names before publication. Real browser profiles, cookies, export files and personal browsing captures do not belong in posts.

## Screenshots and Markdown

PNG, JPEG and static WebP files may sit alongside a post or in its subfolders. Give every screenshot descriptive alt text. The first Markdown image supplies the article Open Graph/Twitter image and alt text, with its actual dimensions and MIME type. A small generated WebP thumbnail appears beside the title on the blog index. Posts without images keep the default brand social card and a text-only listing. Only images referenced by published posts are copied to the public output; draft and unreferenced files stay out of the artifact.

Raw HTML is displayed as text, not executed. External screenshot URLs, paths outside the post folder and symlinked post sources are rejected. Limits: 128 KiB Markdown per post, 4 KiB metadata, 20 screenshots per post, 10 MiB/24 megapixels per screenshot and 256 MiB screenshots across the site.

Build with the dependencies in [requirements.txt](../requirements.txt), then run:

```sh
python3 site/build.py
python3 -m unittest discover -s site -p '*_test.py'
python3 site/check.py
python3 site/check_browser.py
python3 site/publish.py
```

The generated `site/blog/` folder is replaced during builds. Edit `site/posts/`, never generated pages. Removing a post or changing it back to a draft removes it from the next deployment. The homepage, blog and posts share branding, system light/dark preference and accessible focus behavior.
