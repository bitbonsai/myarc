#!/usr/bin/env node
// DevTools helper for tools/check_extension_store.sh.
//   node tools/check_extension_store.mjs enable <profile dir> <extension ID>
//     opens chrome://extensions in a background tab, waits up to 90 s for the
//     extension to install, prints how it arrived and turns it on there the
//     way the page's switch does, with chrome.management.setEnabled
//   node tools/check_extension_store.mjs worker <profile dir> <extension ID>
//     waits up to 30 s for the extension's service worker, then prints its
//     version and whether chrome.management calls it enabled
//   node tools/check_extension_store.mjs inject <profile dir> <extension ID> <URL>
//     opens <URL> unless a tab shows it, reloads it and waits up to 15 s for
//     the extension's content script world, then counts the password fields
//     that world sees
//   node tools/check_extension_store.mjs quit <profile dir>
//     closes the browser the way quitting does
import fs from 'node:fs'

const [mode, profile, id, url] = process.argv.slice(2)
if (!['enable', 'worker', 'inject', 'quit'].includes(mode) || !profile ||
    (mode !== 'quit' && !id) || (mode === 'inject' && !url)) {
  console.error('usage: check_extension_store.mjs enable|worker|inject|quit <profile dir> [extension ID] [URL]')
  process.exit(64)
}

const sleep = (ms) => new Promise((resolve) => setTimeout(resolve, ms))

// No step may hang the check: each gives up after 120 s.
setTimeout(() => {
  console.log(`${mode}: gave up after 120 s`)
  process.exit(1)
}, 120000)

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

const json = async (path) => (await fetch(`http://127.0.0.1:${await port()}/json/${path}`)).json()

const findTarget = async (match, tries = 60) => {
  for (let i = 0; i < tries; i++) {
    const target = (await json('list')).find(match)
    if (target) return target
    await sleep(1000)
  }
  return null
}

const session = (wsUrl) => new Promise((resolve, reject) => {
  const ws = new WebSocket(wsUrl)
  let next = 0
  const pending = new Map()
  const listeners = []
  ws.onmessage = (event) => {
    const message = JSON.parse(event.data)
    if (message.method) return listeners.forEach((listener) => listener(message))
    const waiter = pending.get(message.id)
    if (!waiter) return
    pending.delete(message.id)
    message.error ? waiter.reject(new Error(message.error.message)) : waiter.resolve(message.result)
  }
  ws.onerror = () => reject(new Error(`cannot attach to ${wsUrl}`))
  ws.onopen = () => resolve({
    send: (method, params = {}, sessionId) => new Promise((ok, fail) => {
      const callId = ++next
      pending.set(callId, { resolve: ok, reject: fail })
      ws.send(JSON.stringify({ id: callId, method, params, ...(sessionId ? { sessionId } : {}) }))
    }),
    on: (listener) => listeners.push(listener),
    close: () => ws.close(),
  })
})

const evaluate = async (cdp, expression, contextId) => {
  const { result, exceptionDetails } = await cdp.send('Runtime.evaluate',
    { expression, returnByValue: true, awaitPromise: true, ...(contextId ? { contextId } : {}) })
  if (exceptionDetails) throw new Error(exceptionDetails.exception?.description ?? exceptionDetails.text)
  return result.value
}

const origin = `chrome-extension://${id}`

if (mode === 'quit') {
  const cdp = await session((await json('version')).webSocketDebuggerUrl)
  await cdp.send('Browser.close').catch(() => {})
  console.log('asked the browser to close')
  process.exit(0)
}

