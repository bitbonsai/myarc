// Developer-only two-process probe. Imported without connecting to a browser.
import assert from 'node:assert/strict'
import fs from 'node:fs'
import { randomUUID } from 'node:crypto'
import { setTimeout as delay } from 'node:timers/promises'

export const fixtureURLs = ['http://127.0.0.1:8766/', 'http://127.0.0.1:8766/recovery.html']
export const checkpointName = '.myarc-m0-workspace-crash.json'
const metadataName = 'Default/myarc-workspace-v1.json'
const keys = (value, expected) => assert.deepEqual(Object.keys(value).sort(), expected.split(' ').sort(), 'Unexpected native metadata fields')
const id = value => assert.ok(typeof value === 'string' && /^[1-9]\d{0,19}$/.test(value) && BigInt(value) <= 18446744073709551615n, 'Expected nonzero native uint64 ID')

export const readBoundedText = (path, limit) => {
  const fd = fs.openSync(path, fs.constants.O_RDONLY | fs.constants.O_NOFOLLOW | fs.constants.O_NONBLOCK)
  try {
    assert.ok(fs.fstatSync(fd).isFile(), 'Expected a regular developer fixture file')
    const buffer = Buffer.alloc(limit + 1)
    let size = 0
    while (size < buffer.length) {
      const count = fs.readSync(fd, buffer, size, buffer.length - size)
      if (!count) break
      size += count
    }
    assert.ok(size <= limit, `Fixture read exceeds ${limit} bytes: ${path}`)
    return new TextDecoder('utf-8', { fatal: true }).decode(buffer.subarray(0, size))
  } finally {
    fs.closeSync(fd)
  }
}

export const readBoundedJSON = (path, limit = 65536) => JSON.parse(readBoundedText(path, limit))

// This intentionally accepts only the two-page native journey, not general workspace data.
const projectWorkspace = (state, windowCount) => {
  keys(state, 'version profile_id next_space_id next_item_id next_window_id spaces items windows')
  assert.equal(state.version, 1)
  for (const field of ['profile_id', 'next_space_id', 'next_item_id', 'next_window_id']) id(state[field])
  assert.ok(Array.isArray(state.spaces) && state.spaces.length === 2, 'Require exactly two persisted Spaces')
  assert.ok(Array.isArray(state.items) && state.items.length === 2, 'Require exactly two persisted items')
  assert.ok(Array.isArray(state.windows) && state.windows.length === windowCount, `Require exactly ${windowCount} persisted window(s)`)
  for (const space of state.spaces) {
    keys(space, 'id profile_id name')
    id(space.id)
    assert.equal(space.profile_id, state.profile_id, 'Foreign Space Profile')
    assert.ok(typeof space.name === 'string' && /^[\x20-\x7e]{1,80}$/.test(space.name), 'Use short invented ASCII Space names')
  }
  assert.equal(new Set(state.spaces.map(space => space.id)).size, 2, 'Duplicate Space ID')
  assert.equal(new Set(state.spaces.map(space => space.name)).size, 2, 'Use distinct Space names')
  for (const item of state.items) {
    keys(item, 'id placement owner_id has_url canonical_url')
    id(item.id)
    id(item.owner_id)
  }
  assert.equal(new Set(state.items.map(item => item.id)).size, 2, 'Duplicate item ID')
  const favorite = state.items.find(item => item.placement === 2)
  const temporary = state.items.find(item => item.placement === 0)
  assert.ok(favorite && temporary, 'Require one Profile Favorite and one live Temporary')
  assert.equal(favorite.owner_id, state.profile_id)
  assert.equal(favorite.has_url, true)
  assert.equal(favorite.canonical_url, fixtureURLs[0], 'Favorite must save the original fixture URL')
  assert.equal(temporary.has_url, false)
  assert.equal(temporary.canonical_url, '')
  assert.ok(state.spaces.some(space => space.id === temporary.owner_id), 'Temporary must belong to an existing Space')
  for (const window of state.windows) {
    keys(window, 'id space_id selected_item_id')
    id(window.id)
    assert.ok(state.spaces.some(space => space.id === window.space_id), 'Window references a missing Space')
    assert.equal(window.selected_item_id, favorite.id, 'Select original fixture Favorite before running probe')
  }
  assert.equal(new Set(state.windows.map(window => window.id)).size, windowCount, 'Duplicate stable window ID')
  for (const [field, rows] of [['next_space_id', state.spaces], ['next_item_id', state.items], ['next_window_id', state.windows]]) {
    assert.ok(rows.every(row => BigInt(state[field]) > BigInt(row.id)), 'Native next-ID counter must exceed retained IDs')
  }
  return { version: state.version, profile_id: state.profile_id, spaces: state.spaces, items: state.items, windows: state.windows }
}

