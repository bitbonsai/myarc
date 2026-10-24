# myArc brand kit

Open [index.html](index.html) directly in a browser. Static identity exploration based on the supplied HTML reference and two supplied PNG marks. This is brand material, not a browser implementation or launch announcement.

## Contents

- Original matte and glass logos, unchanged at 1254 × 1254 with transparency.
- Three social cards: midnight and ivory at 1200 × 630, square at 1080 × 1080.
- Desktop wallpapers in both finishes at 2560 × 1600, plus glass phone wallpaper at 1440 × 2560.
- Two transparent horizontal lockups at 1550 × 480, both marks at 128, 256, 512 and 1024 pixels, and dark/light vector wordmarks with the custom A.
- Editable [HTML artboards](artwork/), local Outfit font and its [SIL Open Font License](assets/Outfit-OFL.txt).

## Branding and type

Follow the repository README: **myArc, forever yours.** Outfit is the selected typeface, with ExtraBold 800 for wordmarks and 600 for headings. Display text uses `myArc`; filenames remain lowercase. The page uses a 1rem body and a 0.875rem minimum at every breakpoint. CSS font sizes use rem. Artwork uses larger source text so shared images remain readable when reduced. A short ethos section records product principles, not shipped-feature claims.

## Typeface trial

Open [type-study.html](type-study.html) to compare selected Outfit against Sora and previous Manrope. Native radio controls switch the hero, body copy and wordmark samples without JavaScript. The selected wordmark uses an original barless A with outlined Outfit ExtraBold letters. Both finishes share generated geometry and need no installed font. Main page and exports use that custom wordmark; headings and body copy keep normal Outfit letters. [Alignment comparison](alignment.html) puts the original Outfit A and custom A on identical canvases with baseline and cap-height guides.

## Structure and regeneration

`index.html` is the responsive brand board, with inline CSS and ordinary download links. It needs no server, network connection, JavaScript or build step to view. `build.py` owns the six fixed-size HTML artboards, shared vector wordmarks, raster lockups/icons and deterministic grain tile. Grain sits behind page content on warm ivory; archived noise artwork is not reused. Exported PNGs are presentation assets, not application screenshots.

From repository root, run `bash design/brand/export.sh` to regenerate sources and screenshots. Requires Python 3 with Pillow and fontTools, `rsvg-convert` from librsvg, and agent-browser with Chrome for Testing installed. Font and image decoding finish before capture. The script closes its session and terminates agent-browser/Chrome for Testing helpers on exit, including helpers from other agent-browser sessions. Run it when those sessions can be closed.

Run `python3 design/brand/verify.py` for local links, rem font sizes, minimum type size, branding/copy consistency, image dimensions, transparency and nonempty-content checks. The export script also checks actual font sizes, image proportions and overflow at desktop/mobile widths, text-link hover/fade without layout shift, 40 BPM footer heartbeat and reduced-motion behavior. It content-versions image URLs after rendering so previews cannot retain older wordmarks. These are branding checks, not application tests. Recreate the ZIP after changes; the archive contains this directory except itself and Python caches.

## Interaction details

Page sections use spacing instead of divider rules. Primary buttons use brand blue. Text links are brand blue at 90% alpha, becoming opaque on hover or focus. They use the moving-underline treatment observed on mauriciowolff.com/blog: a 2-to-5-pixel offset and text-color fade over 200 ms, expressed in rem for the page. The footer uses Lucide's outlined heart in dim red, pulsing at 40 BPM on hover or keyboard focus. Reduced motion disables the heartbeat and link transitions. The footer links to bitbonsai on GitHub; product stage remains documented in the repository README.

## Boundaries

Palette accent values come from the supplied HTML, not measured sampling. Ribbon originals are 1254-pixel PNGs, not vectors. Wallpaper logo placements are 1060 or 1150 pixels wide, below original resolution. The 128-pixel variants are raster reductions, not approved favicons. Artwork is not certified for print production.

Outfit comes from Google Fonts, authored by the Outfit Project Authors, under SIL OFL 1.1. Sora and Manrope remain bundled for the separate typeface study, with their own OFL notices. Logo author, creation method and license were not supplied. Permission to use them in this requested draft does not establish redistribution or trademark rights. Public branding clearance remains open. No archived Arc assets or fonts are used.
