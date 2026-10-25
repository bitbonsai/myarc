// Synthetic files and CDP/extension API doubles only. Zero native launches.
import assert from 'node:assert/strict'
import fs from 'node:fs'
import os from 'node:os'
import vm from 'node:vm'
import test from 'node:test'
import { fixtureURLs, readBoundedJSON, twoWindowWorkspaceProjection } from './browser_workspace_recovery.mjs'
import { runTwoWindowRecovery, twoWindowCheckpointName, validateTwoWindowCheckpoint,
  validateTwoWindowTabs, validateTwoWindowTargets } from './browser_workspace_two_window.mjs'

const state = () => ({
  version: 1, profile_id: '1', next_space_id: '3', next_item_id: '3', next_window_id: '3',
  spaces: [{ id: '1', profile_id: '1', name: 'Everyday' }, { id: '2', profile_id: '1', name: 'Reading' }],
  items: [{ id: '1', placement: 2, owner_id: '1', has_url: true, canonical_url: fixtureURLs[0] },
    { id: '2', placement: 0, owner_id: '2', has_url: false, canonical_url: '' }],
  windows: [{ id: '1', space_id: '1', selected_item_id: '1' }, { id: '2', space_id: '2', selected_item_id: '1' }]
})
const pages = (pid = 10) => [fixtureURLs[0], fixtureURLs[0], fixtureURLs[1]].map((url, index) => ({
  type: 'page', targetId: `page-${pid}-${index}`, url, browserContextId: `context-${pid}`
}))
const nativeTabs = pid => pages(pid).map((page, index) => ({ id: pid * 10 + index, windowId: pid + (index === 1 ? 101 : 100),
  url: page.url, active: index < 2, pinned: index < 2, incognito: false }))
const scratch = t => {
  const profile = fs.realpathSync(fs.mkdtempSync(`${os.tmpdir()}/myarc-two-window-unit-`))
  t.after(() => fs.rmSync(profile, { recursive: true, force: true }))
  return profile
}
const fakeNative = t => {
  t.mock.method(console, 'log', () => {})
  const profile = scratch(t)
  fs.mkdirSync(`${profile}/Default`)
  fs.writeFileSync(`${profile}/.myarc-m0-test-profile`, 'API double only\n')
  fs.writeFileSync(`${profile}/Local State`, JSON.stringify({ profile: { info_cache: { Default: {} } } }))
  fs.writeFileSync(`${profile}/Default/Preferences`, '{"vertical_tabs":{"enabled":true}}')
  const metadataPath = `${profile}/Default/myarc-workspace-v1.json`
  fs.writeFileSync(metadataPath, JSON.stringify(state()))
  const manifest = JSON.parse(fs.readFileSync(new URL('../tests/fixtures/browser/workspace-pin-extension/manifest.json', import.meta.url), 'utf8'))
  const workerId = 'faokaoibbnjigecocmhniflfnkpmacle'
  const workerURL = `chrome-extension://${workerId}/worker.js`
  const fake = { profile, metadataPath, pid: 10, calls: [], targets: null, tabs: null, contexts: [], revision: 'invented-test' }
  fake.send = async (method, params, sessionId, timeout) => {
    fake.calls.push({ method, params, sessionId })
    assert.ok(timeout > 0 && timeout <= 3000)
    switch (method) {
      case 'Browser.getBrowserCommandLine': return { arguments: [process.execPath, `--user-data-dir=${profile}`,
        '--profile-directory=Default', '--remote-debugging-address=127.0.0.1', '--enable-automation',
        '--myarc-visual-parity', '--myarc-workspace-sidebar'] }
      case 'Browser.getVersion': return { product: 'Chrome/unit-test', revision: fake.revision, protocolVersion: 'test', jsVersion: 'test' }
      case 'Target.getBrowserContexts': return { browserContextIds: fake.contexts, defaultBrowserContextId: `context-${fake.pid}` }
      case 'Target.getTargets': return { targetInfos: [...(fake.targets ?? pages(fake.pid)), {
        type: 'service_worker', targetId: 'fixture-worker', url: workerURL, browserContextId: `context-${fake.pid}` }] }
      case 'Target.attachToTarget': return { sessionId: params.targetId }
      case 'Target.detachFromTarget': return {}
      case 'ServiceWorker.enable': case 'ServiceWorker.disable': return {}
      case 'ServiceWorker.startWorker':
        assert.equal(params.scopeURL, `chrome-extension://${workerId}/`)
        return {}
      case 'Browser.getWindowForTarget': {
        const index = Number(params.targetId.split('-').at(-1))
        return { windowId: fake.pid + (index === 1 ? 101 : 100) }
      }
      case 'Runtime.evaluate': {
        let context
        if (sessionId === 'fixture-worker') {
          context = { location: { href: workerURL }, chrome: { runtime: { id: workerId, getManifest: () => manifest },
            tabs: { query: async query => { assert.equal(Object.keys(query).length, 0); return fake.tabs ?? nativeTabs(fake.pid) } } } }
        } else {
          const index = Number(sessionId.split('-').at(-1))
          const isFavorite = index < 2
          context = { location: { href: isFavorite ? fixtureURLs[0] : fixtureURLs[1] }, document: {
            title: isFavorite ? 'myArc foundation check' : 'myArc recovery second page', readyState: 'complete',
            visibilityState: isFavorite ? 'visible' : 'hidden', querySelector: selector => {
              assert.equal(selector, '#form #note', 'Observation must never submit/edit the form')
              return isFavorite ? {} : null
            }
          } }
        }
        const value = await vm.runInNewContext(params.expression, context, { timeout: 1000 })
        // JSON serialization models CDP returnByValue across the VM boundary.
        return { result: { value: JSON.parse(JSON.stringify(value)) } }
      }
      case 'SystemInfo.getProcessInfo': return { processInfo: [{ type: 'browser', id: fake.pid }] }
      default: throw new Error(`Forbidden native operation in API double: ${method}`)
    }
  }
  fake.run = phase => runTwoWindowRecovery({ send: fake.send, profile, ownedPid: fake.pid, scenario: `workspace-two-window-${phase}` })
  return fake
}