export const workspaceProjection = state => projectWorkspace(state, 1)

export const twoWindowWorkspaceProjection = state => {
  const workspace = projectWorkspace(state, 2)
  assert.equal(new Set(workspace.windows.map(window => window.space_id)).size, 2, 'Select a different Space in each window')
  return workspace
}

export const validateOwnership = (args, profile, localState, contexts) => {
  assert.ok(Array.isArray(args) && args.length <= 128 && args.every(arg => typeof arg === 'string' && arg.length <= 8192), 'Unbounded browser command line')
  const flag = name => {
    const matches = args.filter(arg => arg === name || arg.startsWith(`${name}=`))
    assert.equal(matches.length, 1, `Require exactly one browser flag: ${name}`)
    return matches[0]
  }
  assert.equal(flag('--user-data-dir'), `--user-data-dir=${profile}`, 'Browser must own the exact canonical marked directory')
  assert.equal(flag('--profile-directory'), '--profile-directory=Default')
  assert.equal(flag('--remote-debugging-address'), '--remote-debugging-address=127.0.0.1')
  for (const name of ['--enable-automation', '--myarc-visual-parity', '--myarc-workspace-sidebar']) assert.equal(flag(name), name)
  assert.ok(!args.some(arg => /^--(?:incognito|guest)(?:=|$)/.test(arg)), 'Regular synthetic Profile required')
  // Upstream GetLastUsedProfileBaseName falls back to Default for absent/empty last_used.
  assert.ok([undefined, '', 'Default'].includes(localState.profile?.last_used), 'Persisted last-used Profile must resolve to Default')
  assert.deepEqual(Object.keys(localState.profile?.info_cache ?? {}), ['Default'], 'Require a single owned regular Profile')
  assert.deepEqual(contexts.browserContextIds, [], 'Close all developer-created private contexts first')
  assert.ok(typeof contexts.defaultBrowserContextId === 'string' && contexts.defaultBrowserContextId.length > 0, 'Native default Profile context unavailable')
  return contexts.defaultBrowserContextId
}

export const validateTargets = (targets, contextId) => {
  assert.ok(Array.isArray(targets) && targets.length <= 32, 'Native target list exceeds narrow fixture bound')
  const pages = targets.filter(target => target.type === 'page')
  assert.ok(pages.length <= 2, 'Extra page targets: close startup/NTP/duplicate tabs before probing')
  for (const page of pages) {
    assert.ok(fixtureURLs.includes(page.url), 'Unexpected page target: only the two exact synthetic fixture URLs are allowed')
    assert.equal(page.browserContextId, contextId, 'Page belongs to a foreign/private Profile context')
  }
  assert.equal(new Set(pages.map(page => page.url)).size, pages.length, 'Duplicate fixture URL target')
  return pages
}

export const validateCheckpointIdentity = checkpoint => {
  assert.equal(checkpoint.format, 1)
  assert.ok(Number.isSafeInteger(checkpoint.browserPid) && checkpoint.browserPid > 0)
  assert.deepEqual(checkpoint.expectedURLs, fixtureURLs)
  assert.ok(typeof checkpoint.profilePath === 'string')
  assert.ok(typeof checkpoint.recordedAt === 'string' && Number.isFinite(Date.parse(checkpoint.recordedAt)))
  keys(checkpoint.build, 'product revision protocolVersion jsVersion executable executableSize executableMtimeNs')
  assert.ok(Object.values(checkpoint.build).every(value => typeof value === 'string' && value.length > 0 && value.length <= 4096))
  return checkpoint
}

export const validateCheckpoint = checkpoint => {
  keys(checkpoint, 'format browserPid build profilePath expectedURLs canary metadata projection recordedAt')
  validateCheckpointIdentity(checkpoint)
  assert.match(checkpoint.canary, /^M0 crash canary [0-9a-f-]{36}$/)
  workspaceProjection(checkpoint.metadata)
  assert.deepEqual(checkpoint.projection, { visibleURL: fixtureURLs[0], hiddenURL: fixtureURLs[1], nativeWindowCount: 1 })
  return checkpoint
}

