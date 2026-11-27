"""Build the Design system docs page at /docs/design-system/."""
from html import escape
from pathlib import Path
import re

from docs import Doc, heading_id

BRAND_ASSETS = Path(__file__).resolve().parent.parent / 'design' / 'brand' / 'assets'
REPO = 'https://github.com/bitbonsai/myarc'

TITLE = 'Design system'
SUMMARY = 'Brand identity, color tokens, typography, layout, components, native glass surfaces and the product decisions behind them.'

PRODUCT_DECISIONS = [
    ('Yours to keep', 'Workspace state lives in local JSON files with no mandatory account or behavioral tracking. Interfaces stay free of sign-in walls, upsell banners and telemetry prompts.'),
    ('Familiar from day one', 'Carry forward Arc\'s sidebar, Spaces, Profiles, pinned tabs, Favorites, command bar, Peek, Split View and Little Arc. Spaces group tabs by topic; Profiles isolate cookies, logins, history and extensions, and several Spaces can share one Profile.'),
    ('Restrained site, personal browser', 'The website uses a quiet editorial grid, clear typography and small blue accents. The browser keeps that clarity while letting Spaces carry warmer themes, custom icons and tactile motion.'),
    ('AI by choice', 'Built-in AI is off by default and never required to browse. External agent control over CLI and MCP is a separate opt-in capability scoped to an Agent Space with a visible Stop control.'),
    ('Motion with meaning', 'Animated onboarding is a release requirement. Motion clarifies spatial transitions and honors prefers-reduced-motion and OS reduce-transparency settings from the first frame.'),
    ('Built in the open', 'Code is GPL-3.0 and plans stay public. Every surface separates working behavior from planned work and documents third-party attribution.'),
]

BRAND_PALETTE = [
    ('Ivory', '#F5F2EC', '#182432', 'Light page surface and dark-mode headings'),
    ('Ink', '#182432', '#F5F2EC', 'Light-mode text and dark-mode panel fill'),
    ('Midnight', '#101C2B', '#F5F2EC', 'Light-mode code panel and dark brand stage'),
    ('Sky', '#10AEFF', '#102238', 'Ribbon highlight and cool accent'),
    ('Blue', '#034BF4', '#FFFFFF', 'Primary action fill and light-mode link'),
    ('Violet', '#4627EE', '#FFFFFF', 'Ribbon midtone and Space accent'),
    ('Orchid', '#A553EC', '#101015', 'Ribbon gradient stop'),
    ('Pink', '#FF527E', '#14202D', 'Ribbon warm accent'),
    ('Coral', '#FFAF8E', '#14202D', 'Ribbon warm highlight'),
]

THEME_TOKENS = [
    ('--page', '#F5F2EC', '#0B1420', 'Page background behind the grain tile'),
    ('--text', '#182432', '#D2D9E1', 'Primary body copy and header links'),
    ('--quiet', '#54616D', '#B1BCC9', 'Secondary copy, captions and metadata'),
    ('--panel', '#101C2B', '#182432', 'Code blocks and dark container surfaces'),
    ('--link-solid', '#034BF4', '#8FB1FF', 'Hover links, focus rings and active nav'),
    ('--blue-deep', '#033DD0', '#033DD0', 'Primary button hover and focus fill'),
    ('--heart', '#B86B73', '#B86B73', 'Footer heart outline and 40 BPM pulse'),
]

TYPE_SCALE = [
    ('Hero display', '5.5rem', '600', '-.045em', '0.98', 'Steps to 4.5rem, 3.75rem and 2.5rem on smaller viewports'),
    ('Article / docs title', '3.5rem', '600', '-.045em', '1.10', 'Steps to 2.5rem below 48rem'),
    ('Section heading', '2.75rem', '600', '-.04em', '1.08', '2.25rem below 48rem; 2rem inside docs and blog bodies'),
    ('Subheading', '1.5rem', '600', '-.02em', '1.30', 'Feature titles, status labels and h3 sections'),
    ('Lede / intro', '1.25rem', '400', '0', '1.50', '1.375rem in homepage hero; 1.125rem on small screens'),
    ('Body', '1.125rem', '400', '0', '1.55', 'Default reading copy, capped at 68ch'),
    ('UI / code / footer', '1rem', '400–600', '0', '1.40', 'Nav links, tables, captions, kbd chips and inline code'),
]

