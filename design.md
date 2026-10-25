# myArc branding system

Current website implementation: [site/index.template.html](site/index.template.html). Asset provenance and licenses: [ATTRIBUTION.md](ATTRIBUTION.md). Original artwork and export tooling: [design/brand/README.md](design/brand/README.md).

## Identity

Use **myArc**, with that capitalization. Primary headline: **myArc, forever yours.**

The colorful ribbon mark accompanies the outlined wordmark. The custom barless A belongs only to the wordmark; headings and body copy use Outfit's normal characters.

Header and footer ribbon marks are **1rem square**, vertically centered beside the wordmark. Header wordmark height is 1.75rem, reduced to 1.375rem below 40rem viewport width. Footer wordmark height is 1.5rem. Keep .5rem between mark and wordmark, or .375rem below 40rem. Preserve each image's proportions and transparent padding.

Use ink wordmarks on light surfaces and ivory wordmarks on dark surfaces. Ribbon colors stay unchanged in both themes. Decorative artwork gets an empty alt; the containing home link and wordmark supply the accessible name.

## Color

Brand palette:

- Ivory: `#F5F2EC`
- Ink: `#182432`
- Midnight: `#101C2B`
- Primary blue: `#034BF4`
- Primary hover: `#033DD0`
- Muted heart: `#B86B73`

Light theme:

- Page: ivory
- Main text: ink
- Secondary text: `#54616D`
- Progress panel: midnight, with ivory text
- Text links and focus rings: primary blue

Dark theme:

- Page: `#0B1420`
- Main text: ivory
- Secondary text: `#B1BCC9`
- Progress panel: ink, with ivory text
- Text links and focus rings: `#8FB1FF`, lightened for contrast on navy

Primary buttons keep the same blue fill and white text in both themes. Development CTAs place the user-supplied GitHub SVG mark at 1.125rem before their label, with a .625rem gap; hide this decorative SVG from assistive technology. Text links start at 90% opacity and become opaque on hover or keyboard focus. Header navigation uses the theme's main text color without underlines, becoming the theme's link color on hover/focus.

Semantic CSS variables are `--page`, `--text`, `--quiet`, `--panel`, `--link-rgb` and `--link-solid`. Keep the named brand colors available separately.

## System preference

`prefers-color-scheme` selects light or dark mode automatically and responds when the system preference changes. CSS variables control surfaces and text. Native `<picture>` sources select the matching wordmark. Theme-aware `theme-color` metadata colors browser chrome.

The page uses HTML/CSS for this behavior. There is no theme toggle, stored override, account setting or JavaScript theme detector.

## Typography

Use the embedded variable **Outfit** font. All font sizes use rem; the root remains 100% so browser text-size preferences work.

- Body: 1.125rem, line-height 1.55
- Main headline: 5.5rem, stepping down to 4.5rem / 3.75rem / 2.75rem
- Section headings: 2.75rem, reduced to 2.25rem below 48rem
- Subheadings and Now / Next / For launch: 1.5rem, semibold
- Hero introduction: 1.375rem, reduced to 1.125rem on small screens
- Progress introduction: 1.25rem, reduced to 1.125rem below 48rem
- Footer: 1rem

Section headings share a size at each breakpoint and sit above their content. Avoid tiny uppercase sidebar labels and empty heading columns. Keep paragraphs at readable measures rather than stretching every line across the page.

## Layout and surfaces

The content wrapper is at most 80rem wide. Horizontal padding is 3rem, then 1.5rem below 48rem and 1.25rem below 40rem.

Sections use 6rem vertical padding, reduced to 4.5rem below 48rem. Space separates sections; horizontal divider rules are unnecessary.

Principles and document lists may use useful content columns. Headings stay above their grids. The progress panel gives artwork its own grid area, with three status groups on wide screens and a vertical list below 64rem. Avoid large spacer hacks and artwork overlapping text.

Reuse the original monochrome grain at a 16rem tile size on page and progress panel. Keep it static. Rounded panels use a 1.75rem radius, reduced to 1.25rem on mobile. Primary buttons are pills.

## Progress journal

Blog index and articles reuse the homepage shell, embedded Outfit and theme tokens. Titles use 3.5rem, reduced to 2.5rem below 48rem; article sections and preview titles use 2rem. Body text stays 1.125rem, dates and code use 1rem, and reading columns are capped at 68ch. Code keeps Outfit. Screenshots retain their original pixels and use a 1rem CSS radius.

Keep the Blog navigation link visible on mobile. The header development button uses 1rem text below 48rem so both links fit where possible; the header wraps at narrower widths. Body links, visible focus and reduced-motion behavior remain shared.

## Interaction and accessibility

Body links retain underlines. Their underline offset moves from .125rem to .3125rem over .2s ease-in-out. Keyboard focus has a visible .1875rem outline with an offset.

Intro motion is finite and ends within five seconds. Footer hover/focus produces one heart pulse per 1.5 seconds, 40 BPM. `prefers-reduced-motion` disables animation, transitions and smooth scrolling in both themes.

Verify light and dark rendering, image decoding, text contrast, keyboard focus and mobile overflow. Theme-specific wordmarks must change with the system preference while ribbon artwork stays unchanged.

## Publishing and social previews

Install [build dependencies](site/requirements.txt), then build with `python3 site/build.py`. Homepage and blog shells embed their visual assets; post screenshots are local static files. Authoring and publishing instructions are in the [website README](site/README.md). Font and visual page assets remain embedded. Public favicon files and the 1200 × 630 `og-image.png` are separate crawler/social-preview assets.

Open Graph and Twitter previews use the approved navy card consistently, regardless of the visitor's color preference. Canonical URL: `https://myarc.dev/`. GitHub Pages hosts the artifact branch; Cloudflare provides DNS. Hosting is separate from local browser operation.

Branding describes an open-source browser project. Keep release requirements distinct from working product behavior. Logo rights and trademark clearance remain unverified as recorded in the attribution document.