export const openWorkspaceObservation = async ({ send, profile }) => {
  assert.equal(fs.realpathSync(profile), profile, 'Use the canonical marked profile path')
  readBoundedText(`${profile}/.myarc-m0-test-profile`, 256)
  const deadline = Date.now() + 20000
  const cdp = (method, params = {}, sessionId) => {
    const remaining = deadline - Date.now()
    assert.ok(remaining > 0, 'Recovery probe exceeded 20-second observation budget; complete native Restore/setup and retry')
    return send(method, params, sessionId, Math.min(3000, remaining))
  }
  assert.equal(fs.realpathSync(`${profile}/Default`), `${profile}/Default`, 'Profile directory must not redirect elsewhere')
  const args = (await cdp('Browser.getBrowserCommandLine')).arguments
  const localState = () => readBoundedJSON(`${profile}/Local State`, 1048576)
  const contextId = validateOwnership(args, profile, localState(), await cdp('Target.getBrowserContexts'))
  assert.equal(readBoundedJSON(`${profile}/Default/Preferences`, 1048576).vertical_tabs?.enabled, true,
    'Seed vertical_tabs.enabled before native startup; workspace adapter must be constructed')
  const version = await cdp('Browser.getVersion')
  const executable = fs.realpathSync(args[0])
  const binary = fs.statSync(executable, { bigint: true })
  assert.ok(binary.isFile() && binary.size > 0n, 'Expected the owned browser executable')
  const build = { product: version.product, revision: version.revision, protocolVersion: version.protocolVersion,
    jsVersion: version.jsVersion, executable, executableSize: String(binary.size), executableMtimeNs: String(binary.mtimeNs) }
  const evaluatePage = async (targetId, expression) => {
    const { sessionId } = await cdp('Target.attachToTarget', { targetId, flatten: true })
    try {
      const result = await cdp('Runtime.evaluate', { expression, awaitPromise: true, returnByValue: true }, sessionId)
      assert.equal(result.exceptionDetails, undefined, 'Synthetic page evaluation failed')
      return result.result.value
    } finally {
      await cdp('Target.detachFromTarget', { sessionId })
    }
  }
  const scopedTargets = async () => {
    const currentContext = validateOwnership(args, profile, localState(), await cdp('Target.getBrowserContexts'))
    assert.equal(currentContext, contextId, 'Default Profile context changed during probe')
    return (await cdp('Target.getTargets')).targetInfos
  }
  return { cdp, deadline, build, contextId, evaluatePage, scopedTargets }
}

