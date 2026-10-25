// Privileged CDP only. No web messages, automatic mutations or keepalive.
const fixtureURL = 'http://127.0.0.1:8766/'

globalThis.myarcWorkspaceFixtureTab = async () => {
  const tabs = await chrome.tabs.query({ url: fixtureURL })
  if (tabs.length !== 1) throw new Error('Expected exactly one loopback fixture tab')
  const tab = tabs[0]
  if (!Number.isSafeInteger(tab.id) || tab.id < 0 || tab.incognito ||
      tab.url !== fixtureURL || tab.pendingUrl || !tab.active) {
    throw new Error('Expected active ordinary fixture tab without pending navigation')
  }
  return { id: tab.id, windowId: tab.windowId, url: tab.url, pinned: tab.pinned }
}

globalThis.myarcWorkspaceFixturePin = async (tabId, pinned) => {
  if (!Number.isSafeInteger(tabId) || tabId < 0 || typeof pinned !== 'boolean') {
    throw new Error('Expected explicit tab ID and boolean pinned state')
  }
  const tab = await globalThis.myarcWorkspaceFixtureTab()
  if (tab.id !== tabId) throw new Error('Fixture tab ID changed; refusing mutation')
  await chrome.tabs.update(tabId, { pinned })
  return globalThis.myarcWorkspaceFixtureTab()
}
