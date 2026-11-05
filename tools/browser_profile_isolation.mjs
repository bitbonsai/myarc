// Developer acceptance tooling for BC-001: two Profiles share no cookies,
// storage, service workers, history or command bar results across an in-place
// Profile swap. Needs a build launched with --myarc-capture-profile-swap=cdp
// and --log-file=PROFILE/myarc-test.log (see tools/check_profile_isolation.sh).
import assert from 'node:assert/strict'
import { spawnSync } from 'node:child_process'
import { randomBytes } from 'node:crypto'
import fs from 'node:fs'
import { setTimeout as delay } from 'node:timers/promises'

export const isolationCheckpointName = '.myarc-m0-profile-isolation.json'
export const isolationLogName = 'myarc-test.log'
export const fixtureOrigin = 'http://127.0.0.1:8766'

// One Profile's synthetic data. Names differ per side, so each side can look
// for the other's while keeping its own.
export const isolationData = (side, token) => {
  assert.ok(['a', 'b'].includes(side), 'Unknown side')
  assert.match(token, /^iso[0-9a-f]{12}$/, 'Bad token')
  return {
    side,
    token,
    cookie: `myarc_iso_${side}`,
    storageKey: `myarc-iso-${side}`,
    database: `myarc-iso-${side}`,
    scope: `${fixtureOrigin}/iso-${side}/`,
    url: `${fixtureOrigin}/?myarc-iso=${token}`
  }
}

export const newToken = () => `iso${randomBytes(6).toString('hex')}`

// "myarc-swap palette <n> rows=tab:<url> page:<url> search:<url> "
export const parsePaletteRows = (log, request) => {
  const marker = `myarc-swap palette ${request} rows=`
  const line = log.split('\n').find(each => each.includes(marker))
  if (line === undefined) return null
  return line.slice(line.indexOf(marker) + marker.length).trim().split(' ').filter(Boolean).map(row => {
    const colon = row.indexOf(':')
    assert.ok(colon > 0, `Bad palette row: ${row}`)
    const kind = row.slice(0, colon)
    assert.ok(['tab', 'page', 'search'].includes(kind), `Bad palette row kind: ${kind}`)
    return { kind, url: row.slice(colon + 1) }
  })
}

// A search row only echoes the typed text; any other row naming the token
// came from history, bookmarks or open tabs.
export const rowsNaming = (rows, token) => rows.filter(row => row.kind !== 'search' && row.url.includes(token))

export const readBoundedLog = (path, limit = 16 * 1024 * 1024) => {
  const size = fs.statSync(path).size
  assert.ok(size <= limit, 'Test log exceeds bound')
  return fs.readFileSync(path, 'utf8')
}

// The other Profile's directory from Local State, which lists every Profile.
export const otherProfileDir = localState => {
  const dirs = Object.keys(localState?.profile?.info_cache ?? {}).filter(dir => dir !== 'Default')
  assert.equal(dirs.length, 1, `Expected exactly one Profile besides Default, found ${JSON.stringify(dirs)}`)
  assert.match(dirs[0], /^Profile \d+$/)
  return dirs[0]
}

export const sqliteCount = (file, sql) => {
  assert.ok(fs.existsSync(file), `Missing ${file}`)
  // immutable: the browser has quit, and History's journal mode won't open read-only.
  const result = spawnSync('sqlite3', [`file:${encodeURI(file)}?immutable=1`, sql], { encoding: 'utf8', timeout: 10000 })
  assert.equal(result.status, 0, `sqlite3 failed on ${file}: ${result.stderr}`)
  const count = Number(result.stdout.trim())
  assert.ok(Number.isSafeInteger(count), `Unexpected sqlite3 output: ${result.stdout}`)
  return count
}

// After quitting: each Profile's Cookies holds only its cookie, and its
// History has its own unique URL and not the other's.
export const checkProfileFiles = (profile, dirs, sides, count = sqliteCount) => {
  const results = []
  for (const [own, other] of [['a', 'b'], ['b', 'a']]) {
    const dir = `${profile}/${dirs[own]}`
    const cookies = name => count(`${dir}/Network/Cookies`, `select count(*) from cookies where name = '${name}'`)
    const visits = token => count(`${dir}/History`, `select count(*) from urls where url like '%${token}%'`)
    const row = {
      dir: dirs[own],
      ownCookie: cookies(sides[own].cookie),
      otherCookie: cookies(sides[other].cookie),
      ownHistory: visits(sides[own].token),
      otherHistory: visits(sides[other].token)
    }
    assert.equal(row.ownCookie, 1, `${dirs[own]} Cookies lacks its own cookie`)
    assert.equal(row.otherCookie, 0, `${dirs[own]} Cookies holds the other Profile's cookie`)
    assert.ok(row.ownHistory >= 1, `${dirs[own]} History lacks its own URL`)
    assert.equal(row.otherHistory, 0, `${dirs[own]} History holds the other Profile's URL`)
    results.push(row)
  }
  return results
}

