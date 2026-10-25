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
python3 site/check_seo.py
python3 site/check_browser.py
```

Build dependencies: Python 3.9+, Pillow, fontTools, Brotli, PyYAML and markdown-it-py. The Markdown parser runs only during builds, with raw HTML disabled. Markdownify/Beautiful Soup export the same rendered main content to Markdown, omitting decorative branding. The website has no executable JavaScript or runtime service dependencies. The homepage remains a standalone HTML file; blog pages use content-hashed first-party assets in their generated `_assets/` folder. Font timestamps are preserved for deterministic builds.

The browser check needs agent-browser and Chrome. It closes agent-browser and Chrome for Testing helpers, including lingering test helpers. These checks exercise the website, not native browser behavior.

## Progress posts and publishing

[Authoring instructions](posts/README.md) cover metadata, approval and screenshots. Put each Markdown post and its local images in `site/posts/YYYY-MM-DD-slug/`. Drafts are excluded by default. `site/blog/` is ignored generated output; the sitemap contains only published routes. Source and generated screenshots are separate so deleting a source reference removes the public artifact on the next deployment.

With the environment above activated, publish with `python3 site/publish.py`. The publisher rebuilds and checks the homepage, uses the existing `bitbonsai` GitHub CLI login, and updates the artifact-only branch without force. It preserves the domain/HTTPS configuration and unrelated root files, while replacing the generated blog subtree so unpublishing removes article pages and screenshots. Credentials are never written to the website or receipt. Run the unit/browser checks before publishing.

## Delivery and machine-readable content

Published post images are high-quality WebP derivatives, capped at 2400px, with a 960px responsive variant when useful. HTML dimensions reserve space; first images load eagerly and later images lazily. Original PNG/JPEG sources remain in `site/posts/` and are excluded from deployment. Every article image requires an explicit caption; publication rejects missing or empty captions. SVG wordmarks and compatible PNG/ICO favicons retain their native formats. Blog font, grain and small ribbon assets are shared and content-hashed for caching.

GitHub Pages negotiates gzip automatically for text responses. Live `Content-Encoding: gzip` and byte-for-byte decompressed read-back were verified; tiny responses may stay uncompressed. Uploading `.gz` sidecars does not configure content negotiation on Pages. Custom cache/security headers and `Accept: text/markdown` negotiation are outside this static host configuration.

Every HTML page links its Markdown equivalent with `rel="alternate"`. Outputs: `/index.md`, `/blog/index.md`, and `/blog/YYYY-MM-DD-slug/index.md`. `/llms.txt` provides project status and discovery links; `/llms-full.txt` contains the generated published text. Drafts and unpublished posts are excluded from both. HTML canonical URLs stay in the sitemap, with post image entries. Source dates are preserved without invented modification timestamps.

Blog metadata includes author attribution, canonical/social fields, language, Blog/BlogPosting and BreadcrumbList JSON-LD. `check_seo.py` checks metadata and local targets before publication. This improves delivery and machine readability; indexing, rankings and AI citations remain unmeasured. Search Console submission and external rich-result validation have not been performed.

## Design

Fable/high produced the layout using `/frontend-design`. Section headings sit above their content. Body text is 1.125rem, section headings share one scale, and subheadings/status labels are 1.5rem. The progress panel gives its smaller artwork a separate grid area.

Blog index and article content use a centered 68ch reading column. Article title, metadata and body share its left edge; text stays left-aligned. Header/footer retain the wider site layout, and phone gutters stay 1.5rem.

Header/footer ribbon marks are 1rem square beside outlined Outfit wordmarks. Header links are ink without underlines at rest; document links remain underlined. Both development CTAs include the user-supplied GitHub SVG mark, with fill inherited from the button text color.

The page follows `prefers-color-scheme` automatically. CSS variables switch surfaces/text and native picture sources select ivory wordmarks in dark mode. Blue primary buttons and colorful artwork remain unchanged. Original grain covers page and panel in both themes. Reduced motion disables animation, transitions and smooth scrolling.

Same-origin page navigation uses native cross-document View Transitions with a one-second blur/fade. The root snapshots animate between clear and 2rem blur while the header stays separately captured. Shared CSS opts the homepage, index and posts in. Reduced motion opts out entirely. Unsupported browsers retain ordinary navigation; there is no router or runtime JavaScript.

## Verification

Offline font/image decoding, no remote page-resource requests, minimum text size, clipping and overflow checks passed at 320/390/768/900/1024/1440. Dark rendering, wordmark switching and sampled AA text contrast passed at 1440/390. Keyboard focus, link hover and the 40-BPM footer heart remain checked. Fresh light/dark desktop and dark mobile screenshots were inspected.

The browser check also serves the generated site over loopback HTTP and follows home → blog → post → blog → home. Chrome `pageswap` evidence confirms transitions in desktop/light and phone/dark modes, and their absence under reduced motion. A fresh navigation recording was inspected. Other-browser transition support remains unmeasured.

All six documentation URLs returned anonymous HTTP 200; an actual Roadmap click also opened the expected GitHub document. Safari, Firefox and screen readers remain unmeasured.

## Hosting and discovery

GitHub Pages serves the root of the artifact-only `gh-pages` branch. Preserve `CNAME` and `.nojekyll` when publishing. HTTPS is enforced on `myarc.dev`; `www` redirects to the apex. Cloudflare has four GitHub Pages apex A records and a `www` CNAME, DNS-only with TTL 300.

The approved 1200 × 630 `og-image.png`, public favicon files, `robots.txt` and `sitemap.xml` support crawlers/social previews. Normal page visuals stay embedded and independently usable offline. Open Graph, Twitter large-image metadata, canonical URL and WebSite JSON-LD describe the unreleased project without claiming a shipped application. Existing social-preview caches may need refreshing.

Full Outfit OFL and Lucide ISC notices are retained in the inert `asset-notices` JSON block. Supplied-logo rights and trademark clearance remain unverified; see [attribution](../ATTRIBUTION.md).
