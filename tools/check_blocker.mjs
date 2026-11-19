#!/usr/bin/env node
// DevTools helper for tools/check_blocker.sh. Reads the fixture page's state
// and presses the bundled blocker popup's filtering-mode steps like a pointer.
//   node tools/check_blocker.mjs title <profile dir> <page URL prefix> [want]
//     waits up to 15 s for the page title to leave "tracker: pending" (and to
//     equal "tracker: <want>" when given), then prints it
//   node tools/check_blocker.mjs set-level <profile dir> <extension ID> <level>
//     clicks the popup's filtering-mode step with real mouse events: 0 is
//     "no filtering" (blocking off for the site), 2 is the default "optimal"
//   node tools/check_blocker.mjs popup-mode <profile dir> <extension ID> [want]
//     prints the mode the newest open popup shows for the site, waiting up to
//     10 s for it to equal <want> (such as "no filtering") when given
import fs from 'node:fs'

const [mode, profile, arg, want] = process.argv.slice(2)
if (!['title', 'set-level', 'popup-mode'].includes(mode) || !profile || !arg || (mode === 'set-level' && !want)) {
  console.error('usage: check_blocker.mjs title|set-level|popup-mode <profile dir> <arg> [want|level]')
  process.exit(64)
}

const sleep = (ms) => new Promise((resolve) => setTimeout(resolve, ms))

const port = async () => {
  for (let i = 0; i < 60; i++) {
    try {
      return fs.readFileSync(`${profile}/DevToolsActivePort`, 'utf8').split('\n')[0].trim()
    } catch {
      await sleep(250)
    }
  }
  throw new Error('DevToolsActivePort never appeared')
}

const targets = async () => {
  const response = await fetch(`http://127.0.0.1:${await port()}/json/list`)
  return response.json()
}

const findTarget = async (match) => {
  for (let i = 0; i < 60; i++) {
    const target = (await targets()).find(match)
    if (target) return target
    await sleep(250)
  }
  throw new Error('target not found')
}

const session = (target) => new Promise((resolve, reject) => {
  const ws = new WebSocket(target.webSocketDebuggerUrl)
  let next = 0
  const pending = new Map()
  ws.onmessage = (event) => {
    const message = JSON.parse(event.data)
    const waiter = pending.get(message.id)
    if (!waiter) return
    pending.delete(message.id)
    message.error ? waiter.reject(new Error(message.error.message)) : waiter.resolve(message.result)
  }
  ws.onerror = () => reject(new Error(`cannot attach to ${target.url}`))
  ws.onopen = () => resolve({
    send: (method, params = {}) => new Promise((ok, fail) => {
      const id = ++next
      pending.set(id, { resolve: ok, reject: fail })
      ws.send(JSON.stringify({ id, method, params }))
    }),
    close: () => ws.close(),
  })
})

const evaluate = async (cdp, expression) =>
  (await cdp.send('Runtime.evaluate', { expression, returnByValue: true })).result.value

if (mode === 'title') {
  let title = ''
  for (let i = 0; i < 60; i++) {
    const page = (await targets()).find((t) => t.type === 'page' && t.url.startsWith(arg))
    title = page?.title ?? ''
    if (title.startsWith('tracker: ') && title !== 'tracker: pending' &&
        (!want || title === `tracker: ${want}`)) break
    await sleep(250)
  }
  console.log(title)
  process.exit(want && title !== `tracker: ${want}` ? 1 : 0)
}

const popupUrl = `chrome-extension://${arg}/popup.html`
let popup = await findTarget((t) => t.url.startsWith(popupUrl))
if (mode === 'popup-mode') {
  // A popup opened again shows as a new target; read the newest one.
  popup = (await targets()).filter((t) => t.url.startsWith(popupUrl)).at(0) ?? popup
}
const cdp = await session(popup)

if (mode === 'popup-mode') {
  let shown = null
  for (let i = 0; i < 40; i++) {
    shown = await evaluate(cdp, `(() => {
      const level = document.querySelector('.filteringModeSlider')?.dataset.level
      const name = document.querySelector('#filteringModeText > span')?.textContent
      return level === undefined || !name || name === '_' ? null : { level, name }
    })()`)
    if (shown && (!want || shown.name === want)) break
    await sleep(250)
  }
  cdp.close()
  console.log(`popup mode: ${shown?.name ?? 'unknown'} (level ${shown?.level ?? '?'})`)
  process.exit(want && shown?.name !== want ? 1 : 0)
}
let box = null
for (let i = 0; i < 40 && !box; i++) {
  box = await evaluate(cdp, `(() => {
    const step = document.querySelector('.filteringModeSlider span[data-level="${want}"]')
    const r = step?.getBoundingClientRect()
    if (!r || r.width === 0) return null
    return { x: r.x + r.width / 2, y: r.y + r.height / 2,
             before: document.querySelector('#filteringModeText span')?.textContent }
  })()`)
  if (!box) await sleep(250)
}
if (!box) throw new Error('popup never showed its filtering slider')
for (const type of ['mouseMoved', 'mousePressed', 'mouseReleased']) {
  await cdp.send('Input.dispatchMouseEvent', {
    type, x: box.x, y: box.y, button: 'left', clickCount: type === 'mouseMoved' ? 0 : 1,
  })
}
console.log(`clicked level ${want} in the popup (mode before: ${box.before ?? 'unknown'})`)
cdp.close()