const seedExpression = data => `(async () => {
  document.cookie = '${data.cookie}=${data.token}; max-age=86400; path=/; SameSite=Lax'
  localStorage.setItem('${data.storageKey}', '${data.token}')
  await new Promise((resolve, reject) => {
    const open = indexedDB.open('${data.database}', 1)
    open.onupgradeneeded = () => open.result.createObjectStore('notes')
    open.onerror = () => reject(open.error)
    open.onsuccess = () => {
      const tx = open.result.transaction('notes', 'readwrite')
      tx.objectStore('notes').put('${data.token}', 'note')
      tx.oncomplete = () => { open.result.close(); resolve() }
      tx.onerror = () => reject(tx.error)
    }
  })
  const registration = await navigator.serviceWorker.register('/isolation-sw.js', { scope: '${new URL(data.scope).pathname}' })
  const worker = registration.installing || registration.waiting || registration.active
  if (worker.state !== 'activated') {
    await new Promise(resolve => worker.addEventListener('statechange', () => worker.state === 'activated' && resolve()))
  }
  return true
})()`

const probeExpression = data => `(async () => ({
  documentCookie: document.cookie,
  cookie: document.cookie.split('; ').some(each => each.startsWith('${data.cookie}=')),
  storage: localStorage.getItem('${data.storageKey}'),
  database: (await indexedDB.databases()).some(each => each.name === '${data.database}'),
  workers: (await navigator.serviceWorker.getRegistrations()).map(each => each.scope)
}))()`

// Reads chrome://history's rendered results, searching through shadow roots.
const historyExpression = `(() => {
  const all = []
  const walk = root => {
    for (const element of root.querySelectorAll('*')) {
      all.push(element)
      if (element.shadowRoot) walk(element.shadowRoot)
    }
  }
  walk(document)
  const items = all.filter(element => element.localName === 'history-item').map(element => element.item?.url ?? '')
  const empty = all.find(element => element.id === 'no-results')
  return { items, empty: Boolean(empty && !empty.hidden && empty.getClientRects().length && empty.textContent.trim()) }
})()`

