// Developer-only regression checks for marked, disposable Chromium profiles.
import assert from 'node:assert/strict'
import fs from 'node:fs'
import { setTimeout as delay } from 'node:timers/promises'

const [profileArg, pidArg, scenario] = process.argv.slice(2)
assert.ok(profileArg && /^\d+$/.test(pidArg ?? '') && ['extension', 'restart', 'runtime', 'workspace-pin'].includes(scenario),
  'Usage: node tools/check_browser_foundation.mjs PROFILE_DIR BROWSER_PID extension|restart|runtime|workspace-pin')
const profile = fs.realpathSync(profileArg)
assert.ok(fs.existsSync(`${profile}/.myarc-m0-test-profile`), 'Expected marked, disposable test profile')
const port = fs.readFileSync(`${profile}/DevToolsActivePort`, 'utf8').trim().split('\n')[0]
assert.ok(/^\d+$/.test(port))
const pin = JSON.parse(fs.readFileSync(new URL('../browser/upstream.json', import.meta.url), 'utf8'))
const version = await (await fetch(`http://127.0.0.1:${port}/json/version`, { signal: AbortSignal.timeout(5000) })).json()
assert.equal(version.Browser, `Chrome/${pin.version}`)
const ws = new WebSocket(version.webSocketDebuggerUrl)
await new Promise((resolve, reject) => {
  const timer = setTimeout(() => { ws.close(); reject(new Error('Browser connection timed out')) }, 5000)
  ws.addEventListener('open', () => { clearTimeout(timer); resolve() }, { once: true })
  ws.addEventListener('error', error => { clearTimeout(timer); reject(error) }, { once: true })
})
try {
  let nextId = 0
  const pending = new Map()
  const requests = new Map()
  const failures = []
  const responses = []
  const crashedTargets = []
  ws.addEventListener('message', event => {
    const message = JSON.parse(event.data)
    if (message.id) {
      const p = pending.get(message.id)
      if (!p) return
      clearTimeout(p.timer)
      pending.delete(message.id)
      if (message.error) p.reject(new Error(JSON.stringify(message.error)))
      else p.resolve(message.result)
    }
    if (message.method === 'Target.targetCrashed') crashedTargets.push(message.params.targetId)
    if (message.method === 'Network.requestWillBeSent' && message.params.request.url === adURL) requests.set(message.params.requestId, message.params.request.url)
    if (message.method === 'Network.loadingFailed' && requests.has(message.params.requestId)) failures.push(message.params.errorText)
    if (message.method === 'Network.responseReceived' && requests.has(message.params.requestId)) responses.push({ url: message.params.response.url, status: message.params.response.status })
  })
  const send = (method, params = {}, sessionId) => new Promise((resolve, reject) => {
    const id = ++nextId
    const timer = setTimeout(() => { pending.delete(id); reject(new Error(`Timed out: ${method}`)) }, 15000)
    pending.set(id, { resolve, reject, timer })
    ws.send(JSON.stringify({ id, method, params, ...(sessionId ? { sessionId } : {}) }))
  })
  const adURL = 'https://googleads.g.doubleclick.net/pagead/id'
  const extensionId = 'ddkjiahejlhfcafbddmgiahcphecmpfh'
  const detailsURL = `chrome://extensions/?id=${extensionId}`
  const processInfo = await send('SystemInfo.getProcessInfo')
  const ownedPid = Number(pidArg)
  assert.ok(Number.isSafeInteger(ownedPid) && ownedPid > 0)
  assert.ok(processInfo.processInfo.some(process => process.type === 'browser' && process.id === ownedPid), 'CDP browser PID does not match the requested test process')
  // shortcut: reuse an open test tab, isolate the default-profile Target.createTarget assertion before extending this path.
  const { targetInfos } = await send('Target.getTargets')
  if (scenario === 'workspace-pin') {
    assert.equal(targetInfos.filter(target => target.type === 'page').length, 1,
      'workspace-pin requires a fresh browser with exactly one ordinary fixture tab')
  }
  const target = targetInfos.find(target => target.type === 'page' && (scenario === 'workspace-pin'
    ? target.url === 'http://127.0.0.1:8766/' : (
    target.url.startsWith('http://127.0.0.1:8766/') ||
    target.url.startsWith('https://chromewebstore.google.com/') ||
    target.url.startsWith('https://example.com/') ||
    target.url.startsWith('chrome://extensions/') ||
    target.url.startsWith(`chrome-extension://${extensionId}/`)
  )))
  assert.ok(target, 'Expected an existing test page; refusing to create a new browser target')
  const { targetId } = target
  const { sessionId } = await send('Target.attachToTarget', { targetId, flatten: true })
  const pageSend = (method, params) => send(method, params, sessionId)
  const evaluate = async expression => {
    const result = await pageSend('Runtime.evaluate', { expression, awaitPromise: true, returnByValue: true })
    assert.equal(result.exceptionDetails, undefined, JSON.stringify(result.exceptionDetails))
    return result.result.value
  }
  const until = async (expression, description) => {
    for (let i = 0; i < 100; i++) {
      if (await evaluate(expression)) return
      await delay(100)
    }
    throw new Error(`Not ready: ${description}`)
  }
  const navigate = async url => {
    const result = await pageSend('Page.navigate', { url })
    assert.equal(result.errorText, undefined)
    await until(`location.href === ${JSON.stringify(url)} && document.readyState === 'complete'`, url)
  }
  const toggle = `document.querySelector('extensions-manager')?.shadowRoot?.querySelector('extensions-detail-view')?.shadowRoot?.querySelector('#enableToggle')`
  const details = async () => {
    await navigate(detailsURL)
    await until(`Boolean(${toggle})`, 'extension enable toggle')
    const text = await evaluate(`document.querySelector('extensions-manager').shadowRoot.querySelector('extensions-detail-view').shadowRoot.textContent`)
    assert.ok(text.includes('2026.1021.1354'))
  }
  const setEnabled = async enabled => {
    await details()
    if (await evaluate(`${toggle}.checked`) !== enabled) await evaluate(`${toggle}.click()`)
    await until(`${toggle}.checked === ${enabled}`, `extension enabled=${enabled}`)
    await delay(300)
  }
  const probe = async expectedBlocked => {
    await navigate('https://example.com/')
    failures.length = 0
    requests.clear()
    responses.length = 0
    const result = await evaluate(`(async () => { try { const r = await fetch(${JSON.stringify(adURL)}, { mode: 'no-cors', cache: 'no-store', credentials: 'omit', signal: AbortSignal.timeout(5000) }); return { resolved: true, type: r.type } } catch (error) { return { resolved: false, name: error.name } } })()`)
    await delay(100)
    const localRedirect = responses.some(response => response.url.startsWith(`chrome-extension://${extensionId}/web_accessible_resources/`))
    const originalNetworkResponse = responses.some(response => {
      const url = new URL(response.url)
      const expected = new URL(adURL)
      return url.origin === expected.origin && url.pathname === expected.pathname
    })
    if (expectedBlocked) {
      assert.ok(failures.includes('net::ERR_BLOCKED_BY_CLIENT') || localRedirect, JSON.stringify({ result, failures, responses }))
      assert.ok(!originalNetworkResponse, 'Enabled extension must prevent original network response')
    } else {
      assert.equal(result.resolved, true)
      assert.equal(result.type, 'opaque')
      assert.equal(failures.length, 0)
      assert.ok(originalNetworkResponse, JSON.stringify(responses))
    }
    console.log(JSON.stringify({ browser: version.Browser, enabled: expectedBlocked, fetch: result, networkErrors: failures, responses }))
  }
  try {
    await pageSend('Network.enable')
    if (scenario === 'workspace-pin') {
      const fixtureURL = 'http://127.0.0.1:8766/'
      const fixtureId = 'faokaoibbnjigecocmhniflfnkpmacle'
      const scopeURL = `chrome-extension://${fixtureId}/`
      const manifest = JSON.parse(fs.readFileSync(new URL('../tests/fixtures/browser/workspace-pin-extension/manifest.json', import.meta.url), 'utf8'))
      const waitFor = async (check, description) => {
        const deadline = Date.now() + 10000
        do {
          const value = await check()
          if (value) return value
          await delay(100)
        } while (Date.now() < deadline)
        throw new Error(`Not ready: ${description}`)
      }
      // Read the browser-owned Profile path, rather than trusting a copied port file.
      await navigate('chrome://version/')
      await until(`Boolean(document.querySelector('#profile_path')?.textContent.trim().startsWith('/'))`, 'native Profile path')
      assert.equal(fs.realpathSync(await evaluate(`document.querySelector('#profile_path').textContent.trim()`)),
        fs.realpathSync(`${profile}/Default`), 'Selected tab belongs to a different Profile')
      const commandLine = await evaluate(`document.querySelector('#command_line').textContent`)
      for (const flag of ['--myarc-visual-parity', '--myarc-workspace-sidebar']) {
        assert.ok(commandLine.includes(flag), `Missing temporary test flag: ${flag}`)
      }
      await navigate(fixtureURL)
      assert.equal(await evaluate(`document.title === 'myArc foundation check' && Boolean(document.querySelector('#form #note'))`), true,
        'Expected original synthetic form')
      const readMetadata = () => {
        let fd
        try {
          fd = fs.openSync(`${profile}/Default/myarc-workspace-v1.json`, 'r')
          // shortcut: fresh single-tab probe only, expand this bound for a larger native journey.
          const buffer = Buffer.alloc(65537)
          const size = fs.readSync(fd, buffer)
          assert.ok(size <= 65536, 'Fresh workspace metadata exceeds 64 KiB')
          const state = JSON.parse(buffer.subarray(0, size).toString('utf8'))
          assert.equal(state.version, 1)
          assert.match(state.profile_id, /^[1-9]\d*$/)
          assert.ok(Array.isArray(state.items) && state.items.length <= 1024)
          assert.ok(Array.isArray(state.spaces) && Array.isArray(state.windows))
          return state
        } catch (error) {
          if (error.code === 'ENOENT') return null
          throw error
        } finally {
          if (fd !== undefined) fs.closeSync(fd)
        }
      }
      const initial = await waitFor(() => {
        const state = readMetadata()
        return state?.items.length === 1 && state.windows.length === 1 && state.spaces.length === 1 &&
          state.items[0].placement === 0 && state.items[0].has_url === false && state.items[0].canonical_url === '' &&
          state.windows[0].selected_item_id === state.items[0].id && state
      }, 'fresh Temporary persisted by the native workspace renderer (renderer must be enabled)')
      assert.match(initial.items[0].id, /^[1-9]\d*$/)
      let workerSession
      try {
        await pageSend('ServiceWorker.enable')
        // One supported lifecycle start wakes an idle MV3 worker; attachment lasts only for this probe.
        await pageSend('ServiceWorker.startWorker', { scopeURL })
        const worker = await waitFor(async () => {
          const { targetInfos } = await send('Target.getTargets')
          const workers = targetInfos.filter(info => info.type === 'service_worker' && info.url === `${scopeURL}worker.js`)
          assert.ok(workers.length <= 1, 'Ambiguous fixture worker')
          return workers[0]
        }, 'loaded fixture service worker; check normal upstream --load-extension support')
        assert.equal(worker.browserContextId, target.browserContextId, 'Fixture worker belongs to a different browser context')
        workerSession = (await send('Target.attachToTarget', { targetId: worker.targetId, flatten: true })).sessionId
        const workerEval = async expression => {
          const result = await send('Runtime.evaluate', { expression, awaitPromise: true, returnByValue: true }, workerSession)
          assert.equal(result.exceptionDetails, undefined, JSON.stringify(result.exceptionDetails))
          return result.result.value
        }
        await waitFor(() => workerEval(`typeof myarcWorkspaceFixtureTab === 'function' && typeof myarcWorkspaceFixturePin === 'function'`),
          'fixture worker initialization')
        const identity = await workerEval(`({ id: chrome.runtime.id, manifest: chrome.runtime.getManifest(), url: location.href })`)
        assert.deepEqual(identity, { id: fixtureId, manifest, url: `${scopeURL}worker.js` }, 'Wrong fixture worker')
        const inspectTab = async () => {
          const { targetInfos } = await send('Target.getTargets')
          const pages = targetInfos.filter(info => info.type === 'page')
          assert.equal(pages.length, 1, 'Fixture browser tab set changed')
          assert.equal(pages[0].targetId, targetId, 'Fixture target changed')
          assert.equal(pages[0].url, fixtureURL, 'Fixture target navigated')
          return workerEval('myarcWorkspaceFixtureTab()')
        }
        const tab = await inspectTab()
        assert.equal(tab.pinned, false, 'Probe requires an initially unpinned fixture tab')
        const mutate = async pinned => {
          assert.deepEqual(await inspectTab(), { ...tab, pinned: !pinned }, 'Fixture tab identity/state changed before mutation')
          return workerEval(`myarcWorkspaceFixturePin(${tab.id}, ${pinned})`)
        }
        await mutate(true)
        const saved = await waitFor(async () => {
          const live = await inspectTab()
          assert.equal(live.id, tab.id)
          const state = readMetadata()
          const item = state?.items.find(item => item.id === initial.items[0].id)
          return live.pinned === true && state?.items.length === 1 && state.profile_id === initial.profile_id &&
            item?.placement === 2 && item.has_url === true && item.canonical_url === fixtureURL &&
            item.owner_id === state.profile_id && item
        }, 'real native pin projection and persisted Profile Favorite')
        const unpinned = await mutate(false)
        assert.deepEqual(unpinned, { ...tab, pinned: true }, 'Generic extension Unpin must leave native pin true')
        // Check actual read-backs across queued projection/write turns; elapsed time alone never passes.
        for (let sample = 0; sample < 5; sample++) {
          assert.deepEqual(await inspectTab(), { ...tab, pinned: true })
          const state = readMetadata()
          assert.equal(state?.profile_id, initial.profile_id)
          assert.deepEqual(state.items, [saved], 'Generic Unpin changed saved ID, canonical URL, owner or placement')
          if (sample < 4) await delay(100)
        }
        console.log(JSON.stringify({ scenario, browser: version.Browser, browserPid: ownedPid, targetId, tabId: tab.id, favorite: saved }))
        console.log('PASS: real extension Pin saved a Favorite; generic Unpin retained native pin and persisted identity (Favorite left saved)')
      } finally {
        if (workerSession) await send('Target.detachFromTarget', { sessionId: workerSession }).catch(() => {})
        await pageSend('ServiceWorker.disable').catch(() => {})
      }
    } else if (scenario === 'runtime') {
      await navigate('http://127.0.0.1:8766/')
      await until(`Boolean(document.querySelector('#note'))`, 'synthetic form')
      await evaluate(`
      document.querySelector('#note').value = 'M0 durable browser note'
      document.querySelector('#form').requestSubmit()
    `)
      assert.equal(await evaluate(`document.querySelector('#result').textContent`), 'Saved: M0 durable browser note')
      await send('Target.setDiscoverTargets', { discover: true })
      ws.send(JSON.stringify({ id: ++nextId, method: 'Page.crash', sessionId }))
      for (let i = 0; i < 50 && !crashedTargets.includes(targetId); i++) await delay(100)
      assert.ok(crashedTargets.includes(targetId), 'Renderer crash event observed')
      await pageSend('Page.reload', { ignoreCache: true })
      await until(`document.readyState === 'complete' && Boolean(document.querySelector('#note'))`, 'renderer reload')
      assert.equal(await evaluate(`localStorage.getItem('myarc-test-note')`), 'M0 durable browser note')
      const processes = await send('SystemInfo.getProcessInfo')
      assert.ok(processes.processInfo.some(process => process.type === 'browser' && process.id === ownedPid))
      console.log('PASS: renderer crash/reload preserved acknowledged localStorage; browser PID unchanged')
      const privateProbe = async write => {
        const { browserContextId } = await send('Target.createBrowserContext', { disposeOnDetach: true })
        try {
          const { targetId: privateTarget } = await send('Target.createTarget', { url: 'http://127.0.0.1:8766/', browserContextId, background: true })
          const { sessionId: privateSession } = await send('Target.attachToTarget', { targetId: privateTarget, flatten: true })
          const privateEval = async expression => {
            const result = await send('Runtime.evaluate', { expression, returnByValue: true }, privateSession)
            assert.equal(result.exceptionDetails, undefined)
            return result.result.value
          }
          let ready = false
          for (let i = 0; i < 100; i++) {
            if (await privateEval(`document.readyState === 'complete' && Boolean(document.querySelector('#note'))`)) { ready = true; break }
            await delay(100)
          }
          assert.ok(ready, 'Off-the-record fixture loaded')
          const info = await send('Target.getTargetInfo', { targetId: privateTarget })
          assert.equal(info.targetInfo.browserContextId, browserContextId)
          assert.equal(await privateEval(`localStorage.getItem('myarc-test-note')`), null)
          if (write) {
            await privateEval(`localStorage.setItem('myarc-test-note', 'M0 private-only note')`)
            assert.equal(await privateEval(`localStorage.getItem('myarc-test-note')`), 'M0 private-only note')
          }
        } finally {
          await send('Target.disposeBrowserContext', { browserContextId })
        }
      }
      await privateProbe(true)
      assert.equal(await evaluate(`localStorage.getItem('myarc-test-note')`), 'M0 durable browser note')
      await privateProbe(false)
      console.log('PASS: off-the-record localStorage isolated from normal context and absent in fresh off-the-record context')
    } else if (scenario === 'restart') {
      const previous = JSON.parse(fs.readFileSync(`${profile}/.myarc-m0-extension-check.json`, 'utf8'))
      assert.notEqual(previous.browserPid, ownedPid, 'Restart the owned browser before this check')
      assert.equal(previous.browserVersion, version.Browser)
      await details()
      assert.equal(await evaluate(`${toggle}.checked`), true, 'enabled state persisted after restart')
      await probe(true)
      console.log('PASS: enabled extension and blocking survive restart')
    } else {
      await setEnabled(true)
      await probe(true)
      await setEnabled(false)
      await probe(false)
      await setEnabled(true)
      await probe(true)
      fs.writeFileSync(`${profile}/.myarc-m0-extension-check.json`, JSON.stringify({
        browserPid: ownedPid, browserVersion: version.Browser, extensionVersion: '2026.1021.1354'
      }) + '\n')
      console.log('PASS: enabled/disabled/re-enabled native extension behavior')
    }
  } finally {
    if (scenario === 'extension') await setEnabled(true)
    await send('Target.detachFromTarget', { sessionId }).catch(() => {})
  }
} finally {
  ws.close()
}
