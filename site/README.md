# myarc.dev landing page

Public landing page for myArc. [Live site](https://myarc.dev/). Current visual rules and theme tokens are in [design.md](../design.md).

## Build

`index.template.html` is the source. `build.py` embeds Outfit, wordmarks, ribbon artwork, grain and license notices into `index.html`. Edit the template and rebuild:

```sh
python3 site/build.py
python3 site/check.py
python3 site/check_browser.py
```

Build dependencies: Python 3, Pillow, fontTools and Brotli. The published page has no executable JavaScript or runtime dependencies. Font timestamps are preserved for deterministic builds.

The browser check needs agent-browser and Chrome. It closes agent-browser and Chrome for Testing helpers, including lingering test helpers. These checks exercise the website, not native browser behavior.

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
