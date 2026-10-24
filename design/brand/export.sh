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
# Content-version local images after every render so previews cannot reuse old artwork.
python3 - <<'PY'
from pathlib import Path
import hashlib
import re
root = Path.cwd()
for page in [root / 'index.html', root / 'preview.html', root / 'type-study.html', *sorted((root / 'artwork').glob('*.html'))]:
    def version(match):
        path = match.group(1)
        digest = hashlib.sha256((page.parent / path).read_bytes()).hexdigest()[:12]
        return f'src="{path}?v={digest}"'
    source = page.read_text()
    page.write_text(re.sub(r'src="((?:\.\./)?(?:assets|exports)/[^"?]+\.(?:png|svg))(?:\?[^"]*)?"', version, source))
PY
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
agent-browser --session myarc-brand set viewport 1440 1000
agent-browser --session myarc-brand hover h1
agent-browser --session myarc-brand eval --stdin <<'JS'
(async () => {
  const link = document.querySelector('#footer a')
  await Promise.all(link.getAnimations().map(animation => animation.finished))
  const style = getComputedStyle(link)
  if (style.textUnderlineOffset !== '2px' || style.color !== 'rgba(3, 75, 244, 0.9)') throw new Error('Resting link style differs')
  const rect = link.getBoundingClientRect()
  window.brandLinkSize = {width: rect.width, height: rect.height}
  for (const node of document.querySelectorAll('.nav, .block, .end')) {
    const s = getComputedStyle(node)
    if (s.borderTopWidth !== '0px' || s.borderBottomWidth !== '0px') throw new Error('Page divider remains')
  }
  if (getComputedStyle(document.querySelector('.button')).backgroundColor !== 'rgb(3, 75, 244)') throw new Error('Primary button must be blue')
  return {linkAtRest: style.color, underline: style.textUnderlineOffset, dividers: 'none', primary: 'blue'}
})()
JS
agent-browser --session myarc-brand hover '#footer a'
agent-browser --session myarc-brand eval --stdin <<'JS'
(async () => {
  const link = document.querySelector('#footer a')
  await Promise.all(link.getAnimations().map(animation => animation.finished))
  const style = getComputedStyle(link)
  const rect = link.getBoundingClientRect()
  if (style.textUnderlineOffset !== '5px' || style.color !== 'rgb(3, 75, 244)') throw new Error('Link hover failed')
  if (rect.width !== window.brandLinkSize.width || rect.height !== window.brandLinkSize.height) throw new Error('Link hover shifts layout')
  const heart = getComputedStyle(document.querySelector('.heart'))
  if (heart.animationName !== 'heartbeat' || heart.animationDuration !== '1.5s') throw new Error('Heartbeat must be 40 BPM')
  return {hoverColor: style.color, underline: style.textUnderlineOffset, heartbeatBpm: 60 / parseFloat(heart.animationDuration), layout: 'unchanged'}
})()
JS
agent-browser --session myarc-brand set media light reduced-motion
agent-browser --session myarc-brand eval --stdin <<'JS'
(() => {
  if (!matchMedia('(prefers-reduced-motion: reduce)').matches) throw new Error('Reduced motion not emulated')
  if (getComputedStyle(document.querySelector('.heart')).animationName !== 'none') throw new Error('Heart moves under reduced motion')
  for (const node of document.querySelectorAll('a:not(.word)')) {
    if (getComputedStyle(node).transitionDuration !== '0s') throw new Error('Link transitions under reduced motion')
  }
  return {reducedMotion: 'PASS'}
})()
JS
python3 verify.py
