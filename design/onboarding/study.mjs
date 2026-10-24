import { initialState, lessons, makeSample, transition } from './state.mjs'

const root = document.querySelector('#study')
const copy = document.querySelector('.copy')
const sidebar = document.querySelector('.sidebar')
const page = document.querySelector('.page')
const dialog = document.querySelector('#command-dialog')
const query = document.querySelector('#command-query')
const results = document.querySelector('#command-results')
const announcement = document.querySelector('#announcement')
const escape = value => String(value).replace(/[&<>"']/g, char => ({ '&': '&amp;', '<': '&lt;', '>': '&gt;', '"': '&quot;', "'": '&#39;' })[char])
let state = initialState()
let visiblePins = 12
let failedOnce = false
let commandOpener = null
let generation = 0

const primary = (action, label) => `<button class="primary" data-action="${action}">${label} <span aria-hidden="true">↗</span></button>`
const textButton = (action, label) => `<button class="text-button" data-action="${action}">${label}</button>`
const heading = (step, title, description) => `<p class="eyebrow">${step}</p><h1 id="scene-title" tabindex="-1">${title}</h1><p class="description">${description}</p>`

const renderCopy = sample => {
  switch (state.scene) {
    case 'welcome':
      return `${heading('01 / A fresh beginning', 'A little space.<br>All your own.', 'Bring the way you browse. Leave the noise behind. Your next chapter starts right here, on your device.')}<div class="actions">${primary('begin', 'Make it mine')}${textButton('fresh', 'Start fresh')}</div><p class="fine-print">A clickable design study, not a browser. No account, file access or network navigation.</p>`
    case 'source':
      return `${heading('02 / Bring your rhythm', 'Familiar things.<br>A new home.', 'Choose invented content to explore the motion. Real export selection will follow verified format support.')}<fieldset class="sample-choices"><legend>Choose a synthetic workspace</legend>${[
        ['standard', 'A familiar setup', '18 pins · 3 Spaces · 2 Profiles'],
        ['empty', 'A clean slate', 'Empty workspace variant'],
        ['large', 'A bigger collection', '1,000 pins · bounded reveal'],
        ['failure', 'An interrupted move', 'One deliberate sample failure']
      ].map(([value, label, detail]) => `<label class="sample-choice"><input type="radio" name="sample" value="${value}" ${state.sample === value ? 'checked' : ''}><span><strong>${label}</strong><small>${detail}</small></span></label>`).join('')}</fieldset><div class="actions">${primary('review', 'Review sample')}${textButton('back', 'Back')}${textButton('fresh', 'Start fresh')}</div>`
    case 'preview':
      return `${heading('03 / Before anything moves', 'Your setup,<br>at a glance.', 'Review the sample before applying it to this study. Nothing is read from your browser or written to disk.')}<div class="summary"><span>${sample.pins.length.toLocaleString()} synthetic pins across 3 Spaces</span><span>Everyday + Weekend share Personal Profile.</span><span>Studio uses a separate Work Profile.</span></div><p class="fine-print">No cookies, passwords, extension data or executable Boosts. This fixture proves no Arc format coverage.</p>${state.error ? `<p class="error" role="alert">${escape(state.error)}</p>` : ''}<div class="actions">${primary('apply', state.error ? 'Retry sample' : 'Apply sample')}${textButton('back', 'Back')}</div>`
    case 'commit':
      return `${heading('03 / Study state', 'Applying<br>sample state.', 'This is an in-memory study action, not import progress. The reveal follows its result.')}<div class="actions">${textButton('replay', 'Cancel and replay')}</div>`
    case 'reveal':
      return `${heading('04 / A place for everything', sample.pins.length ? 'There you are.' : 'Room to begin.', sample.pins.length ? 'The same sample pins settle into their places. A small, bounded reveal makes room for even a big collection.' : 'A quiet beginning is a complete beginning. Your space is ready for whatever comes next.')}<div class="actions">${primary('practice', 'Try the workspace')}${textButton('skip', 'Skip guidance')}</div><p class="fine-print">Sample applied in memory. These are local study controls, not imported data or live pages.</p>`
    case 'practice': {
      const descriptions = [
        sample.pins.length ? 'Choose a pin in the sidebar. Its sample page appears here without opening a website.' : 'This sample has no pins. Skip this lesson, or replay with the familiar setup to try opening one.',
        'Select another colored Space in the sidebar. Notice its separate pins and Profile label.',
        'Open Find a pin, or press Cmd+T. Search the sample, then choose a result. Escape closes it.'
      ]
      return `${heading('05 / Make yourself at home', ['One small<br>discovery.', 'A change<br>of scene.', 'A shortcut<br>to anywhere.'][state.lesson], descriptions[state.lesson])}<ol class="lesson-list">${lessons.map((lesson, index) => `<li ${state.lesson === index ? 'aria-current="step"' : ''}><span class="lesson-bullet ${state.completed.includes(index) ? 'done' : ''}" aria-label="${state.completed.includes(index) ? 'Done' : 'Not done'}">${state.completed.includes(index) ? '✓' : index + 1}</span>${lesson}</li>`).join('')}</ol><div class="actions">${primary('next', state.completed.includes(state.lesson) ? state.lesson === 2 ? 'Finish guidance' : 'Next lesson' : 'Skip this lesson')}${textButton('skip', 'Skip all guidance')}</div>`
    }
    case 'complete':
      return `${heading('06 / Yours from here', 'Settle in.', 'The guide steps aside. Your sample workspace stays right where you left it. Keep exploring, or replay the first minute.')}<p class="fine-print">${state.completed.length} of 3 study interactions tried. No browsing, import or durable storage has shipped in this prototype.</p><div class="actions">${primary('focusWorkspace', 'Explore sample')}${textButton('replay', 'Replay')}</div>`
  }
}

const renderWorkspace = sample => {
  const live = ['practice', 'complete'].includes(state.scene)
  const space = sample.spaces.find(item => item.id === state.activeSpace)
  const pins = sample.pins.filter(pin => pin.space === space.id)
  const selected = sample.pins.find(pin => pin.id === state.selectedPin)
  sidebar.innerHTML = `<p class="sidebar-title">A place to land.</p><div class="space-list" ${live ? 'role="group" aria-label="Sample Spaces"' : 'aria-hidden="true"'}>${sample.spaces.map(item => live
    ? `<button class="space" style="--space-color:${item.color}" data-action="space" data-id="${item.id}" aria-label="${item.name} Space" aria-pressed="${item.id === space.id}"><span class="space-dot"></span></button>`
    : `<span class="space" style="--space-color:${item.color}"><span class="space-dot"></span></span>`).join('')}</div><p class="space-name">${space.name}</p><p class="profile-name">${sample.profiles.find(profile => profile.id === space.profile).name} Profile</p><div class="pin-list">${pins.slice(0, visiblePins).map((pin, index) => live
    ? `<button class="pin" data-action="pin" data-id="${pin.id}" aria-pressed="${pin.id === state.selectedPin}" style="--index:${index}"><span class="pin-icon" aria-hidden="true">${index + 1}</span><span class="pin-label">${escape(pin.title)}</span></button>`
    : `<div class="pin" data-pin-id="${pin.id}" style="--index:${index}"><span class="pin-icon" aria-hidden="true">${index + 1}</span><span class="pin-label">${escape(pin.title)}</span></div>`).join('')}${pins.length ? '' : '<p class="pin-count">Your next pin belongs here.</p>'}</div><p class="pin-count">${Math.min(visiblePins, pins.length)} of ${pins.length} sample pins</p>${live && pins.length > visiblePins ? textButton('more', 'Show more pins') : ''}${live ? '<button class="command-trigger" data-action="command">Find a pin <kbd>⌘ T</kbd></button>' : ''}`
  page.innerHTML = `<span class="page-label">${selected ? 'Local sample content' : 'Your space, at your pace'}</span><div class="page-mark" aria-hidden="true"></div><h2>${selected ? escape(selected.title) : 'Less noise.<br>More room.'}</h2><p>${selected ? 'You opened a sample pin. In the browser, this action will use the shared command path to open a real page.' : 'A calm corner for the things<br>you want to come back to.'}</p>${selected ? `<p class="sample-url">${escape(selected.url)}</p><p>Rendered locally. No website requested.</p>` : ''}`
}

const render = (focus = true, focusSelector = '') => {
  const sample = makeSample(state.sample)
  root.dataset.scene = state.scene
  copy.innerHTML = renderCopy(sample)
  renderWorkspace(sample)
  const progress = ['welcome', 'source', 'preview', 'reveal', 'practice', 'complete']
  const current = state.scene === 'commit' ? 'preview' : state.scene
  document.querySelector('.steps').innerHTML = ['Welcome', 'Setup', 'Review', 'Reveal', 'Practice', 'Handoff'].map((label, index) => `<li ${progress[index] === current ? 'aria-current="step"' : ''}>${label}</li>`).join('')
  if (focusSelector) document.querySelector(focusSelector)?.focus()
  else if (focus) document.querySelector('#scene-title').focus()
}

const dispatch = (event, focusSelector = '') => {
  const before = state
  state = transition(state, event)
  if (state !== before) {
    render(state.scene !== before.scene || state.lesson !== before.lesson, focusSelector)
    if (state.scene !== before.scene) announcement.textContent = document.querySelector('#scene-title').textContent
  }
}

const search = () => {
  const term = query.value.trim().toLowerCase()
  const matches = makeSample(state.sample).pins.filter(pin => pin.title.toLowerCase().includes(term)).slice(0, 8)
  results.innerHTML = matches.length ? matches.map(pin => `<button class="command-result" data-result="${pin.id}">${escape(pin.title)}<small>${escape(pin.url)}</small></button>`).join('') : '<p class="fine-print">No matching sample pins. Try another search or close the command bar.</p>'
}

const openCommand = () => {
  if (!['practice', 'complete'].includes(state.scene) || dialog.open) return
  dispatch({ type: 'command' })
  commandOpener = document.querySelector('[data-action="command"]')
  query.value = ''
  search()
  dialog.showModal()
  query.focus()
}

root.addEventListener('click', event => {
  const button = event.target.closest('button[data-action]')
  if (!button) return
  const action = button.dataset.action
  if (action === 'command') return openCommand()
  if (action === 'focusWorkspace') return document.querySelector('.space')?.focus()
  if (action === 'more') {
    visiblePins += 12
    render(false, '[data-action="more"]')
    if (!document.activeElement.matches('[data-action="more"]')) document.querySelector('.command-trigger')?.focus()
    return
  }
  if (action === 'space') visiblePins = 12
  if (action === 'replay') {
    generation++
    failedOnce = false
    visiblePins = 12
  }
  dispatch({ type: action, id: button.dataset.id }, ['space', 'pin'].includes(action) ? `[data-action="${action}"][data-id="${button.dataset.id}"]` : '')
  if (action === 'pin') announcement.textContent = 'Sample pin opened locally.'
  if (action === 'space') announcement.textContent = `${makeSample(state.sample).spaces.find(space => space.id === state.activeSpace).name} Space selected.`
  if (action === 'apply' && state.scene === 'commit') {
    const attempt = ++generation
    // A microtask supplies a fixture result. Animation never decides success.
    queueMicrotask(() => {
      if (attempt !== generation || state.scene !== 'commit') return
      const failure = state.sample === 'failure' && !failedOnce
      if (failure) failedOnce = true
      dispatch({ type: failure ? 'failed' : 'applied' })
    })
  }
})

root.addEventListener('change', event => {
  if (event.target.name !== 'sample') return
  failedOnce = false
  visiblePins = 12
  dispatch({ type: 'sample', sample: event.target.value }, `input[value="${event.target.value}"]`)
})

document.querySelector('#restart').addEventListener('click', () => {
  generation++
  failedOnce = false
  visiblePins = 12
  dispatch({ type: 'replay' })
})

document.querySelector('#motion').addEventListener('change', event => {
  document.documentElement.classList.toggle('reduce-motion', event.target.value === 'reduce')
  document.documentElement.classList.toggle('full-motion', event.target.value === 'full')
})

document.addEventListener('keydown', event => {
  if (event.key === 'Escape' && dialog.open) {
    event.preventDefault()
    dialog.close()
    return
  }
  if ((event.metaKey || event.ctrlKey) && event.key.toLowerCase() === 't' && ['practice', 'complete'].includes(state.scene)) {
    event.preventDefault()
    openCommand()
  }
})
query.addEventListener('input', search)
document.querySelector('#close-command').addEventListener('click', () => dialog.close())
dialog.addEventListener('close', () => commandOpener?.focus())
results.addEventListener('click', event => {
  const button = event.target.closest('[data-result]')
  const pin = makeSample(state.sample).pins.find(item => item.id === button?.dataset.result)
  if (!pin) return
  dispatch({ type: 'space', id: pin.space })
  visiblePins = Math.max(12, makeSample(state.sample).pins.filter(item => item.space === pin.space).findIndex(item => item.id === pin.id) + 1)
  dispatch({ type: 'pin', id: pin.id })
  commandOpener = document.querySelector(`[data-action="pin"][data-id="${pin.id}"]`)
  dialog.close()
  announcement.textContent = 'Search result opened as local sample content.'
})

render(false)