NATIVE_GLASS_TOKENS = [
    ('kCornerRadius', '16 DIP', 'Outer corner radius on all frosted-glass dialogs and modal panels'),
    ('kButtonCornerRadius', '7 DIP', 'Corner radius for native dialog action buttons'),
    ('kPadding', '24 DIP', 'Standard interior dialog inset across body and actions'),
    ('kTitleTopPadding', '36 DIP', 'Top inset clearing the window close control above the title'),
    ('kButtonHeight / MinWidth', '36 / 84 DIP', 'Minimum hit target and width for dialog buttons'),
    ('kDarkGlassTint', '#18181D at 63%', 'Dark frosted tint with a 1 DIP specular rim at 20% alpha'),
    ('kLightGlassTint', '#FAFAFD at 83%', 'Light frosted tint with a 1 DIP specular rim at 14% alpha'),
    ('GlassMaterial presets', '4 levels', 'kWindow, kPopover, kHud and kSheet; opaque fallback under reduce-transparency or without blur'),
]

MOTION_TOKENS = [
    ('Micro feedback', '120–200 ms', 'Link underline shift (200ms), button scale 1.02 (180ms), glass dialogs (200ms)'),
    ('Surface transitions', '220–350 ms', 'Screenshot peek lift (300ms), lightbox scale 96% to 100% (300ms), onboarding moves'),
    ('Major reveals', '600–1000 ms', 'Hero rise (900ms), cross-document View Transitions (1s blur/fade), onboarding reveals'),
    ('Easing curve', 'cubic-bezier(.2, .7, .2, 1)', 'Shared deceleration token (--ease) for entrances and surface motion'),
    ('Heartbeat', '1.5 s (40 BPM)', 'Footer heart scales to 1.18 on hover or keyboard focus'),
]