if (mode === 'enable') {
  // The extensions page, where a user turns an extension on.
  const browser = await session((await json('version')).webSocketDebuggerUrl)
  await browser.send('Target.createTarget', { url: 'chrome://extensions/', background: true })
  browser.close()
  const page = await findTarget((t) => t.type === 'page' && t.url.startsWith('chrome://extensions'), 15)
  if (!page) throw new Error('chrome://extensions never opened')
  const cdp = await session(page.webSocketDebuggerUrl)
  let info = null
  for (let i = 0; i < 180 && !info; i++) {
    info = await evaluate(cdp, `chrome.management.get('${id}').then((e) => ({
      name: e.name, version: e.version, enabled: e.enabled, installType: e.installType,
      disabledReason: e.disabledReason ?? 'none' })).catch(() => null)`)
    if (!info) await sleep(500)
  }
  if (!info) throw new Error('the extension never installed (90 s)')
  const before = `installed ${info.name} ${info.version}, install type ${info.installType}, enabled ${info.enabled}, disabled reason ${info.disabledReason}`
  if (!info.enabled) {
    await cdp.send('Runtime.evaluate', {
      expression: `chrome.management.setEnabled('${id}', true)`, awaitPromise: true, userGesture: true,
    })
  }
  const after = await evaluate(cdp, `chrome.management.get('${id}').then((e) => e.enabled)`)
  await cdp.send('Page.close').catch(() => {})
  cdp.close()
  console.log(`${before}; enabled from chrome://extensions: ${after}`)
  process.exit(after ? 0 : 1)
}

if (mode === 'worker') {
  // Attached through the browser target, which lists workers that
  // /json/list can miss.
  const browser = await session((await json('version')).webSocketDebuggerUrl)
  let worker = null
  for (let i = 0; i < 60 && !worker; i++) {
    const { targetInfos } = await browser.send('Target.getTargets')
    worker = targetInfos.find((t) => t.type === 'service_worker' && t.url.startsWith(origin))
    if (!worker) await sleep(500)
  }
  if (!worker) {
    console.log('worker: not running after 30 s')
    process.exit(1)
  }
  const { sessionId } = await browser.send('Target.attachToTarget', { targetId: worker.targetId, flatten: true })
  const { result, exceptionDetails } = await browser.send('Runtime.evaluate', {
    expression: `chrome.management.getSelf().then((me) => ({ name: me.name, version: me.version, enabled: me.enabled, installType: me.installType }))`,
    returnByValue: true, awaitPromise: true,
  }, sessionId)
  browser.close()
  if (exceptionDetails) throw new Error(exceptionDetails.exception?.description ?? exceptionDetails.text)
  const self = result.value
  console.log(`worker: running at ${worker.url}, ${self.name} ${self.version}, enabled ${self.enabled}, install type ${self.installType}`)
  process.exit(self.enabled ? 0 : 1)
}

// inject
let page = (await json('list')).find((t) => t.type === 'page' && t.url.startsWith(url))
if (!page) {
  const browser = await session((await json('version')).webSocketDebuggerUrl)
  await browser.send('Target.createTarget', { url, background: true })
  browser.close()
  page = await findTarget((t) => t.type === 'page' && t.url.startsWith(url), 15)
}
if (!page) throw new Error(`no tab with ${url}`)
const cdp = await session(page.webSocketDebuggerUrl)
const worlds = new Map()
cdp.on((message) => {
  if (message.method === 'Runtime.executionContextCreated') {
    const context = message.params.context
    if (context.origin === origin) worlds.set(context.id, context)
  }
  if (message.method === 'Runtime.executionContextsCleared') worlds.clear()
})
await cdp.send('Runtime.enable')
await cdp.send('Page.enable')
await cdp.send('Page.reload', { ignoreCache: true })
let seen = null
for (let i = 0; i < 60 && !seen; i++) {
  await sleep(250)
  for (const context of worlds.values()) {
    try {
      const fields = await evaluate(cdp, `({ id: chrome.runtime.id, fields: document.querySelectorAll('input[type=password]').length, href: location.href })`, context.id)
      if (fields.id === id && fields.href.startsWith(url)) seen = { ...fields, name: context.name }
    } catch {
      // The world went away with a navigation; wait for the next one.
    }
  }
}
cdp.close()
if (!seen) {
  console.log(`content script: no world from ${origin} on ${url}`)
  process.exit(1)
}
console.log(`content script: injected as "${seen.name}" on ${url}, sees ${seen.fields} password field(s)`)
process.exit(0)
