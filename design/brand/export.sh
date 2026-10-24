#!/usr/bin/env bash
set -euo pipefail
cd "$(dirname "$0")"
root="$PWD"
cleanup() {
  agent-browser --session myarc-brand close >/dev/null 2>&1 || true
  pkill -f '/[a]gent-browser-darwin-arm64' || true
  pkill -f '[G]oogle Chrome for Testing' || true
}
trap cleanup EXIT
python3 build.py
while read -r name width height; do
  agent-browser --session myarc-brand open "file://$root/artwork/$name.html"
  agent-browser --session myarc-brand set viewport "$width" "$height"
  agent-browser --session myarc-brand eval "document.fonts.ready.then(() => Promise.all(Array.from(document.images).map(img => img.decode())))"
  agent-browser --session myarc-brand screenshot "$root/exports/$name.png"
done < <(python3 -c "import json; [print(a['name'], a['width'], a['height']) for a in json.load(open('artwork/manifest.json'))]")
agent-browser --session myarc-brand open "file://$root/index.html"
agent-browser --session myarc-brand eval "document.fonts.ready.then(() => Promise.all(Array.from(document.images).map(img => img.decode())))"
for width in 1440 728 390; do
  agent-browser --session myarc-brand set viewport "$width" 1000
  agent-browser --session myarc-brand eval --stdin <<'JS'
(() => {
  const rootSize = parseFloat(getComputedStyle(document.documentElement).fontSize)
  const outfitLoaded = Array.from(document.fonts).some(font => font.family === 'Outfit' && font.status === 'loaded')
  if (!outfitLoaded || !getComputedStyle(document.body).fontFamily.startsWith('Outfit')) throw new Error('Outfit must be loaded and active')
  if (parseFloat(getComputedStyle(document.body).fontSize) !== rootSize) throw new Error('Body must use 1rem')
  if (document.documentElement.scrollWidth > innerWidth) throw new Error('Horizontal overflow')
  const textElements = Array.from(document.querySelectorAll('body *')).filter(el =>
    el.getClientRects().length && Array.from(el.childNodes).some(node =>
      node.nodeType === Node.TEXT_NODE && node.textContent.trim()
    )
  )
  const minFont = Math.min(...textElements.map(el => parseFloat(getComputedStyle(el).fontSize)))
  if (minFont < rootSize * 0.875) throw new Error('Text below 0.875rem')
  for (const img of document.images) {
    const r = img.getBoundingClientRect()
    if (Math.abs(r.width / r.height - img.naturalWidth / img.naturalHeight) > 0.01) throw new Error('Distorted image: ' + img.src)
  }
  return {width: innerWidth, baseFont: rootSize, minFont, images: document.images.length, layout: 'PASS'}
})()
JS
done
python3 verify.py
