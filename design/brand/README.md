# myArc brand kit

Open [index.html](index.html) directly in a browser. Static identity exploration based on the supplied HTML reference and two supplied PNG marks. This is brand material, not a browser implementation or launch announcement.

## Contents

- Original matte and glass logos, unchanged at 1254 × 1254 with transparency.
- Three social cards: midnight and ivory at 1200 × 630, square at 1080 × 1080.
- Desktop wallpapers in both finishes at 2560 × 1600, plus glass phone wallpaper at 1440 × 2560.
- Two transparent horizontal lockups at 1550 × 480, and both marks at 128, 256, 512 and 1024 pixels.
- Editable [HTML artboards](artwork/), local Manrope font and its [SIL Open Font License](assets/Manrope-OFL.txt).

## Branding and type

Follow the repository README: **myArc, forever yours.** Display text uses `myArc`; filenames remain lowercase. The page uses a 1rem body and a 0.875rem minimum at every breakpoint. CSS font sizes use rem. Artwork uses larger source text so shared images remain readable when reduced.

## Structure and regeneration

`index.html` is the responsive brand board, with inline CSS and ordinary download links. It needs no server, network connection, JavaScript or build step to view. `build.py` owns the six fixed-size HTML artboards and raster lockups/icons. Exported PNGs are presentation assets, not application screenshots.

From repository root, run `bash design/brand/export.sh` to regenerate sources and screenshots. Requires Python 3, Pillow, and agent-browser with Chrome for Testing installed. Font and image decoding finish before capture. The script closes its session and terminates agent-browser/Chrome for Testing helpers on exit, including helpers from other agent-browser sessions. Run it when those sessions can be closed.

Run `python3 design/brand/verify.py` for local links, rem font sizes, minimum type size, branding/copy consistency, image dimensions, transparency and nonempty-content checks. The export script also checks actual font sizes, image proportions and overflow at desktop/mobile widths. These are branding checks, not application tests. Recreate the ZIP after changes; the archive contains this directory except itself and Python caches.

## Boundaries

Palette accent values come from the supplied HTML, not measured sampling. Originals are 1254-pixel PNGs, not vectors. Wallpaper logo placements are 1060 or 1150 pixels wide, below original resolution. The 128-pixel variants are raster reductions, not approved favicons. Artwork is not certified for print production.

Manrope comes from Google Fonts, authored by Mikhail Sharanda and contributors, under SIL OFL 1.1. Logo author, creation method and license were not supplied. Permission to use them in this requested draft does not establish redistribution or trademark rights. Public branding clearance remains open. No archived Arc assets or fonts are used.
