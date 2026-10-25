// Local fixture/argument checks only. API doubles below prove no native browser behavior.
import assert from 'node:assert/strict'
import { createHash } from 'node:crypto'
import fs from 'node:fs'
import os from 'node:os'
import { checkpointName, fixtureURLs, readBoundedJSON, runWorkspaceRecovery, validateCheckpoint, validateOwnership,
  validateTargets, workspaceProjection } from './browser_workspace_recovery.mjs'
import { spawnSync } from 'node:child_process'
import vm from 'node:vm'
import test from 'node:test'

const root = new URL('../', import.meta.url)
const fixture = new URL('../tests/fixtures/browser/workspace-pin-extension/', import.meta.url)
const manifest = JSON.parse(fs.readFileSync(new URL('manifest.json', fixture), 'utf8'))
const workerSource = fs.readFileSync(new URL('worker.js', fixture), 'utf8')
const script = new vm.Script(workerSource)
const fixtureTab = { id: 7, windowId: 1, url: 'http://127.0.0.1:8766/', pinned: false, incognito: false, active: true }
const loadWorker = (tabs = [fixtureTab], updateError) => {
  const calls = []
  const context = vm.createContext({ chrome: { tabs: {
    query: async query => {
      assert.equal(query.url, fixtureTab.url)
      return tabs
    },
    update: async (id, changes) => {
      calls.push({ id, pinned: changes.pinned, keys: Object.keys(changes) })
      if (updateError) throw updateError
    }
  } } })
  script.runInContext(context, { timeout: 1000 })
  assert.equal(calls.length, 0, 'Loading worker must not mutate any tab')
  return { context, calls }
}

test('fixture has stable public identity and only tabs permission', () => {
  assert.equal(manifest.manifest_version, 3)
  assert.equal(manifest.version, '1.0.0')
  assert.deepEqual(manifest.permissions, ['tabs'])
  assert.deepEqual(manifest.background, { service_worker: 'worker.js' })
  assert.deepEqual(Object.keys(manifest).sort(), ['background', 'key', 'manifest_version', 'name', 'permissions', 'version'])
  assert.equal(createHash('sha256').update(Buffer.from(manifest.key, 'base64')).digest('hex').slice(0, 32)
    .replace(/[0-9a-f]/g, digit => String.fromCharCode(97 + parseInt(digit, 16))), 'faokaoibbnjigecocmhniflfnkpmacle')
})

test('fixture forwards explicit Pin and Unpin to tabs.update, propagating API failure', async () => {
  const { context, calls } = loadWorker()
  await context.myarcWorkspaceFixturePin(7, true)
  await context.myarcWorkspaceFixturePin(7, false)
  assert.deepEqual(calls, [
    { id: 7, pinned: true, keys: ['pinned'] },
    { id: 7, pinned: false, keys: ['pinned'] }
  ])
  const failed = loadWorker([fixtureTab], new Error('API rejected'))
  await assert.rejects(failed.context.myarcWorkspaceFixturePin(7, true), /API rejected/)
})

test('fixture rejects ambiguous, navigated, private, inactive and changed tabs before mutation', async () => {
  for (const tabs of [[], [fixtureTab, fixtureTab],
    [{ ...fixtureTab, url: 'https://example.com/' }],
    [{ ...fixtureTab, pendingUrl: 'http://127.0.0.1:8766/other' }],
    [{ ...fixtureTab, incognito: true }], [{ ...fixtureTab, active: false }],
    [{ ...fixtureTab, id: -1 }], [{ ...fixtureTab, id: 8 }]]) {
    const { context, calls } = loadWorker(tabs)
    await assert.rejects(context.myarcWorkspaceFixturePin(7, true))
    assert.equal(calls.length, 0)
  }
  const { context, calls } = loadWorker()
  for (const [id, pinned] of [['7', true], [-1, true], [NaN, true], [7, 0], [7, 'false']]) {
    await assert.rejects(context.myarcWorkspaceFixturePin(id, pinned), /explicit tab ID and boolean/)
  }
  assert.equal(calls.length, 0)
})

