// Developer acceptance tooling. API-double tests establish no native behavior.
import assert from 'node:assert/strict'
import fs from 'node:fs'
import { setTimeout as delay } from 'node:timers/promises'
import { fixtureURLs, openWorkspaceObservation, readBoundedJSON, twoWindowWorkspaceProjection,
  validateCheckpointIdentity } from './browser_workspace_recovery.mjs'

export const twoWindowCheckpointName = '.myarc-m0-workspace-two-window.json'
const workerId = 'faokaoibbnjigecocmhniflfnkpmacle'
const workerScope = `chrome-extension://${workerId}/`
const workerManifest = JSON.parse(fs.readFileSync(new URL('../tests/fixtures/browser/workspace-pin-extension/manifest.json', import.meta.url), 'utf8'))
const projection = { nativeWindowCount: 2, favoriteProjections: 2, temporaryProjections: 1, favoritePinned: true }

export const validateTwoWindowTargets = (targets, contextId, verifying = false) => {
  assert.ok(Array.isArray(targets) && targets.length <= 32, 'Native target list exceeds narrow fixture bound')
  for (const target of targets) {
    if (target.browserContextId !== undefined) assert.equal(target.browserContextId, contextId, 'Foreign native target context')
  }
  const pages = targets.filter(target => target.type === 'page')
  for (const page of pages) {
    assert.equal(page.browserContextId, contextId, 'Page belongs to a foreign/private Profile context')
    assert.ok(typeof page.targetId === 'string' && page.targetId.length > 0 && page.targetId.length <= 256, 'Invalid native target ID')
    assert.ok(fixtureURLs.includes(page.url) || (verifying && page.url === 'chrome://newtab/'), 'Unexpected page target')
  }
  assert.equal(new Set(pages.map(page => page.targetId)).size, pages.length, 'Duplicate native page target ID')
  for (const [url, limit] of [[fixtureURLs[0], 2], [fixtureURLs[1], 1], ['chrome://newtab/', verifying ? 1 : 0]]) {
    assert.ok(pages.filter(page => page.url === url).length <= limit, 'Duplicate/unexpected page projection')
  }
  return pages
}

export const validateTwoWindowCheckpoint = checkpoint => {
  assert.deepEqual(Object.keys(checkpoint).sort(), 'format browserPid build profilePath expectedURLs metadata projection recordedAt'.split(' ').sort())
  validateCheckpointIdentity(checkpoint)
  twoWindowWorkspaceProjection(checkpoint.metadata)
  assert.deepEqual(checkpoint.projection, projection)
  return checkpoint
}

// CDP and extension window IDs are native session IDs, never Rust stable IDs.
export const validateTwoWindowTabs = (tabs, pages) => {
  assert.ok(Array.isArray(tabs) && tabs.length === 3, 'Require exactly three actual native tabs')
  assert.equal(new Set(tabs.map(tab => tab.id)).size, 3, 'Duplicate native tab ID')
  for (const tab of tabs) {
    assert.ok(Number.isSafeInteger(tab.id) && tab.id >= 0 && Number.isSafeInteger(tab.windowId) && tab.windowId > 0, 'Invalid native tab/window ID')
    assert.equal(tab.incognito, false, 'Private native tab')
    assert.ok(!tab.pendingUrl, 'Native tab has pending navigation')
    assert.ok(fixtureURLs.includes(tab.url), 'Unexpected actual native tab URL')
    assert.equal(tab.pinned, tab.url === fixtureURLs[0], 'Native Favorite pin projection lost or Temporary unexpectedly pinned')
    assert.equal(tab.active, tab.url === fixtureURLs[0], 'Native selection differs from both saved Favorite selections')
    assert.equal(pages.filter(page => page.url === tab.url && page.windowId === tab.windowId).length, 1, 'CDP/extension native window projection mismatch')
  }
  const windows = new Set(tabs.map(tab => tab.windowId))
  assert.equal(windows.size, 2, 'Require two distinct actual native windows')
  for (const windowId of windows) {
    assert.equal(tabs.filter(tab => tab.windowId === windowId && tab.url === fixtureURLs[0]).length, 1, 'Require one Favorite projection per native window')
  }
  return projection
}