test('two-window schema keeps one logical Favorite with two selections and distinct Spaces', () => {
  assert.deepEqual(twoWindowWorkspaceProjection(state()).windows, state().windows)
  for (const mutate of [value => value.windows.pop(), value => { value.windows[1].id = '1' },
    value => { value.windows[1].space_id = '1' }, value => { value.windows[1].space_id = '9' },
    value => { value.windows[1].selected_item_id = '2' }, value => { value.items[0].owner_id = '2' },
    value => { value.items[0].canonical_url = fixtureURLs[1] }, value => value.items.push({ ...value.items[0], id: '3' }),
    value => { value.next_window_id = '2' }, value => { value.windows[0].id = '18446744073709551616' }]) {
    const value = state()
    mutate(value)
    assert.throws(() => twoWindowWorkspaceProjection(value))
  }
})

test('two-window targets allow two Favorite pages only, bound startup NTP and reject foreign contexts', () => {
  assert.equal(validateTwoWindowTargets(pages(), 'context-10').length, 3)
  const ntp = { ...pages()[0], targetId: 'ntp', url: 'chrome://newtab/' }
  assert.equal(validateTwoWindowTargets([...pages(), ntp], 'context-10', true).length, 4)
  assert.throws(() => validateTwoWindowTargets([...pages(), ntp], 'context-10'))
  for (const targets of [[...pages(), pages()[0]], [...pages(), { ...pages()[0], targetId: 'duplicate-favorite' }],
    [...pages(), { ...ntp, url: 'about:blank' }], [...pages(), ntp, { ...ntp, targetId: 'ntp2' }],
    [{ ...pages()[0], browserContextId: 'foreign' }],
    [...pages(), { type: 'service_worker', browserContextId: 'foreign' }], Array(33).fill({ type: 'browser' })]) {
    assert.throws(() => validateTwoWindowTargets(targets, 'context-10', true))
  }
})

test('two-window native readback requires both pins, two windows and exact CDP/tab projection agreement', () => {
  const nativePages = nativeTabs(10).map(({ url, windowId }) => ({ url, windowId }))
  assert.equal(validateTwoWindowTabs(nativeTabs(10), nativePages).favoritePinned, true)
  for (const mutate of [tabs => tabs.pop(), tabs => { tabs[1].windowId = tabs[0].windowId },
    tabs => { tabs[0].pinned = false }, tabs => { tabs[2].pinned = true }, tabs => { tabs[1].active = false },
    tabs => { tabs[1].incognito = true }, tabs => { tabs[1].pendingUrl = fixtureURLs[1] },
    tabs => { tabs[1].id = tabs[0].id }, tabs => { tabs[1].windowId = 999 }]) {
    const tabs = nativeTabs(10)
    mutate(tabs)
    assert.throws(() => validateTwoWindowTabs(tabs, nativePages))
  }
})