CSS = '''
    .ds-section-intro { color: var(--quiet) }
    .ds-grid { display: grid; grid-template-columns: repeat(2, minmax(0, 1fr)); gap: 1rem; margin: 1.5rem 0 }
    .ds-card { padding: 1.25rem; border-radius: 1.125rem; border: .0625rem solid color-mix(in srgb, currentColor 14%, transparent); background: color-mix(in srgb, var(--page) 84%, transparent) }
    .ds-card h4 { margin: 0; font-size: 1.25rem; font-weight: 600; letter-spacing: -.02em; line-height: 1.3 }
    .ds-card p { margin-top: .5rem; color: var(--quiet); font-size: 1rem; line-height: 1.5 }

    .ds-marks { display: grid; grid-template-columns: repeat(2, minmax(0, 1fr)); gap: 1rem; margin: 1.5rem 0 }
    .post-body figure.ds-mark-card { margin: 0; border-radius: 1.125rem; border: .0625rem solid color-mix(in srgb, currentColor 14%, transparent); overflow: hidden; background: color-mix(in srgb, var(--page) 84%, transparent) }
    .ds-stage { display: grid; place-items: center; padding: 2rem 1.5rem; aspect-ratio: 16 / 10; background: #ebe7df; color: #182432 }
    .ds-stage.is-dark { background: #101c2b; color: #f5f2ec }
    .post-body figure.ds-mark-card img { width: 6.5rem; height: 6.5rem; margin: 0; border-radius: 0; opacity: 1 }
    .post-body figure.ds-mark-card figcaption { margin: 0; padding: .875rem 1.125rem; text-align: left; font-size: 1rem; color: var(--quiet) }

    .ds-lockups { display: grid; grid-template-columns: repeat(2, minmax(0, 1fr)); gap: 1rem; margin: 1.5rem 0 }
    .post-body figure.ds-lockup-card { margin: 0; border-radius: 1.125rem; border: .0625rem solid color-mix(in srgb, currentColor 14%, transparent); overflow: hidden }
    .ds-lockup-stage { display: flex; align-items: center; justify-content: center; gap: .625rem; padding: 2rem 1.25rem; background: #f5f2ec; color: #182432 }
    .ds-lockup-stage.is-dark { background: #0b1420; color: #f5f2ec }
    .post-body figure.ds-lockup-card img { width: 1.5rem; height: 1.5rem; margin: 0; border-radius: 0; opacity: 1; flex: none }
    .ds-wordmark { height: 2.25rem; width: auto; max-width: 100%; display: block; flex: none }
    .post-body figure.ds-lockup-card figcaption { margin: 0; padding: .875rem 1.125rem; text-align: left; font-size: 1rem; color: var(--quiet); background: color-mix(in srgb, var(--page) 84%, transparent) }

    .ds-palette { display: grid; grid-template-columns: repeat(3, minmax(0, 1fr)); gap: .75rem; margin: 1.5rem 0 }
    .ds-swatch { display: flex; flex-direction: column; justify-content: space-between; gap: 1.5rem; padding: 1rem; border-radius: .875rem; border: .0625rem solid color-mix(in srgb, currentColor 16%, transparent) }
    .ds-swatch strong { display: block; font-size: 1.125rem; font-weight: 600; line-height: 1.2 }
    .ds-swatch span { display: block; font-size: 1rem; font-variant-numeric: tabular-nums; opacity: .9 }

    .ds-tokens { display: grid; gap: .75rem; margin: 1.5rem 0 }
    .ds-token-row { display: grid; grid-template-columns: minmax(0, 1.1fr) minmax(0, 1fr) minmax(0, 1.4fr); gap: .5rem 1rem; align-items: baseline; padding: .875rem 1rem; border-radius: .875rem; border: .0625rem solid color-mix(in srgb, currentColor 12%, transparent); font-size: 1rem }
    .ds-token-vals { display: flex; flex-wrap: wrap; gap: .375rem .75rem; align-items: center }
    .ds-chip { display: inline-flex; align-items: center; gap: .375rem; font-variant-numeric: tabular-nums }
    .ds-dot { width: .875rem; height: .875rem; border-radius: 50%; border: .0625rem solid color-mix(in srgb, currentColor 24%, transparent); flex: none }
    .ds-token-role { color: var(--quiet) }

    .ds-type-list { display: grid; gap: 1rem; margin: 1.5rem 0 }
    .ds-type-row { padding: 1rem 1.25rem; border-radius: 1rem; border: .0625rem solid color-mix(in srgb, currentColor 12%, transparent) }
    .ds-type-sample { margin: 0; color: var(--text) }
    .ds-type-meta { margin-top: .5rem; color: var(--quiet); font-size: 1rem }

    .ds-glass-demo { position: relative; isolation: isolate; overflow: hidden; padding: 2rem 1.5rem; border-radius: 1.25rem; margin: 1.5rem 0; background: radial-gradient(45% 55% at 22% 28%, rgb(16 174 255 / .36), transparent 72%), radial-gradient(45% 55% at 78% 32%, rgb(70 39 238 / .34), transparent 72%), radial-gradient(45% 55% at 55% 82%, rgb(255 82 126 / .28), transparent 72%), var(--page); border: .0625rem solid color-mix(in srgb, currentColor 12%, transparent) }
    .ds-glass-panel { max-width: 28rem; margin: 0 auto; padding: 1.5rem; border-radius: 1rem; border: .0625rem solid rgb(255 255 255 / .55); background: rgb(250 250 253 / .78); -webkit-backdrop-filter: blur(1.25rem) saturate(1.5); backdrop-filter: blur(1.25rem) saturate(1.5); box-shadow: inset 0 .0625rem 0 rgb(255 255 255 / .5), 0 1rem 2.5rem rgb(16 28 43 / .18) }
    .ds-glass-dots { display: flex; gap: .375rem; margin-bottom: .75rem }
    .ds-glass-dot { width: .75rem; height: .75rem; border-radius: 50%; background: #ff5f57 }
    .ds-glass-panel h4 { margin: 0; font-size: 1.25rem; font-weight: 600; letter-spacing: -.02em; line-height: 1.3 }
    .ds-glass-panel p { margin-top: .5rem; color: var(--quiet); font-size: 1rem }
    .ds-glass-actions { display: flex; flex-wrap: wrap; gap: .625rem; margin-top: 1.125rem }
    .ds-native-btn { display: inline-flex; align-items: center; justify-content: center; min-width: 5.25rem; min-height: 2.25rem; padding: .375rem 1rem; border-radius: .4375rem; background: var(--blue); color: #fff; font-size: 1rem; font-weight: 600 }
    .ds-native-btn.is-secondary { background: color-mix(in srgb, currentColor 10%, transparent); color: var(--text) }

    .ds-components { display: grid; gap: 1rem; margin: 1.5rem 0 }
    .ds-specimen { padding: 1.25rem; border-radius: 1.125rem; border: .0625rem solid color-mix(in srgb, currentColor 14%, transparent) }
    .ds-specimen-label { margin: 0; color: var(--quiet); font-size: 1rem; font-weight: 600 }
    .ds-row { display: flex; flex-wrap: wrap; align-items: center; gap: .75rem 1rem; margin-top: 1rem }
    .ds-pill-demo { display: inline-flex; align-items: center; padding: .25rem .75rem; border-radius: 999rem; font-size: 1rem; font-weight: 600; line-height: 1.4 }
    .ds-pill-docs { box-shadow: inset 0 0 0 .0625rem rgb(var(--link-rgb) / .45); color: var(--link-solid); letter-spacing: .06em; text-transform: uppercase }
    .ds-pill-done { background: rgb(var(--link-rgb) / .14); color: var(--link-solid) }
    .ds-pill-partial { border: .0625rem dashed currentColor; color: var(--text) }
    .ds-pill-missing { border: .0625rem solid color-mix(in srgb, var(--quiet) 45%, transparent); color: var(--quiet) }
    .post-body .ds-kbd { padding: .125rem .5rem; border: .0625rem solid color-mix(in srgb, currentColor 18%, transparent); border-bottom-width: .125rem; border-radius: .375rem; background: color-mix(in srgb, currentColor 6%, transparent); font-family: inherit; font-size: 1rem; font-weight: 500; white-space: nowrap }

    @media (prefers-color-scheme: dark) {
      .ds-type-sample, .ds-card h4, .ds-glass-panel h4 { color: var(--ivory) }
      .ds-glass-panel { border-color: rgb(255 255 255 / .2); background: rgb(24 24 29 / .63); box-shadow: inset 0 .0625rem 0 rgb(255 255 255 / .12), 0 1.25rem 3rem rgb(0 0 0 / .55) }
    }

    @media (max-width: 48rem) {
      .ds-grid, .ds-marks, .ds-lockups { grid-template-columns: minmax(0, 1fr) }
      .ds-palette { grid-template-columns: repeat(2, minmax(0, 1fr)) }
      .ds-token-row { grid-template-columns: minmax(0, 1fr) }
    }

    @media (max-width: 40rem) {
      .ds-palette { grid-template-columns: minmax(0, 1fr) }
    }
'''


