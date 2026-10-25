// Local fixture/argument checks only. API doubles below prove no native browser behavior.
import assert from 'node:assert/strict'
import { createHash } from 'node:crypto'
import fs from 'node:fs'
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
  for (const mode of ['extension', 'restart', 'runtime', 'workspace-pin']) {
    const result = run(['.', '1', mode])
    assert.equal(result.status, 1)
    assert.match(result.stderr, /Expected marked, disposable test profile/)
  }
})