export const runTwoWindowRecovery = async ({ send, profile, ownedPid, scenario }) => {
  assert.ok(['workspace-two-window-checkpoint', 'workspace-two-window-verify'].includes(scenario), 'Unknown two-window phase')
  const { cdp, deadline, build, contextId, evaluatePage, scopedTargets } = await openWorkspaceObservation({ send, profile })
  const verifying = scenario === 'workspace-two-window-verify'
  const path = `${profile}/${twoWindowCheckpointName}`
  const previous = verifying ? validateTwoWindowCheckpoint(readBoundedJSON(path)) : null
  if (previous) {
    assert.notEqual(previous.browserPid, ownedPid, 'Browser PID unchanged: parent owns actual crash/relaunch provenance')
    assert.equal(previous.profilePath, `${profile}/Default`, 'Checkpoint belongs to another Profile')
    assert.deepEqual(build, previous.build, 'Restored browser build/executable differs from checkpoint')
  } else {
    assert.ok(!fs.existsSync(path), 'Checkpoint already exists; preserve it for verify or explicitly remove it before a new journey')
  }
  const initialPages = validateTwoWindowTargets(await scopedTargets(), contextId, verifying)
  const favorite = initialPages.find(page => page.url === fixtureURLs[0])
  assert.ok(favorite, 'Complete genuine native Restore/setup before probing')
  // Same supported MV3 wake as workspace-pin, solely for read-only tabs.query.
  const { sessionId } = await cdp('Target.attachToTarget', { targetId: favorite.targetId, flatten: true })
  try {
    await cdp('ServiceWorker.enable', {}, sessionId)
    await cdp('ServiceWorker.startWorker', { scopeURL: workerScope }, sessionId)
    const settleDeadline = Math.min(deadline - 1000, Date.now() + 10000)
    let settled = 0
    let retained
    let lastError
    while (Date.now() < settleDeadline && settled < 5) {
      const targets = await scopedTargets()
      // Foreign contexts, excess projections and unknown URLs abort immediately.
      const pages = validateTwoWindowTargets(targets, contextId, verifying)
      const workers = targets.filter(target => target.type === 'service_worker' && target.url === `${workerScope}worker.js`)
      assert.ok(workers.length <= 1, 'Ambiguous fixture worker')
      if (workers[0]) assert.equal(workers[0].browserContextId, contextId, 'Foreign fixture worker')
      try {
        assert.equal(pages.length, 3, 'Restore not ready: require Favorite/Favorite/Temporary, with startup NTP closed')
        assert.equal(pages.filter(page => page.url === fixtureURLs[0]).length, 2, 'Missing native Favorite projection')
        assert.equal(pages.filter(page => page.url === fixtureURLs[1]).length, 1, 'Missing native Temporary projection')
        assert.ok(workers[0], 'Load existing workspace-pin MV3 fixture in this Profile; worker not ready')
        const metadata = readBoundedJSON(`${profile}/Default/myarc-workspace-v1.json`)
        const workspace = twoWindowWorkspaceProjection(metadata)
        if (previous) assert.deepEqual(workspace, twoWindowWorkspaceProjection(previous.metadata), 'Workspace loss/rebind: stable IDs, owners, Spaces or window selections differ')
        const nativePages = []
        for (const page of pages) {
          const { windowId } = await cdp('Browser.getWindowForTarget', { targetId: page.targetId })
          assert.ok(Number.isSafeInteger(windowId) && windowId > 0, 'Missing actual native window ID')
          const observed = await evaluatePage(page.targetId, `({ url: location.href.slice(0, 256), title: document.title.slice(0, 81),
            ready: document.readyState, visibility: document.visibilityState, form: Boolean(document.querySelector('#form #note')) })`)
          assert.equal(observed.url, page.url, 'Fixture navigated during observation')
          assert.equal(observed.ready, 'complete', 'Fixture still loading')
          const isFavorite = page.url === fixtureURLs[0]
          assert.equal(observed.title, isFavorite ? 'myArc foundation check' : 'myArc recovery second page')
          assert.equal(observed.visibility, isFavorite ? 'visible' : 'hidden', 'Native page selection differs')
          assert.equal(observed.form, isFavorite, 'Synthetic fixture form differs')
          nativePages.push({ url: page.url, windowId })
        }
        const native = await evaluatePage(workers[0].targetId, `(async () => {
          const tabs = await chrome.tabs.query({})
          if (tabs.length > 4) throw new Error('Excess native tabs')
          return { id: chrome.runtime.id, manifest: chrome.runtime.getManifest(), url: location.href,
            tabs: tabs.map(({ id, windowId, url, pendingUrl, incognito, active, pinned }) =>
              ({ id, windowId, url, pendingUrl, incognito, active, pinned })) }
        })()`)
        assert.deepEqual({ id: native.id, manifest: native.manifest, url: native.url },
          { id: workerId, manifest: workerManifest, url: `${workerScope}worker.js` }, 'Wrong fixture worker identity')
        validateTwoWindowTabs(native.tabs, nativePages)
        assert.ok(Date.now() <= settleDeadline, 'Observation completed after ten-second settle deadline')
        const observation = { metadata, workspace, projection }
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
    assert.equal(settled, 5, `Two-window restore not ready or loss within ten-second budget: ${lastError?.message ?? 'state never settled'}. Complete genuine Restore and close startup NTP; never rewrite workspace metadata.`)
    const processes = await cdp('SystemInfo.getProcessInfo')
    assert.ok(processes.processInfo.some(process => process.type === 'browser' && process.id === ownedPid), 'Owned browser PID changed during probe')
    if (!verifying) {
      const checkpoint = validateTwoWindowCheckpoint({ format: 1, browserPid: ownedPid, build, profilePath: `${profile}/Default`,
        expectedURLs: fixtureURLs, metadata: retained.metadata, projection, recordedAt: new Date().toISOString() })
      fs.writeFileSync(path, `${JSON.stringify(checkpoint, null, 2)}\n`, { flag: 'wx', mode: 0o600 })
    }
    console.log(JSON.stringify({ scenario, browserPid: ownedPid, previousBrowserPid: previous?.browserPid,
      browser: build.product, workspace: retained.workspace, projection, stableSamples: settled }))
    console.log('PASS: observed disk workspace identity, two native windows and both Favorite pins; parent owns crash/Restore provenance. No site-storage or acknowledged-write durability check; native-to-Rust binding is not directly observable.')
  } finally {
    try {
      await cdp('ServiceWorker.disable', {}, sessionId)
    } finally {
      await cdp('Target.detachFromTarget', { sessionId })
    }
  }
}