def _wordmark_svg():
    svg = (BRAND_ASSETS / 'wordmark-ink.svg').read_text().strip()
    svg = re.sub(r'<title>.*?</title>', '', svg, count=1)
    svg = re.sub(r'\sfill="[^"]*"', ' fill="currentColor"', svg, count=1)
    svg = re.sub(r'\scolor="[^"]*"', '', svg, count=1)
    return svg.replace('<svg ', '<svg class="ds-wordmark" role="img" aria-label="myArc wordmark" focusable="false" ', 1)


def _add_toc(html):
    toc = []
    seen = {'main', 'toc-title'}

    def replace(match):
        tag, body = match.group(1), match.group(2)
        text = re.sub(r'<[^>]+>', '', body).strip()
        anchor = heading_id(text, seen)
        toc.append((int(tag[1]), anchor, text))
        return f'<{tag} id="{anchor}">{body}</{tag}>'

    rendered = re.sub(r'<(h[23])>(.*?)</\1>', replace, html)
    return rendered, toc


def design_system_doc():
    wordmark = _wordmark_svg()

    decisions = ''.join(
        f'<div class="ds-card"><h4>{escape(title)}</h4><p>{escape(body)}</p></div>'
        for title, body in PRODUCT_DECISIONS
    )

    swatches = ''.join(
        f'<div class="ds-swatch" style="background:{hex_code};color:{fg}">'
        f'<strong>{escape(name)}</strong> '
        f'<span>{escape(hex_code)}</span>'
        '</div>'
        for name, hex_code, fg, _ in BRAND_PALETTE
    )

    theme_rows = ''.join(
        '<div class="ds-token-row">'
        f'<div><code>{escape(token)}</code></div>'
        '<div class="ds-token-vals">'
        f'<span class="ds-chip"><span class="ds-dot" style="background:{light}" aria-hidden="true"></span>{escape(light)}</span> '
        f'<span class="ds-chip"><span class="ds-dot" style="background:{dark}" aria-hidden="true"></span>{escape(dark)}</span>'
        '</div>'
        f'<div class="ds-token-role">{escape(role)}</div>'
        '</div>'
        for token, light, dark, role in THEME_TOKENS
    )

    type_rows = ''.join(
        '<div class="ds-type-row">'
        f'<p class="ds-type-sample" style="font-size:{sample_size};font-weight:{weight};letter-spacing:{tracking};line-height:{leading}">{escape(label)}</p>'
        f'<p class="ds-type-meta">{escape(size)} · weight {escape(weight)} · line-height {escape(leading)} · {escape(notes)}</p>'
        '</div>'
        for (label, size, weight, tracking, leading, notes), sample_size in zip(
            TYPE_SCALE,
            ('2.5rem', '2.25rem', '2rem', '1.5rem', '1.25rem', '1.125rem', '1rem'),
        )
    )

    native_rows = ''.join(
        '<div class="ds-token-row">'
        f'<div><code>{escape(token)}</code></div>'
        f'<div>{escape(value)}</div>'
        f'<div class="ds-token-role">{escape(role)}</div>'
        '</div>'
        for token, value, role in NATIVE_GLASS_TOKENS
    )

    motion_rows = ''.join(
        '<div class="ds-token-row">'
        f'<div><strong>{escape(name)}</strong></div>'
        f'<div><code>{escape(duration)}</code></div>'
        f'<div class="ds-token-role">{escape(detail)}</div>'
        '</div>'
        for name, duration, detail in MOTION_TOKENS
    )

    body = f'''
<p class="ds-section-intro">myArc shares one visual language across its website, user docs and native desktop browser shell. It ships on macOS today, with Linux and Windows ahead using the same tokens and OS-native surfaces.</p>

<h2>Product decisions</h2>
<p>Every visual choice traces back to a product constraint in <a href="{REPO}/blob/main/docs/PRODUCT.md">docs/PRODUCT.md</a> and <a href="{REPO}/blob/main/AGENTS.md">AGENTS.md</a>.</p>
<div class="ds-grid">{decisions}</div>

<h2>Brand identity</h2>
<p>Write the product name as <strong>myArc</strong> in display copy, headings and prose. Keep filenames, bundle identifiers, URLs and CLI commands lowercase (<code>myarc</code>). The primary tagline is <strong>myArc, forever yours.</strong></p>

<h3>Marks and lockups</h3>
<p>Two ribbon finishes serve different surfaces: the matte mark sits on light backgrounds and navigation bars, while the glass mark pairs with dark navy stages. Keep 12% clear space around the ribbon and preserve its proportions and colors.</p>
<div class="ds-marks">
  <figure class="ds-mark-card">
    <div class="ds-stage"><img src="@@LOGO_MATTE_LG@@" alt="myArc matte ribbon logo on warm ivory" width="256" height="256" decoding="async"></div>
    <figcaption>Matte ribbon mark for light surfaces and site chrome.</figcaption>
  </figure>
  <figure class="ds-mark-card">
    <div class="ds-stage is-dark"><img src="@@LOGO_GLASS_LG@@" alt="myArc glass ribbon logo on midnight navy" width="256" height="256" decoding="async"></div>
    <figcaption>Glass ribbon mark for dark surfaces and promotional stages.</figcaption>
  </figure>
</div>
<p>The horizontal lockup pairs a <code>1rem</code> ribbon mark with the outlined Outfit ExtraBold wordmark. The custom barless A belongs only to the wordmark; headings and body copy use Outfit's standard letters. Header wordmarks sit at <code>1.75rem</code> (<code>1.375rem</code> below <code>40rem</code>) with a <code>.5rem</code> gap (<code>.375rem</code> below <code>40rem</code>), and footer wordmarks sit at <code>1.5rem</code>.</p>
<div class="ds-lockups">
  <figure class="ds-lockup-card">
    <div class="ds-lockup-stage"><img src="@@LOGO_MATTE@@" alt="myArc matte ribbon beside ink wordmark" width="64" height="64" decoding="async">{wordmark}</div>
    <figcaption>Light lockup: ink wordmark (#182432) on ivory (#F5F2EC).</figcaption>
  </figure>
  <figure class="ds-lockup-card">
    <div class="ds-lockup-stage is-dark"><img src="@@LOGO_MATTE@@" alt="myArc matte ribbon beside ivory wordmark" width="64" height="64" decoding="async">{wordmark}</div>
    <figcaption>Dark lockup: ivory wordmark (#F5F2EC) on dark navy (#0B1420).</figcaption>
  </figure>
</div>

<h2>Color</h2>
<p>Warm ivory and deep navy anchor the reading surfaces. Brand blue drives interactive focus and primary actions, while the ribbon spectrum supplies secondary accents for Spaces and artwork.</p>

<h3>Brand palette</h3>
<div class="ds-palette">{swatches}</div>

<h3>Light and dark tokens</h3>
<p>The site follows <code>prefers-color-scheme</code> in HTML and CSS with no theme toggle, stored override or script detector. Semantic CSS variables switch surfaces and text while <code>&lt;picture&gt;</code> elements swap ink and ivory wordmarks. Primary buttons keep the same blue fill and white label in both themes.</p>
<div class="ds-tokens">{theme_rows}</div>

<h2>Typography</h2>
<p><strong>Outfit</strong> is the single embedded variable typeface across weights 100 to 900, falling back to Helvetica Neue and sans-serif. The root stays at <code>100%</code> so browser text-size preferences work, and every size is authored in <code>rem</code>.</p>

<h3>Type scale</h3>
<div class="ds-type-list">{type_rows}</div>

<h3>Reading rhythm</h3>
<ul>
  <li>Section headings sit above their content and share a single scale per breakpoint.</li>
  <li>Long-form prose in docs and blog posts caps at <code>68ch</code> so lines stay easy to scan.</li>
  <li>Inline code keeps Outfit inside a subtly bordered pill; fenced blocks sit on <code>--panel</code> with <code>--ivory</code> text.</li>
  <li>Space separates sections rather than horizontal divider rules.</li>
</ul>

<h2>Layout and surfaces</h2>
<p>The site wrapper caps at <code>80rem</code> with <code>3rem</code> desktop gutters and <code>1.5rem</code> mobile gutters at <code>48rem</code> and below. Sections use <code>6rem</code> vertical padding on desktop and <code>4.5rem</code> on mobile.</p>
<p>A deterministic monochrome grain tile repeats at <code>16rem</code> square behind page content. Light mode stacks two layers of the half-opacity WebP tile so texture reads on ivory; dark mode uses a single layer.</p>

<h3>Native browser glass</h3>
<p>Modal dialogs, About myArc, updater sheets and agent prompts share one centralized frosted-glass token set (currently implemented for macOS in <code>browser/foundation/glass_mac.h</code>, with the same geometry and opaque fallback contract planned for Linux and Windows). When OS reduce-transparency is on, or on systems without window blur, glass surfaces switch cleanly to opaque tinted fills.</p>
<div class="ds-glass-demo">
  <div class="ds-glass-panel">
    <div class="ds-glass-dots" aria-hidden="true"><span class="ds-glass-dot"></span></div>
    <h4>Frosted dialog surface</h4>
    <p>16 DIP outer corners, 1 DIP specular rim, 24 DIP interior padding and 7 DIP action buttons over a blurred backdrop.</p>
    <div class="ds-glass-actions">
      <span class="ds-native-btn">Allow</span>
      <span class="ds-native-btn is-secondary">Cancel</span>
    </div>
  </div>
</div>
<div class="ds-tokens">{native_rows}</div>

<h2>Components</h2>
<p>Interactive elements share the same focus ring (<code>.1875rem solid var(--link-solid)</code> with <code>.25rem</code> offset) and work without JavaScript.</p>
<div class="ds-components">
  <div class="ds-specimen">
    <p class="ds-specimen-label">Buttons and links</p>
    <div class="ds-row">
      <a class="button" href="/download/">Primary action</a>
      <a class="button ghost" href="/docs/">Secondary action</a>
      <a href="../install/">Moving-underline link</a>
    </div>
  </div>
  <div class="ds-specimen">
    <p class="ds-specimen-label">Pills, status badges and keys</p>
    <div class="ds-row">
      <span class="ds-pill-demo ds-pill-docs">Docs</span>
      <span class="ds-pill-demo ds-pill-done">Done</span>
      <span class="ds-pill-demo ds-pill-partial">Partial</span>
      <span class="ds-pill-demo ds-pill-missing">Missing</span>
      <kbd class="ds-kbd">⌘L</kbd>
      <kbd class="ds-kbd">⌘D</kbd>
      <kbd class="ds-kbd">⌃⇧=</kbd>
    </div>
  </div>
</div>

<h3>Iconography</h3>
<ul>
  <li><strong>Browser chrome and site controls</strong> use <strong>Lucide</strong> icons on a 24px grid at stroke width 2 (ISC license): sidebar actions, Peek controls, command bar search, Settings tabs, close buttons and the footer heart.</li>
  <li><strong>Space icons</strong> use 45 filled <strong>Ionicons</strong> glyphs (MIT license) alongside Unicode 15.1 emoji with English CLDR search annotations.</li>
  <li>Status indicators pair text labels with distinct border styles so meaning never depends on color alone.</li>
</ul>

<h2>Motion and accessibility</h2>
<p>Intro motion finishes within five seconds. Same-origin site navigation uses native cross-document View Transitions with a one-second blur and fade while keeping the header crisp. The homepage hero ribbon is drawn in WebGL, pauses off screen and falls back to the static image when motion is reduced.</p>
<div class="ds-tokens">{motion_rows}</div>
<ul>
  <li><code>prefers-reduced-motion: reduce</code> disables View Transitions, smooth scrolling, hover scaling, pulse animations and spatial movement while preserving full content and layout.</li>
  <li>Native frosted dialogs honor OS reduce-transparency settings and fall back to opaque tinted surfaces where window blur is unavailable.</li>
  <li>Text contrast meets WCAG AA in both light and dark themes, verified across desktop and mobile viewports.</li>
</ul>

<h2>Licensing and provenance</h2>
<ul>
  <li><strong>myArc code</strong> is licensed under <a href="{REPO}/blob/main/LICENSE">GPL-3.0</a>.</li>
  <li><strong>Outfit</strong> is licensed under SIL Open Font License 1.1. <strong>Lucide</strong> uses the ISC license, and <strong>Ionicons</strong> uses MIT.</li>
  <li><strong>Ribbon marks</strong> (<code>myarc-matte.png</code> and <code>myarc-glass.png</code>) are user-supplied originals whose redistribution and trademark clearance remain open, as recorded in <a href="{REPO}/blob/main/ATTRIBUTION.md">ATTRIBUTION.md</a>.</li>
  <li>Downloadable brand assets, wallpapers, social cards and the full visual spec live in <a href="{REPO}/tree/main/design/brand">design/brand</a> and <a href="{REPO}/blob/main/design.md">design.md</a>.</li>
</ul>
'''

    html, toc = _add_toc(body.strip())
    return Doc(
        slug='design-system',
        area='Brand and design',
        order=1,
        title=TITLE,
        summary=SUMMARY,
        html=html,
        toc=toc,
        extra_css=CSS,
    )
