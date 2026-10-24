# myarc.dev landing page

Public landing page for myArc. One HTML file, inline CSS, no JavaScript, no network requests beyond outbound links. Content and branding follow the repository README and `design/brand/README.md`.

## Files

- `index.template.html` is the source. It holds the full page with build-time tokens where assets go.
- `build.py` replaces the tokens and writes `index.html`, a single self-contained file for any static host.
- `index.html` is generated. Edit the template, not the output.

## Asset tokens

| Token | Replaced with | Used as |
| --- | --- | --- |
| `@@OUTFIT_FONT@@` | variable Outfit WOFF2 data URI | `@font-face` src, weight 100 to 900 |
| `@@WORDMARK_INK@@` | dark wordmark SVG data URI | header and footer `img src` |
| `@@WORDMARK_IVORY@@` | light wordmark SVG data URI | sign-off inside the midnight panel |
| `@@LOGO_MATTE@@` | matte ribbon WebP data URI | hero art on ivory |
| `@@LOGO_GLASS@@` | glass ribbon WebP data URI | decorative art in the midnight panel |
| `@@GRAIN@@` | monochrome grain PNG data URI | body background tile at 16rem |
| `@@FAVICON@@` | matte mark PNG data URI | `link rel="icon"` |
| `@@HEART@@` | inline Lucide heart SVG with `class="heart"` | footer sentence |

Wordmark images carry the SVG's intrinsic 3056 by 1049 dimensions so the browser reserves space before the data URI decodes. Logo HTML attributes preserve their square aspect ratio; the build resamples the approved originals to WebP at up to 960 pixels.

## Page structure

1. Header. Ink wordmark, two on-page links, primary button to GitHub.
2. Hero. "myArc, forever yours." with the approved lede, primary button and a text link to the README. The matte ribbon sits beside the headline and shifts a little past its column edge.
3. Why. The approved ethos as a large statement, plus one paragraph naming the Arc workflows the project carries forward.
4. Principles. The four approved principles in a two-by-two grid.
5. Progress. A midnight panel with the planned architecture and a three-row status list: now, next, for launch. The glass ribbon bleeds out of the bottom-right corner.
6. Read along. Six links into the repository docs.
7. Footer. Wordmark, the "Made with love" line with the pulsing heart.

## Design decisions

- **Direction.** Quiet editorial page on warm ivory with grain. Ink type carries the page, the ribbon logos carry the colour, blue stays on buttons and links. One dark moment, the progress panel, so the glass ribbon gets a background where its highlights read.
- **Layout.** Sections after the hero share a 4/8 grid: a small uppercase label on the left, content on the right. Spacing separates sections instead of rules.
- **Type.** Outfit only. Headline 5.5rem on desktop, 4.5rem under 64rem, 3.75rem under 48rem, 2.75rem under 40rem. Body 1rem, labels and footer .875rem, nothing smaller. All sizes and breakpoints in rem.
- **Motion.** Hero copy rises in three staggered steps on load. The hero ribbon rises, then makes one 3.5-second drift and settles. All intro movement ends within five seconds. The footer heart pulses once per 1.5 seconds on hover or focus-within. `prefers-reduced-motion` removes every animation, link transitions and smooth scrolling.
- **Links.** Brand blue at 90 percent alpha, opaque on hover and focus, underline offset moves from .125rem to .3125rem over .2s ease-in-out. Focus-visible draws a .1875rem blue outline. Header text links use the same blue treatment; wordmark links remain artwork.
- **Honesty.** No download, release date, counts or screenshots. Development stage lives in the progress panel copy and the GitHub call to action. Launch requirements (CLI and MCP, blocking by default, Boosts) sit under "For launch", not as shipped features.
- **No JavaScript.** Nothing on the page needs it. Skip link, anchors and hover and focus states are CSS and HTML.

## Build and checks

Build dependencies: Python 3 with Pillow, fontTools and Brotli for WOFF2 compression. These run locally; the published page needs none of them. Font timestamps are preserved; two consecutive builds produced byte-identical HTML.

```sh
python3 site/build.py
python3 site/check.py
python3 site/check_browser.py
```

The browser check needs separately installed agent-browser and Chrome. It closes agent-browser and Chrome for Testing helpers, including lingering helpers from other test runs.

The build retains full Outfit OFL and Lucide ISC notices inside an inert `application/json` block named `asset-notices`. It includes no executable JavaScript.

## Verification

- Widths 320, 390, 768 and 1440 passed: no horizontal scroll or clipped text; actual body/minimum font sizes were 16/14 pixels. Body has `overflow-x: clip` as a guard; the hero shift is 6 percent of a 34rem box, inside the 3rem page padding.
- Under 64rem the progress panel adds bottom padding so the glass ribbon sits below the text rather than over it.
- Tab through: skip link, wordmark, two nav links, button, hero button, README link, six doc links, footer wordmark, `@bitbonsai`. Every stop shows the blue outline.
- Reduced motion: no heartbeat, no rise, no drift, no link transition.

## Hosting

Published at [bitbonsai.github.io/myarc](https://bitbonsai.github.io/myarc/) using GitHub Pages, HTTPS enforced. Source is the root of `gh-pages`, an artifact-only branch containing `index.html` and `.nojekyll`. Deployment commit: `1f9f608cbdb97f16acf3f67604893a562a003304`, authored and committed by `bitbonsai`. A public HTTP 200 response matched the generated file byte-for-byte.

Rebuild from the template before replacing the branch-root HTML. Preserve `.nojekyll` and any subsequently configured `CNAME` file. Main-checkout browser work is separate from this deployment branch.

Custom-domain binding and Cloudflare DNS for `myarc.dev` remain pending account access. Domain HTTPS must be verified after DNS is connected; `.dev` requires HTTPS. Existing DNS records were not changed.

## Limitations

- Doc links point at `blob/main` paths on GitHub. If the default branch or file names change, update the six links.
- Chromium offline rendering, image/font decoding, link hover, actual keyboard focus and reduced motion passed. Fresh desktop/mobile screenshots were inspected. Safari, Firefox and screen-reader behavior remain unmeasured.
- Logo author and license remain unsettled per the brand README, so the footer does not state them.
