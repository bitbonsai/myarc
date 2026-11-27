HTML: https://myarc.dev/docs/design-system/

# Design system

Brand identity, color tokens, typography, layout, components, native glass surfaces and the product decisions behind them.

myArc shares one visual language across its website, user docs and native desktop browser shell. It ships on macOS today, with Linux and Windows ahead using the same tokens and OS-native surfaces.

## Product decisions

Every visual choice traces back to a product constraint in [docs/PRODUCT.md](https://github.com/bitbonsai/myarc/blob/main/docs/PRODUCT.md) and [AGENTS.md](https://github.com/bitbonsai/myarc/blob/main/AGENTS.md).

#### Yours to keep

Workspace state lives in local JSON files with no mandatory account or behavioral tracking. Interfaces stay free of sign-in walls, upsell banners and telemetry prompts.

#### Familiar from day one

Carry forward Arc's sidebar, Spaces, Profiles, pinned tabs, Favorites, command bar, Peek, Split View and Little Arc. Spaces group tabs by topic; Profiles isolate cookies, logins, history and extensions, and several Spaces can share one Profile.

#### Restrained site, personal browser

The website uses a quiet editorial grid, clear typography and small blue accents. The browser keeps that clarity while letting Spaces carry warmer themes, custom icons and tactile motion.

#### AI by choice

Built-in AI is off by default and never required to browse. External agent control over CLI and MCP is a separate opt-in capability scoped to an Agent Space with a visible Stop control.

#### Motion with meaning

Animated onboarding is a release requirement. Motion clarifies spatial transitions and honors prefers-reduced-motion and OS reduce-transparency settings from the first frame.

#### Built in the open

Code is GPL-3.0 and plans stay public. Every surface separates working behavior from planned work and documents third-party attribution.

## Brand identity

Write the product name as **myArc** in display copy, headings and prose. Keep filenames, bundle identifiers, URLs and CLI commands lowercase (`myarc`). The primary tagline is **myArc, forever yours.**

### Marks and lockups

Two ribbon finishes serve different surfaces: the matte mark sits on light backgrounds and navigation bars, while the glass mark pairs with dark navy stages. Keep 12% clear space around the ribbon and preserve its proportions and colors.

![myArc matte ribbon logo on warm ivory](https://myarc.dev/blog/_assets/logo-matte-lg.f60a69a18811.webp)

Matte ribbon mark for light surfaces and site chrome.



![myArc glass ribbon logo on midnight navy](https://myarc.dev/blog/_assets/logo-glass-lg.6a37f9017d92.webp)

Glass ribbon mark for dark surfaces and promotional stages.

The horizontal lockup pairs a `1rem` ribbon mark with the outlined Outfit ExtraBold wordmark. The custom barless A belongs only to the wordmark; headings and body copy use Outfit's standard letters. Header wordmarks sit at `1.75rem` (`1.375rem` below `40rem`) with a `.5rem` gap (`.375rem` below `40rem`), and footer wordmarks sit at `1.5rem`.

![myArc matte ribbon beside ink wordmark](https://myarc.dev/blog/_assets/logo-matte.adbb3c7ddc14.webp)

Light lockup: ink wordmark (#182432) on ivory (#F5F2EC).



![myArc matte ribbon beside ivory wordmark](https://myarc.dev/blog/_assets/logo-matte.adbb3c7ddc14.webp)

Dark lockup: ivory wordmark (#F5F2EC) on dark navy (#0B1420).

## Color

Warm ivory and deep navy anchor the reading surfaces. Brand blue drives interactive focus and primary actions, while the ribbon spectrum supplies secondary accents for Spaces and artwork.

### Brand palette

**Ivory** #F5F2EC

**Ink** #182432

**Midnight** #101C2B

**Sky** #10AEFF

**Blue** #034BF4

**Violet** #4627EE

**Orchid** #A553EC

**Pink** #FF527E

**Coral** #FFAF8E

### Light and dark tokens

The site follows `prefers-color-scheme` in HTML and CSS with no theme toggle, stored override or script detector. Semantic CSS variables switch surfaces and text while `<picture>` elements swap ink and ivory wordmarks. Primary buttons keep the same blue fill and white label in both themes.

`--page`

#F5F2EC #0B1420

Page background behind the grain tile

`--text`

#182432 #D2D9E1

Primary body copy and header links

`--quiet`

#54616D #B1BCC9

Secondary copy, captions and metadata

`--panel`

#101C2B #182432

Code blocks and dark container surfaces

`--link-solid`

#034BF4 #8FB1FF

Hover links, focus rings and active nav

`--blue-deep`

#033DD0 #033DD0

Primary button hover and focus fill

`--heart`

#B86B73 #B86B73

Footer heart outline and 40 BPM pulse

## Typography

**Outfit** is the single embedded variable typeface across weights 100 to 900, falling back to Helvetica Neue and sans-serif. The root stays at `100%` so browser text-size preferences work, and every size is authored in `rem`.

### Type scale

Hero display

5.5rem · weight 600 · line-height 0.98 · Steps to 4.5rem, 3.75rem and 2.5rem on smaller viewports

Article / docs title

3.5rem · weight 600 · line-height 1.10 · Steps to 2.5rem below 48rem

Section heading

2.75rem · weight 600 · line-height 1.08 · 2.25rem below 48rem; 2rem inside docs and blog bodies

Subheading

1.5rem · weight 600 · line-height 1.30 · Feature titles, status labels and h3 sections

Lede / intro

1.25rem · weight 400 · line-height 1.50 · 1.375rem in homepage hero; 1.125rem on small screens

Body

1.125rem · weight 400 · line-height 1.55 · Default reading copy, capped at 68ch

UI / code / footer

1rem · weight 400–600 · line-height 1.40 · Nav links, tables, captions, kbd chips and inline code

### Reading rhythm

- Section headings sit above their content and share a single scale per breakpoint.
- Long-form prose in docs and blog posts caps at `68ch` so lines stay easy to scan.
- Inline code keeps Outfit inside a subtly bordered pill; fenced blocks sit on `--panel` with `--ivory` text.
- Space separates sections rather than horizontal divider rules.

## Layout and surfaces

The site wrapper caps at `80rem` with `3rem` desktop gutters and `1.5rem` mobile gutters at `48rem` and below. Sections use `6rem` vertical padding on desktop and `4.5rem` on mobile.

A deterministic monochrome grain tile repeats at `16rem` square behind page content. Light mode stacks two layers of the half-opacity WebP tile so texture reads on ivory; dark mode uses a single layer.

### Native browser glass

Modal dialogs, About myArc, updater sheets and agent prompts share one centralized frosted-glass token set (currently implemented for macOS in `browser/foundation/glass_mac.h`, with the same geometry and opaque fallback contract planned for Linux and Windows). When OS reduce-transparency is on, or on systems without window blur, glass surfaces switch cleanly to opaque tinted fills.

#### Frosted dialog surface

16 DIP outer corners, 1 DIP specular rim, 24 DIP interior padding and 7 DIP action buttons over a blurred backdrop.

Allow
Cancel

`kCornerRadius`

16 DIP

Outer corner radius on all frosted-glass dialogs and modal panels

`kButtonCornerRadius`

7 DIP

Corner radius for native dialog action buttons

`kPadding`

24 DIP

Standard interior dialog inset across body and actions

`kTitleTopPadding`

36 DIP

Top inset clearing the window close control above the title

`kButtonHeight / MinWidth`

36 / 84 DIP

Minimum hit target and width for dialog buttons

`kDarkGlassTint`

#18181D at 63%

Dark frosted tint with a 1 DIP specular rim at 20% alpha

`kLightGlassTint`

#FAFAFD at 83%

Light frosted tint with a 1 DIP specular rim at 14% alpha

`GlassMaterial presets`

4 levels

kWindow, kPopover, kHud and kSheet; opaque fallback under reduce-transparency or without blur

## Components

Interactive elements share the same focus ring (`.1875rem solid var(--link-solid)` with `.25rem` offset) and work without JavaScript.

Buttons and links

[Primary action](https://myarc.dev/download/)
[Secondary action](https://myarc.dev/docs/)
[Moving-underline link](https://myarc.dev/docs/install/)

Pills, status badges and keys

Docs
Done
Partial
Missing
`⌘L`
`⌘D`
`⌃⇧=`

### Iconography

- **Browser chrome and site controls** use **Lucide** icons on a 24px grid at stroke width 2 (ISC license): sidebar actions, Peek controls, command bar search, Settings tabs, close buttons and the footer heart.
- **Space icons** use 45 filled **Ionicons** glyphs (MIT license) alongside Unicode 15.1 emoji with English CLDR search annotations.
- Status indicators pair text labels with distinct border styles so meaning never depends on color alone.

## Motion and accessibility

Intro motion finishes within five seconds. Same-origin site navigation uses native cross-document View Transitions with a one-second blur and fade while keeping the header crisp. The homepage hero ribbon is drawn in WebGL, pauses off screen and falls back to the static image when motion is reduced.

**Micro feedback**

`120–200 ms`

Link underline shift (200ms), button scale 1.02 (180ms), glass dialogs (200ms)

**Surface transitions**

`220–350 ms`

Screenshot peek lift (300ms), lightbox scale 96% to 100% (300ms), onboarding moves

**Major reveals**

`600–1000 ms`

Hero rise (900ms), cross-document View Transitions (1s blur/fade), onboarding reveals

**Easing curve**

`cubic-bezier(.2, .7, .2, 1)`

Shared deceleration token (--ease) for entrances and surface motion

**Heartbeat**

`1.5 s (40 BPM)`

Footer heart scales to 1.18 on hover or keyboard focus

- `prefers-reduced-motion: reduce` disables View Transitions, smooth scrolling, hover scaling, pulse animations and spatial movement while preserving full content and layout.
- Native frosted dialogs honor OS reduce-transparency settings and fall back to opaque tinted surfaces where window blur is unavailable.
- Text contrast meets WCAG AA in both light and dark themes, verified across desktop and mobile viewports.

## Licensing and provenance

- **myArc code** is licensed under [GPL-3.0](https://github.com/bitbonsai/myarc/blob/main/LICENSE).
- **Outfit** is licensed under SIL Open Font License 1.1. **Lucide** uses the ISC license, and **Ionicons** uses MIT.
- **Ribbon marks** (`myarc-matte.png` and `myarc-glass.png`) are user-supplied originals whose redistribution and trademark clearance remain open, as recorded in [ATTRIBUTION.md](https://github.com/bitbonsai/myarc/blob/main/ATTRIBUTION.md).
- Downloadable brand assets, wallpapers, social cards and the full visual spec live in [design/brand](https://github.com/bitbonsai/myarc/tree/main/design/brand) and [design.md](https://github.com/bitbonsai/myarc/blob/main/design.md).
