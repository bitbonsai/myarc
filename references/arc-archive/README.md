# Arc website archive reference

Local research capture from the Internet Archive snapshot dated 2025-12-10 05:26:36 UTC:

- Source: https://web.archive.org/web/20251210052636/https://arc.net/
- Captured pages: home, Max, Search, Developers, Students, Blog, Privacy, and Security
- Screenshots: home, Max, and Search
- Assets: core homepage CSS, small JavaScript entry files, two PNG images, three homepage MP4 clips, and five Arc Max MP4 clips
- Discovery records: interactive snapshots, DOM asset inventory, network requests, and download results
- Local mirror: `site/index.html`, with navigation rewritten across all eight captured pages and bundled assets under `site/assets/`

All eight MP4 files were available as direct archived resources, so `yt-dlp` was unnecessary. The Max clips cover its hero sequence, Ask ChatGPT, Tidy Tabs, Tidy Downloads, and five-second previews.

The local mirror disables archived application scripts so links behave as regular local links. Assets downloaded successfully are local. Missing responsive images use absolute Wayback URLs because the archive stopped accepting connections during the mirror download; `site/asset-manifest.tsv` records each result.

These files are research references, not myarc product assets. Arc, its branding, and the captured site assets remain the property of their respective owners.