test('API double: two-window checkpoint/verify retain logical identity while native IDs change; zero page writes', async t => {
  const fake = fakeNative(t)
  const before = fs.readFileSync(fake.metadataPath, 'utf8')
  await fake.run('checkpoint')
  const path = `${fake.profile}/${twoWindowCheckpointName}`
  const checkpoint = validateTwoWindowCheckpoint(readBoundedJSON(path))
  const saved = fs.readFileSync(path, 'utf8')
  assert.equal(fs.statSync(path).mode & 0o777, 0o600)
  assert.deepEqual(checkpoint.metadata, state())
  assert.equal('canary' in checkpoint, false)
  await assert.rejects(fake.run('checkpoint'), /Checkpoint already exists/)
  await assert.rejects(fake.run('verify'), /Browser PID unchanged/)
  fake.pid = 20
  fake.revision = 'different-build'
  await assert.rejects(fake.run('verify'), /build\/executable differs/)
  fake.revision = 'invented-test'
  await fake.run('verify')
  assert.equal(fs.readFileSync(fake.metadataPath, 'utf8'), before)
  assert.equal(fs.readFileSync(path, 'utf8'), saved)
  assert.ok(!fake.calls.some(({ method }) => /navigate|createTarget|crash|close|reload|update/i.test(method)))
  assert.equal(fake.calls.filter(({ method }) => method === 'Target.attachToTarget').length,
    fake.calls.filter(({ method }) => method === 'Target.detachFromTarget').length)
  for (const mutate of [value => { value.canary = 'unexpected' }, value => { value.metadata.windows[1].id = '1' },
    value => { value.projection.favoritePinned = false }, value => { value.build.revision = '' }]) {
    const value = structuredClone(checkpoint)
    mutate(value)
    assert.throws(() => validateTwoWindowCheckpoint(value))
  }
})

test('API double: malformed/oversized metadata and foreign scope never create a checkpoint', async t => {
  const fake = fakeNative(t)
  for (const value of ['{', ' '.repeat(65537), JSON.stringify({ ...state(), items: null })]) {
    fs.writeFileSync(fake.metadataPath, value)
    await assert.rejects(fake.run('checkpoint'))
  }
  fs.writeFileSync(fake.metadataPath, JSON.stringify(state()))
  fake.contexts = ['private']
  await assert.rejects(fake.run('checkpoint'), /Close all developer-created private contexts/)
  fake.contexts = []
  fake.targets = [{ ...pages()[0], browserContextId: 'foreign' }]
  await assert.rejects(fake.run('checkpoint'), /Foreign native target context/)
  assert.equal(fs.existsSync(`${fake.profile}/${twoWindowCheckpointName}`), false)
})

test('API double: missing second native window cannot pass a restored subset', { timeout: 20000 }, async t => {
  const fake = fakeNative(t)
  await fake.run('checkpoint')
  fake.pid = 20
  fake.targets = [pages(20)[0], pages(20)[2]]
  await assert.rejects(fake.run('verify'), /Two-window restore not ready or loss.*require Favorite\/Favorite\/Temporary/s)
})

test('API double: valid saved-ID, Profile-owner and stable-window rebinds fail against checkpoint', { timeout: 60000 }, async t => {
  const fake = fakeNative(t)
  await fake.run('checkpoint')
  fake.pid = 20
  for (const mutate of [value => {
    value.items[0].id = '3'
    value.next_item_id = '4'
    value.windows.forEach(window => { window.selected_item_id = '3' })
  }, value => {
    value.profile_id = '9'
    value.spaces.forEach(space => { space.profile_id = '9' })
    value.items[0].owner_id = '9'
  }, value => {
    value.windows[1].id = '3'
    value.next_window_id = '4'
  }, value => {
    value.windows.forEach(window => { window.space_id = window.space_id === '1' ? '2' : '1' })
  }]) {
    const value = state()
    mutate(value)
    twoWindowWorkspaceProjection(value)
    fs.writeFileSync(fake.metadataPath, JSON.stringify(value))
    await assert.rejects(fake.run('verify'), /Two-window restore not ready or loss.*Workspace loss\/rebind/s)
  }
})
