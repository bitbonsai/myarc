# myarc.dev website and progress journal

Public website and Markdown progress journal for myArc. [Live site](https://myarc.dev/). Current visual rules and theme tokens are in [design.md](../design.md).

## Build

`index.template.html` supplies the shared layout and theme. `build.py` embeds Outfit, wordmarks, ribbon artwork, grain and license notices, then builds the homepage, blog index and approved posts. Install build dependencies in an isolated environment:

```sh
python3 -m venv .venv
. .venv/bin/activate
python3 -m pip install -r site/requirements.txt
python3 site/build.py
python3 -m unittest discover -s site -p '*_test.py'
python3 site/check.py
python3 site/check_browser.py
```

Build dependencies: Python 3.9+, Pillow, fontTools, Brotli, PyYAML and markdown-it-py. The Markdown parser runs only during builds, with raw HTML disabled. The published page has no executable JavaScript or runtime dependencies. Font timestamps are preserved for deterministic builds.

The browser check needs agent-browser and Chrome. It closes agent-browser and Chrome for Testing helpers, including lingering test helpers. These checks exercise the website, not native browser behavior.

## Progress posts and publishing

[Authoring instructions](posts/README.md) cover metadata, approval and screenshots. Put each Markdown post and its local images in `site/posts/YYYY-MM-DD-slug/`. Drafts are excluded by default. `site/blog/` is ignored generated output; the sitemap contains only published routes. Source and generated screenshots are separate so deleting a source reference removes the public artifact on the next deployment.

With the environment above activated, publish with `python3 site/publish.py`. The publisher rebuilds and checks the homepage, uses the existing `bitbonsai` GitHub CLI login, and updates the artifact-only branch without force. It preserves the domain/HTTPS configuration and unrelated root files, while replacing the generated blog subtree so unpublishing removes article pages and screenshots. Credentials are never written to the website or receipt. Run the unit/browser checks before publishing.

## Design

Fable/high produced the layout using `/frontend-design`. Section headings sit above their content. Body text is 1.125rem, section headings share one scale, and subheadings/status labels are 1.5rem. The progress panel gives its smaller artwork a separate grid area.

Header/footer ribbon marks are 1rem square beside outlined Outfit wordmarks. Header links are ink without underlines at rest; document links remain underlined. Both development CTAs include the user-supplied GitHub SVG mark, with fill inherited from the button text color.

The page follows `prefers-color-scheme` automatically. CSS variables switch surfaces/text and native picture sources select ivory wordmarks in dark mode. Blue primary buttons and colorful artwork remain unchanged. Original grain covers page and panel in both themes. Reduced motion disables animation, transitions and smooth scrolling.

## Verification

Offline font/image decoding, no remote page-resource requests, minimum text size, clipping and overflow checks passed at 320/390/768/900/1024/1440. Dark rendering, wordmark switching and sampled AA text contrast passed at 1440/390. Keyboard focus, link hover and the 40-BPM footer heart remain checked. Fresh light/dark desktop and dark mobile screenshots were inspected.

All six documentation URLs returned anonymous HTTP 200; an actual Roadmap click also opened the expected GitHub document. Safari, Firefox and screen readers remain unmeasured.

## Hosting and discovery

GitHub Pages serves the root of the artifact-only `gh-pages` branch. Preserve `CNAME` and `.nojekyll` when publishing. HTTPS is enforced on `myarc.dev`; `www` redirects to the apex. Cloudflare has four GitHub Pages apex A records and a `www` CNAME, DNS-only with TTL 300.

The approved 1200 × 630 `og-image.png`, public favicon files, `robots.txt` and `sitemap.xml` support crawlers/social previews. Normal page visuals stay embedded and independently usable offline. Open Graph, Twitter large-image metadata, canonical URL and WebSite JSON-LD describe the unreleased project without claiming a shipped application. Existing social-preview caches may need refreshing.

Full Outfit OFL and Lucide ISC notices are retained in the inert `asset-notices` JSON block. Supplied-logo rights and trademark clearance remain unverified; see [attribution](../ATTRIBUTION.md).