export const runWorkspaceRecovery = async ({ send, profile, ownedPid, scenario }) => {
  assert.ok(['workspace-crash-checkpoint', 'workspace-crash-verify'].includes(scenario), 'Unknown recovery phase')
  const { cdp, deadline, build, contextId, evaluatePage, scopedTargets } = await openWorkspaceObservation({ send, profile })
  const verifying = scenario === 'workspace-crash-verify'
  const path = `${profile}/${checkpointName}`
  const previous = verifying ? validateCheckpoint(readBoundedJSON(path)) : null
  if (previous) {
    assert.notEqual(previous.browserPid, ownedPid, 'Browser PID unchanged: parent must crash/relaunch only the owned disposable browser')
    assert.equal(previous.profilePath, `${profile}/Default`, 'Checkpoint belongs to another Profile')
    assert.deepEqual(build, previous.build, 'Restored browser build/executable differs from checkpoint')
  } else {
    assert.ok(!fs.existsSync(path), 'Checkpoint already exists; preserve it for verify or explicitly remove it before a new journey')
  }
  const canary = previous?.canary ?? `M0 crash canary ${randomUUID()}`
  const scopedPages = async () => validateTargets(await scopedTargets(), contextId)
  const observe = async pages => {
    if (pages.length !== 2) throw new Error('Restore not ready: require both real fixture pages; parent may need native Restore')
    const metadata = readBoundedJSON(`${profile}/${metadataName}`)
    const workspace = workspaceProjection(metadata)
    if (previous) assert.deepEqual(workspace, workspaceProjection(previous.metadata), 'Workspace loss/rebind: stable IDs, ownership, names or window selection differ')
    const windows = []
    for (const url of fixtureURLs) {
      const page = pages.find(page => page.url === url)
      windows.push((await cdp('Browser.getWindowForTarget', { targetId: page.targetId })).windowId)
      const observed = await evaluatePage(page.targetId, `({ url: location.href.slice(0, 256), title: document.title.slice(0, 81), ready: document.readyState,
        visibility: document.visibilityState, form: Boolean(document.querySelector('#form #note')),
        canary: localStorage.getItem('myarc-test-note')?.slice(0, 81), result: document.querySelector('#result')?.textContent.slice(0, 100) })`)
      assert.equal(observed.url, url, 'Fixture navigated during observation')
      assert.equal(observed.ready, 'complete', 'Restored fixture still loading')
      assert.equal(observed.title, url === fixtureURLs[0] ? 'myArc foundation check' : 'myArc recovery second page')
      assert.equal(observed.visibility, url === fixtureURLs[0] ? 'visible' : 'hidden', 'Native page selection/projection differs')
      if (url === fixtureURLs[0]) {
        assert.equal(observed.form, true, 'Original fixture form missing')
        if (verifying || submitted) assert.equal(observed.canary, canary, 'Acknowledged localStorage canary lost')
        if (!verifying && submitted) assert.equal(observed.result, `Saved: ${canary}`, 'Canary UI acknowledgement missing')
      }
    }
    assert.ok(windows.every(window => Number.isInteger(window) && window === windows[0]), 'Both pages must share one actual native window')
    return { metadata, workspace }
  }
  let submitted = false
  if (!verifying) {
    await observe(await scopedPages())
    const pages = await scopedPages()
    await evaluatePage(pages.find(page => page.url === fixtureURLs[0]).targetId, `(() => {
      if (location.href !== ${JSON.stringify(fixtureURLs[0])} || document.title !== 'myArc foundation check') throw new Error('Fixture changed')
      document.querySelector('#note').value = ${JSON.stringify(canary)}
      document.querySelector('#form').requestSubmit()
    })()`)
    submitted = true
  }
  const settleDeadline = Math.min(deadline - 1000, Date.now() + 10000)
  let settled = 0
  let lastError
  let retained
  while (Date.now() < settleDeadline && settled < 5) {
    // Scope failures abort immediately, even while a genuine restore is settling.
    const pages = await scopedPages()
    try {
      const observation = await observe(pages)
      assert.ok(Date.now() <= settleDeadline, 'Observation completed after the ten-second settle deadline')
      if (retained) assert.deepEqual(observation.workspace, retained.workspace, 'Workspace changed during settlement')
      retained = observation
      settled++
      lastError = undefined
    } catch (error) {
      if (!verifying) throw error
      settled = 0
      retained = undefined
      lastError = error
    }
    if (settled < 5) await delay(200)
  }
  assert.equal(settled, 5, `Restore not ready or recovery loss within 10-second settle budget: ${lastError?.message ?? 'state never settled'}. Complete genuine native Restore, close startup tabs and retry; do not rewrite workspace metadata.`)
  const processes = await cdp('SystemInfo.getProcessInfo')
  assert.ok(processes.processInfo.some(process => process.type === 'browser' && process.id === ownedPid), 'Owned browser PID changed during probe')
  if (!verifying) {
    const checkpoint = validateCheckpoint({ format: 1, browserPid: ownedPid, build, profilePath: `${profile}/Default`,
      expectedURLs: fixtureURLs, canary, metadata: retained.metadata,
      projection: { visibleURL: fixtureURLs[0], hiddenURL: fixtureURLs[1], nativeWindowCount: 1 }, recordedAt: new Date().toISOString() })
    fs.writeFileSync(path, `${JSON.stringify(checkpoint, null, 2)}\n`, { flag: 'wx', mode: 0o600 })
  }
  console.log(JSON.stringify({ scenario, browserPid: ownedPid, previousBrowserPid: previous?.browserPid,
    browser: build.product, workspace: retained.workspace, expectedURLs: fixtureURLs, stableSamples: settled }))
  console.log(verifying
    ? 'PASS: changed-process restored pages, canary and retained workspace IDs/ownership/selection match checkpoint; parent owns crash provenance'
    : 'PASS: observed native pages, acknowledged canary and persisted workspace checkpointed; parent owns crash/relaunch/Restore')
}