test('CLI preserves existing modes and rejects unsafe arguments before connecting', () => {
  const run = args => spawnSync(process.execPath, ['tools/check_browser_foundation.mjs', ...args], {
    cwd: root, encoding: 'utf8', timeout: 5000, maxBuffer: 65536
  })
  for (const args of [[], ['.', 'not-a-pid', 'workspace-pin'], ['.', '1', 'unknown']]) {
    const result = run(args)
    assert.equal(result.status, 1)
    assert.match(result.stderr, /Usage:.*extension\|restart\|runtime\|workspace-pin/)
  }
  for (const pid of ['0', '9007199254740992']) {
    const result = run(['.', pid, 'workspace-crash-checkpoint'])
    assert.equal(result.status, 1)
    assert.match(result.stderr, /positive safe-integer browser PID/)
  }
  assert.match(run(['.', '1', 'runtime', 'unexpected']).stderr, /Usage:/)
  for (const mode of ['extension', 'restart', 'runtime', 'workspace-pin', 'workspace-crash-checkpoint', 'workspace-crash-verify',
    'workspace-two-window-checkpoint', 'workspace-two-window-verify']) {
    const result = run(['.', '1', mode])
    assert.equal(result.status, 1)
    assert.match(result.stderr, /Expected marked, disposable test profile/)
  }
})

const nativeState = () => ({
  version: 1, profile_id: '1', next_space_id: '3', next_item_id: '3', next_window_id: '2',
  spaces: [{ id: '1', profile_id: '1', name: 'Everyday' }, { id: '2', profile_id: '1', name: 'Reading' }],
  items: [{ id: '1', placement: 2, owner_id: '1', has_url: true, canonical_url: fixtureURLs[0] },
    { id: '2', placement: 0, owner_id: '2', has_url: false, canonical_url: '' }],
  windows: [{ id: '1', space_id: '1', selected_item_id: '1' }]
})
const localState = { profile: { last_used: 'Default', info_cache: { Default: {} } } }
const contexts = { browserContextIds: [], defaultBrowserContextId: 'native-default' }
const browserArgs = profile => [process.execPath, `--user-data-dir=${profile}`, '--profile-directory=Default',
  '--remote-debugging-address=127.0.0.1', '--enable-automation', '--myarc-visual-parity', '--myarc-workspace-sidebar']
const pageTargets = contextId => fixtureURLs.map((url, index) => ({ type: 'page', url, targetId: `page-${index}`, browserContextId: contextId }))
const scratch = t => {
  // Wholly invented JSON in a test temp directory, never an existing browser profile.
  const dir = fs.realpathSync(fs.mkdtempSync(`${os.tmpdir()}/myarc-client-unit-`))
  t.after(() => fs.rmSync(dir, { recursive: true, force: true }))
  return dir
}

test('recovery accepts observed native schema and rejects ownership, saved identity and selection mistakes', () => {
  assert.deepEqual(workspaceProjection(nativeState()).items, nativeState().items)
  const mutations = [
    state => { state.version = 2 },
    state => { state.extra = true },
    state => { state.spaces.pop() },
    state => { state.spaces[0].profile_id = '8' },
    state => { state.spaces[1].id = '1' },
    state => { state.spaces[1].name = 'Everyday' },
    state => { state.spaces[1].name = 'x'.repeat(81) },
    state => { state.items.pop() },
    state => { state.items[1].id = '1' },
    state => { state.items[0].id = '18446744073709551616' },
    state => { state.items[0].owner_id = '2' },
    state => { state.items[0].canonical_url = fixtureURLs[1] },
    state => { state.items[0].placement = 1 },
    state => { state.items[1].has_url = true },
    state => { state.items[1].canonical_url = fixtureURLs[1] },
    state => { state.items[1].owner_id = '9' },
    state => { state.windows[0].selected_item_id = '2' },
    state => { state.windows[0].space_id = '9' },
    state => { state.windows.push(state.windows[0]) },
    state => { state.next_item_id = '2' }
  ]
  for (const mutate of mutations) {
    const state = nativeState()
    mutate(state)
    assert.throws(() => workspaceProjection(state))
  }
})

test('recovery uses exact browser flags, one regular Profile and real default context', () => {
  assert.equal(validateOwnership(browserArgs('/owned'), '/owned', localState, contexts), 'native-default')
  for (const last_used of [undefined, '']) {
    assert.equal(validateOwnership(browserArgs('/owned'), '/owned', { profile: { ...localState.profile, last_used } }, contexts), 'native-default')
  }
  for (const args of [browserArgs('/foreign'), browserArgs('/owned').filter(arg => arg !== '--enable-automation'),
    [...browserArgs('/owned'), '--user-data-dir=/owned'], [...browserArgs('/owned'), '--incognito'],
    browserArgs('/owned').map(arg => arg.replace('127.0.0.1', '0.0.0.0'))]) {
    assert.throws(() => validateOwnership(args, '/owned', localState, contexts))
  }
  for (const state of [{}, { profile: { ...localState.profile, last_used: 'Profile 1' } },
    { profile: { ...localState.profile, info_cache: { Default: {}, 'Profile 1': {} } } }]) {
    assert.throws(() => validateOwnership(browserArgs('/owned'), '/owned', state, contexts))
  }
  for (const scope of [{ browserContextIds: [] }, { ...contexts, browserContextIds: ['private'] }]) {
    assert.throws(() => validateOwnership(browserArgs('/owned'), '/owned', localState, scope))
  }
})

