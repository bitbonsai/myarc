// Renders the welcome's 3D ribbon showpiece as PNG frames with alpha, for
// tools/encode_hevc_alpha.swift to turn into the bundled video.
//
//   node tools/render_welcome_ribbon.mjs <out dir> [pixels]
//
// It loads mountRibbon() from tools/welcome_ribbon/index.html (the user's
// WebGL2 ribbon) into headless Chrome on a transparent page, with time
// stepped exactly 1/60 s per frame: requestAnimationFrame and performance.now
// are replaced, and Math.random is seeded, so every run gives the same
// frames. It skips the draw-in and the first sheen, plays the "spin"
// showpiece and keeps rendering until the mark is back at rest, then writes
// frame-NNN.png at <pixels> square (default 360, 3x a 120 pt box).
import { spawn } from 'node:child_process'
import { mkdirSync, mkdtempSync, readFileSync, rmSync, writeFileSync } from 'node:fs'
import { tmpdir } from 'node:os'
import { dirname, join, resolve } from 'node:path'
import { fileURLToPath } from 'node:url'

const [outArg, sizeArg] = process.argv.slice(2)
if (!outArg) {
  console.error('usage: node tools/render_welcome_ribbon.mjs <out dir> [pixels]')
  process.exit(64)
}
const out = resolve(outArg)
const size = Number(sizeArg || 360)
const here = dirname(fileURLToPath(import.meta.url))
const chrome = process.env.CHROME || '/Applications/Google Chrome.app/Contents/MacOS/Google Chrome'

// Seconds of the ribbon's own clock: rendered unseen until SKIP, then the
// showpiece starts and HOLD seconds are captured.
const SKIP = 5
const HOLD = 3.4
const FPS = 60

const source = readFileSync(join(here, 'welcome_ribbon/index.html'), 'utf8')
const script = source.match(/<script>\n(\/\/ myArc ribbon[\s\S]*?)<\/script>/)
if (!script) {
  console.error('mountRibbon() not found in tools/welcome_ribbon/index.html')
  process.exit(1)
}
const page = `<!doctype html><meta charset="utf-8">
<style>html, body { margin: 0; background: transparent; overflow: hidden }
canvas { display: block; width: ${size}px; height: ${size}px }</style>
<canvas></canvas>
<script>${script[1]}</script>
<script>window.ribbon = mountRibbon(document.querySelector('canvas'))</script>`

// Runs before the page's scripts: a fixed clock and a seeded random.
const prelude = `(() => {
  let now = 0, queue = [], id = 0, seed = 1
  Math.random = () => { seed = (seed * 16807) % 2147483647; return (seed - 1) / 2147483646 }
  performance.now = () => now
  window.requestAnimationFrame = (fn) => { queue.push([++id, fn]); return id }
  window.cancelAnimationFrame = (n) => { queue = queue.filter(([i]) => i !== n) }
  window.__step = (capture) => {
    now += 1000 / ${FPS}
    const run = queue; queue = []
    for (const [, fn] of run) fn(now)
    return capture ? document.querySelector('canvas').toDataURL('image/png') : ''
  }
})()`

const work = mkdtempSync(join(tmpdir(), 'ribbon-'))
writeFileSync(join(work, 'ribbon.html'), page)
mkdirSync(out, { recursive: true })
const port = 9333
const browser = spawn(chrome, [
  '--headless=new', `--remote-debugging-port=${port}`, `--user-data-dir=${join(work, 'profile')}`,
  `--window-size=${size},${size}`, '--force-device-scale-factor=1', '--hide-scrollbars',
  '--no-first-run', '--no-default-browser-check', 'about:blank',
], { stdio: 'ignore' })

const sleep = (ms) => new Promise((r) => setTimeout(r, ms))
let target
for (let i = 0; i < 100 && !target; i++) {
  await sleep(100)
  try {
    const list = await (await fetch(`http://127.0.0.1:${port}/json`)).json()
    target = list.find((t) => t.type === 'page')
  } catch {}
}
if (!target) {
  browser.kill()
  console.error('headless Chrome did not start')
  process.exit(1)
}
const ws = new WebSocket(target.webSocketDebuggerUrl)
await new Promise((r) => ws.addEventListener('open', r, { once: true }))
let next = 0
const pending = new Map()
ws.addEventListener('message', (e) => {
  const msg = JSON.parse(e.data)
  if (msg.id && pending.has(msg.id)) {
    pending.get(msg.id)(msg)
    pending.delete(msg.id)
  }
})
const send = (method, params = {}) => new Promise((r) => {
  const id = ++next
  pending.set(id, r)
  ws.send(JSON.stringify({ id, method, params }))
})
const evaluate = async (expression) => {
  const reply = await send('Runtime.evaluate', { expression, returnByValue: true })
  if (reply.result?.exceptionDetails) throw new Error(JSON.stringify(reply.result.exceptionDetails))
  return reply.result?.result?.value
}

try {
  await send('Page.enable')
  await send('Page.addScriptToEvaluateOnNewDocument', { source: prelude })
  await send('Page.navigate', { url: `file://${join(work, 'ribbon.html')}` })
  for (let i = 0; i < 100 && !(await evaluate('!!window.__step && document.readyState === "complete"')); i++) await sleep(50)
  if (!(await evaluate('!!window.ribbon'))) throw new Error('no WebGL2 in headless Chrome')
  for (let i = 0; i < SKIP * FPS; i++) await evaluate('__step(false)')
  await evaluate("ribbon.play('spin')")
  const frames = Math.round(HOLD * FPS)
  for (let i = 0; i < frames; i++) {
    const url = await evaluate('__step(true)')
    writeFileSync(join(out, `frame-${String(i).padStart(3, '0')}.png`), Buffer.from(url.split(',')[1], 'base64'))
  }
  console.log(`frames=${frames} size=${size} dir=${out}`)
} finally {
  ws.close()
  browser.kill()
  await sleep(300)
  rmSync(work, { recursive: true, force: true })
}