export const runProfileIsolation = async ({ send, profile, ownedPid, scenario }) => {
  assert.ok(['profile-isolation', 'profile-isolation-restart'].includes(scenario), 'Unknown isolation phase')
  const restart = scenario === 'profile-isolation-restart'
  const checkpointPath = `${profile}/${isolationCheckpointName}`
  const logPath = `${profile}/${isolationLogName}`
  assert.ok(fs.existsSync(logPath), 'Launch with --enable-logging --log-file=PROFILE/myarc-test.log')
  const previous = restart ? JSON.parse(fs.readFileSync(checkpointPath, 'utf8')) : null
  if (previous) {
    assert.equal(previous.format, 1)
    assert.notEqual(previous.browserPid, ownedPid, 'Relaunch the browser before the restart phase')
  } else {
    assert.ok(!fs.existsSync(checkpointPath), 'Checkpoint exists; use a fresh profile')
  }
  const sides = previous
    ? { a: isolationData('a', previous.tokens.a), b: isolationData('b', previous.tokens.b) }
    : { a: isolationData('a', newToken()), b: isolationData('b', newToken()) }
  let request = previous ? previous.request + 100 : 0
  const results = []
  const record = (step, value) => {
    results.push({ step, ...value })
    console.log(JSON.stringify({ step, ...value }))
  }

  const waitFor = async (check, description, timeoutMs = 15000) => {
    const deadline = Date.now() + timeoutMs
    do {
      const value = await check()
      if (value) return value
      await delay(150)
    } while (Date.now() < deadline)
    throw new Error(`Not ready: ${description}`)
  }
  const pages = async () => (await send('Target.getTargets')).targetInfos.filter(info => info.type === 'page')
  const sessions = new Map()
  const page = async targetId => {
    if (!sessions.has(targetId)) {
      const { sessionId } = await send('Target.attachToTarget', { targetId, flatten: true })
      sessions.set(targetId, sessionId)
    }
    const sessionId = sessions.get(targetId)
    const evaluate = async expression => {
      const result = await send('Runtime.evaluate', { expression, awaitPromise: true, returnByValue: true }, sessionId)
      assert.equal(result.exceptionDetails, undefined, JSON.stringify(result.exceptionDetails))
      return result.result.value
    }
    const navigate = async url => {
      const result = await send('Page.navigate', { url }, sessionId)
      assert.equal(result.errorText, undefined, `Navigation to ${url} failed`)
      await waitFor(async () => (await evaluate(`location.href === ${JSON.stringify(url)} && document.readyState === 'complete'`)), url)
    }
    return { targetId, evaluate, navigate }
  }
  const visible = async target => (await target.evaluate('document.visibilityState')) === 'visible'
  const bounds = async target => (await send('Browser.getWindowForTarget', { targetId: target.targetId })).bounds

  // Asks the build's test hook for a step through any tab's title.
  const ask = async (target, action) => {
    request += 1
    await target.evaluate(`document.title = ${JSON.stringify(`myarc-swap:${request}:${action}`)}`)
    await waitFor(() => readBoundedLog(logPath).includes(`myarc-swap test ${request} `), `hook took request ${request}`, 5000)
    return request
  }
  const palette = async (target, token) => {
    const id = await ask(target, `palette:${token}`)
    return waitFor(() => parsePaletteRows(readBoundedLog(logPath), id), `palette rows for request ${id}`)
  }
  const history = async (target, token, back) => {
    await target.navigate(`chrome://history/?q=${token}`)
    const state = await waitFor(async () => {
      const value = await target.evaluate(historyExpression)
      return (value.items.length || value.empty) && value
    }, 'chrome://history results')
    await target.navigate(back)
    return state.items.filter(url => url.includes(token))
  }

  // Swaps the shown window in place and waits until `to` shows instead.
  const swap = async (from, to, action = 'swap') => {
    const before = await bounds(from)
    await ask(from, action)
    if (to) {
      await waitFor(async () => (await visible(to)) && !(await visible(from)), `${action} shows the other Profile`)
      assert.deepEqual(await bounds(to), before, 'Swapped window moved or resized')
    }
  }

  const expectAbsent = async (target, other, fresh) => {
    const probe = await target.evaluate(probeExpression(other))
    if (fresh) {
      assert.equal(probe.documentCookie, '', 'document.cookie is not empty')
      assert.deepEqual(probe.workers, [], 'A service worker is registered')
    }
    assert.equal(probe.cookie, false, `Cookie ${other.cookie} leaked`)
    assert.equal(probe.storage, null, `localStorage ${other.storageKey} leaked`)
    assert.equal(probe.database, false, `IndexedDB ${other.database} leaked`)
    assert.ok(!probe.workers.includes(other.scope), `Service worker ${other.scope} leaked`)
    return probe
  }
  const expectOwn = async (target, own) => {
    const probe = await target.evaluate(probeExpression(own))
    assert.ok(probe.cookie && probe.storage === own.token && probe.database && probe.workers.includes(own.scope),
      `Own data missing: ${JSON.stringify(probe)}`)
    return probe
  }
  // In the shown window: its own unique URL shows in the command bar and
  // history, the other Profile's doesn't.
  const expectSearches = async (target, own, other, label) => {
    const ownRows = rowsNaming(await palette(target, own.token), own.token)
    assert.ok(ownRows.length > 0, `${label}: command bar misses its own URL`)
    const otherRows = rowsNaming(await palette(target, other.token), other.token)
    assert.deepEqual(otherRows, [], `${label}: command bar shows the other Profile's URL`)
    const ownHistory = await history(target, own.token, `${fixtureOrigin}/`)
    assert.ok(ownHistory.length > 0, `${label}: history misses its own URL`)
    const otherHistory = await history(target, other.token, `${fixtureOrigin}/`)
    assert.deepEqual(otherHistory, [], `${label}: history shows the other Profile's URL`)
    record(`${label} searches`, { ownCommandBar: ownRows.length, otherCommandBar: 0, ownHistory: ownHistory.length, otherHistory: 0 })
  }

  let a
  let b
  if (!previous) {
    const initial = await pages()
    assert.equal(initial.length, 1, 'Start from a fresh browser with one fixture tab')
    assert.ok(initial[0].url.startsWith(`${fixtureOrigin}/`), 'Expected the fixture tab')
    a = await page(initial[0].targetId)
    const contextA = initial[0].browserContextId
    await a.navigate(sides.a.url)
    await a.evaluate(seedExpression(sides.a))
    await expectOwn(a, sides.a)
    assert.ok(await visible(a), 'Profile A window is not shown')
    record('A seeded', { url: sides.a.url })
    await expectSearches(a, sides.a, sides.b, 'A before swap')

    // Creating a Space in a new Profile swaps to it; its window has no tab yet.
    await swap(a, null, 'create')
    await waitFor(async () => !(await visible(a)), 'swap to the new Profile')
    await ask(a, 'tab')
    const created = await waitFor(async () => (await pages()).find(info => info.browserContextId !== contextA), 'tab in Profile B')
    b = await page(created.targetId)
    await b.navigate(`${fixtureOrigin}/`)
    assert.ok(await visible(b) && !(await visible(a)), 'Profile B window is not the shown one')
    assert.deepEqual(await bounds(b), await bounds(a), 'Profile B window is not at Profile A window frame')
    record('A to B absent', await expectAbsent(b, sides.a, true))
    // B has no URL of its own yet, so only the absence half applies here.
    const rowsB = rowsNaming(await palette(b, sides.a.token), sides.a.token)
    assert.deepEqual(rowsB, [], 'B command bar shows A URL')
    const historyB = await history(b, sides.a.token, `${fixtureOrigin}/`)
    assert.deepEqual(historyB, [], 'B history shows A URL')
    record('A to B searches', { commandBar: rowsB.length, history: historyB.length })

    await b.navigate(sides.b.url)
    await b.evaluate(seedExpression(sides.b))
    await expectOwn(b, sides.b)
    record('B seeded', { url: sides.b.url })
    await expectSearches(b, sides.b, sides.a, 'B')
    await swap(b, a)
    await a.navigate(`${fixtureOrigin}/`)
    record('B to A absent', await expectAbsent(a, sides.b, false))
    await expectOwn(a, sides.a)
    await expectSearches(a, sides.a, sides.b, 'A after swap back')
  } else {
    const all = await pages()
    const contexts = [...new Set(all.map(info => info.browserContextId))]
    assert.equal(contexts.length, 2, `Expected both Profiles restored, found ${contexts.length} contexts`)
    for (const info of all) {
      if (!info.url.startsWith(`${fixtureOrigin}/`)) continue
      const target = await page(info.targetId)
      const probe = await target.evaluate(probeExpression(sides.a))
      if (probe.cookie && !a) a = target
      else if (!probe.cookie && !b) b = target
    }
    assert.ok(a && b, 'Restored fixture pages for both Profiles not found')
    record('restart A keeps', { own: await expectOwn(a, sides.a) })
    record('restart B keeps', { own: await expectOwn(b, sides.b) })
    record('restart A absent', await expectAbsent(a, sides.b, false))
    record('restart B absent', await expectAbsent(b, sides.a, false))
    // A restored ordered-out window was never shown, so its page still
    // reports visible. The hook names the Profile it acts in instead.
    const shownDir = async () => {
      const id = await ask(a, 'noop')
      return readBoundedLog(logPath).match(new RegExp(`myarc-swap test ${id} noop in (.+)`))[1]
    }
    const restartSwap = async from => {
      await ask(a, 'swap')
      await waitFor(async () => (await shownDir()) !== from, 'swap after relaunch')
      return shownDir()
    }
    const first = await shownDir()
    const [shown, hidden, own, other] = first === 'Default' ? [a, b, sides.a, sides.b] : [b, a, sides.b, sides.a]
    record('restart shown', { profile: first })
    const second = await restartSwap(first)
    await expectSearches(hidden, other, own, `restart ${other.side.toUpperCase()} (${second})`)
    const third = await restartSwap(second)
    assert.equal(third, first, 'Second swap after relaunch did not come back')
    await expectSearches(shown, own, other, `restart ${own.side.toUpperCase()} (${third})`)
  }

  fs.writeFileSync(checkpointPath, JSON.stringify({
    format: 1, browserPid: ownedPid, tokens: { a: sides.a.token, b: sides.b.token }, request
  }) + '\n')
  for (const sessionId of sessions.values()) await send('Target.detachFromTarget', { sessionId }).catch(() => {})
  // Quit like Cmd+Q, then read the files the browser wrote.
  await send('Browser.close').catch(() => {})
  await waitFor(() => {
    try {
      process.kill(ownedPid, 0)
      return false
    } catch {
      return true
    }
  }, 'browser exit', 30000)
  const localState = JSON.parse(fs.readFileSync(`${profile}/Local State`, 'utf8'))
  const files = checkProfileFiles(profile, { a: 'Default', b: otherProfileDir(localState) }, sides)
  record('files after quit', { files })
  console.log(`PASS: ${scenario}: ${results.length} steps, no shared cookies, storage, service workers, history or command bar results`)
  return results
}