test('recovery rejects unknown, duplicate, foreign and excessive native page targets', () => {
  assert.equal(validateTargets(pageTargets('native-default'), 'native-default').length, 2)
  assert.equal(validateTargets([], 'native-default').length, 0, 'Missing pages can settle, never pass as a subset')
  const pages = pageTargets('native-default')
  for (const targets of [[...pages, pages[0]], [{ ...pages[0], url: 'chrome://newtab/' }],
    [{ ...pages[0], url: 'http://127.0.0.1:8766/?unexpected' }], [{ ...pages[0], browserContextId: 'private' }],
    [pages[0], { ...pages[1], url: pages[0].url }], Array(33).fill({ type: 'service_worker' })]) {
    assert.throws(() => validateTargets(targets, 'native-default'))
  }
})

test('developer JSON reads reject oversize, malformed UTF-8, directories and symlinks', t => {
  const dir = scratch(t)
  const file = `${dir}/metadata.json`
  fs.writeFileSync(file, '{"ok":true}')
  assert.deepEqual(readBoundedJSON(file, 11), { ok: true })
  assert.throws(() => readBoundedJSON(file, 10), /exceeds 10 bytes/)
  fs.symlinkSync(file, `${dir}/link`)
  assert.throws(() => readBoundedJSON(`${dir}/link`), /ELOOP/)
  assert.throws(() => readBoundedJSON(dir), /regular developer fixture file/)
  fs.writeFileSync(file, Buffer.from([0xff]))
  assert.throws(() => readBoundedJSON(file), /encoded data/)
  fs.writeFileSync(file, '{')
  assert.throws(() => readBoundedJSON(file), SyntaxError)
})

const fakeNative = (t, pid = 10) => {
  t.mock.method(console, 'log', () => {})
  const profile = scratch(t)
  fs.mkdirSync(`${profile}/Default`)
  fs.writeFileSync(`${profile}/.myarc-m0-test-profile`, 'Invented local unit test only\n')
  fs.writeFileSync(`${profile}/Local State`, JSON.stringify(localState))
  fs.writeFileSync(`${profile}/Default/Preferences`, JSON.stringify({ vertical_tabs: { enabled: true } }))
  const metadataPath = `${profile}/Default/myarc-workspace-v1.json`
  fs.writeFileSync(metadataPath, JSON.stringify(nativeState()))
  const storage = new Map()
  const calls = []
  const fake = { profile, pid, storage, calls, metadataPath, revision: '@invented-unit-test', pages: null }
  const documents = fixtureURLs.map((url, index) => {
    const note = { value: '' }
    const result = { textContent: 'Nothing saved' }
    const form = { requestSubmit: () => {
      storage.set('myarc-test-note', note.value)
      result.textContent = `Saved: ${note.value}`
    } }
    return { location: { href: url }, localStorage: { getItem: key => storage.get(key) ?? null }, document: {
      title: index === 0 ? 'myArc foundation check' : 'myArc recovery second page', readyState: 'complete',
      visibilityState: index === 0 ? 'visible' : 'hidden',
      querySelector: selector => index === 0 ? ({ '#note': note, '#result': result, '#form': form, '#form #note': note }[selector] ?? null) : null
    } }
  })
  fake.send = async (method, params, sessionId, timeout) => {
    calls.push(method)
    assert.ok(timeout > 0 && timeout <= 3000)
    const contextId = `context-${fake.pid}`
    const pages = fake.pages ?? pageTargets(contextId).map((page, index) => ({ ...page, targetId: `target-${fake.pid}-${index}` }))
    switch (method) {
      case 'Browser.getBrowserCommandLine': return { arguments: browserArgs(profile) }
      case 'Target.getBrowserContexts': return { browserContextIds: [], defaultBrowserContextId: contextId }
      case 'Browser.getVersion': return { product: 'Chrome/unit-test', revision: fake.revision, protocolVersion: 'test', jsVersion: 'test' }
      case 'Target.getTargets': return { targetInfos: pages }
      case 'Browser.getWindowForTarget': return { windowId: fake.pid + 100 }
      case 'Target.attachToTarget': return { sessionId: params.targetId }
      case 'Target.detachFromTarget': return {}
      case 'Runtime.evaluate': {
        const index = Number(sessionId.split('-').at(-1))
        const value = vm.runInNewContext(params.expression, documents[index], { timeout: 1000 })
        return { result: { value } }
      }
      case 'SystemInfo.getProcessInfo': return { processInfo: [{ type: 'browser', id: fake.pid }] }
      default: throw new Error(`Forbidden/unexpected native command in API double: ${method}`)
    }
  }
  fake.run = scenario => runWorkspaceRecovery({ send: fake.send, profile, ownedPid: fake.pid, scenario })
  return fake
}

test('API double: checkpoint and changed-process verify compare stable IDs, leave metadata untouched and never navigate', async t => {
  const fake = fakeNative(t)
  const before = fs.readFileSync(fake.metadataPath, 'utf8')
  await fake.run('workspace-crash-checkpoint')
  const checkpointPath = `${fake.profile}/${checkpointName}`
  const checkpoint = validateCheckpoint(readBoundedJSON(checkpointPath))
  assert.equal(checkpoint.browserPid, 10)
  assert.equal(checkpoint.canary, fake.storage.get('myarc-test-note'))
  assert.equal(fs.statSync(checkpointPath).mode & 0o777, 0o600)
  assert.deepEqual(checkpoint.metadata, nativeState())
  await assert.rejects(fake.run('workspace-crash-checkpoint'), /Checkpoint already exists/)
  await assert.rejects(fake.run('workspace-crash-verify'), /Browser PID unchanged/)
  fake.pid = 20
  fake.revision = '@other-build'
  await assert.rejects(fake.run('workspace-crash-verify'), /build\/executable differs/)
  fake.revision = '@invented-unit-test'
  // Native target/context/window IDs all change in the double; persistent IDs must not.
  await fake.run('workspace-crash-verify')
  assert.equal(fs.readFileSync(fake.metadataPath, 'utf8'), before)
  assert.equal(fs.readFileSync(checkpointPath, 'utf8'), `${JSON.stringify(checkpoint, null, 2)}\n`)
  const attached = fake.calls.filter(method => method === 'Target.attachToTarget').length
  assert.equal(fake.calls.filter(method => method === 'Target.detachFromTarget').length, attached)
  assert.ok(!fake.calls.some(method => /navigate|createTarget|crash|close|reload|startWorker/i.test(method)))
  for (const mutate of [value => { value.canary = 'personal data' }, value => { value.expectedURLs.pop() },
    value => { value.projection.visibleURL = fixtureURLs[1] }, value => { value.metadata.items[0].owner_id = '9' }]) {
    const changed = structuredClone(checkpoint)
    mutate(changed)
    assert.throws(() => validateCheckpoint(changed))
  }
})

test('API double: missing renderer prerequisite or unexpected page aborts before canary/checkpoint write', async t => {
  const fake = fakeNative(t)
  const preferences = `${fake.profile}/Default/Preferences`
  fs.writeFileSync(preferences, '{}')
  await assert.rejects(fake.run('workspace-crash-checkpoint'), /Seed vertical_tabs.enabled/)
  fs.writeFileSync(preferences, '{"vertical_tabs":{"enabled":true}}')
  fake.pages = [{ ...pageTargets(`context-${fake.pid}`)[0], url: 'https://example.com/' }]
  await assert.rejects(fake.run('workspace-crash-checkpoint'), /Unexpected page target/)
  assert.equal(fake.storage.size, 0)
  assert.equal(fs.existsSync(`${fake.profile}/${checkpointName}`), false)
})

test('API double: verify fails on lost canary within bounded restore budget', { timeout: 20000 }, async t => {
  const fake = fakeNative(t)
  await fake.run('workspace-crash-checkpoint')
  fake.pid = 20
  fake.storage.clear()
  await assert.rejects(fake.run('workspace-crash-verify'), /Restore not ready or recovery loss.*localStorage canary lost/s)
})

test('API double: verify rejects persistent ID rebind even when restored URLs match', { timeout: 20000 }, async t => {
  const fake = fakeNative(t)
  await fake.run('workspace-crash-checkpoint')
  fake.pid = 20
  const changed = nativeState()
  changed.items[0].id = '3'
  changed.windows[0].selected_item_id = '3'
  changed.next_item_id = '4'
  fs.writeFileSync(fake.metadataPath, JSON.stringify(changed))
  await assert.rejects(fake.run('workspace-crash-verify'), /Restore not ready or recovery loss.*Workspace loss\/rebind/s)
})

test('API double: verify never accepts a subset of restored pages', { timeout: 20000 }, async t => {
  const fake = fakeNative(t)
  await fake.run('workspace-crash-checkpoint')
  fake.pid = 20
  fake.pages = [pageTargets('context-20')[0]]
  await assert.rejects(fake.run('workspace-crash-verify'), /Restore not ready or recovery loss.*require both real fixture pages/s)
})
