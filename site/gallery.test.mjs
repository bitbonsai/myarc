import assert from 'node:assert/strict'
import { readFileSync } from 'node:fs'
import { test } from 'node:test'
import { runInNewContext } from 'node:vm'

const source = readFileSync(new URL('./gallery.js', import.meta.url), 'utf8')

const setup = (reduced = false) => {
  const timers = new Map()
  const frames = new Map()
  let nextId = 0
  let focused = null
  const element = () => {
    const listeners = new Map()
    const classes = new Set()
    return {
      classList: {
        add: (...names) => names.forEach(name => classes.add(name)),
        remove: (...names) => names.forEach(name => classes.delete(name)),
        contains: name => classes.has(name)
      },
      addEventListener: (name, callback) => listeners.set(name, callback),
      fire: (name, values = {}) => {
        const event = { button: 0, preventDefault: () => { event.defaultPrevented = true }, ...values }
        listeners.get(name)?.(event)
        return event
      },
      setAttribute: () => {},
      focus: () => {}
    }
  }
  const image = element()
  image.removeAttribute = name => { delete image[name] }
  const caption = element()
  const close = element()
  close.focus = () => { focused = close }
  const dialog = element()
  dialog.open = false
  dialog.querySelector = selector => ({ img: image, p: caption, button: close })[selector]
  dialog.showModal = () => {
    dialog.open = true
    close.focus()
  }
  dialog.close = () => {
    dialog.open = false
    dialog.fire('close')
  }
  dialog.getBoundingClientRect = () => ({ left: 10, right: 100, top: 10, bottom: 100 })
  const links = ['Card', 'Arc', 'myArc'].map((alt, index) => {
    const link = element()
    link.href = `${index}.webp`
    link.querySelector = () => ({ src: link.href, alt })
    link.closest = () => ({ querySelector: () => ({ textContent: `${alt} caption` }) })
    link.focus = () => { focused = link }
    return link
  })
  runInNewContext(source, {
    document: {
      querySelector: () => dialog,
      querySelectorAll: () => links
    },
    matchMedia: () => ({ matches: reduced }),
    requestAnimationFrame: callback => { frames.set(++nextId, callback); return nextId },
    cancelAnimationFrame: id => frames.delete(id),
    setTimeout: (callback, delay) => {
      assert.equal(delay, 150)
      timers.set(++nextId, callback)
      return nextId
    },
    clearTimeout: id => timers.delete(id)
  })
  const flush = queue => {
    const callbacks = [...queue.values()]
    queue.clear()
    callbacks.forEach(callback => callback())
  }
  return {
    dialog, image, caption, links, close, timers, frames,
    focused: () => focused,
    tick: () => flush(timers),
    frame: () => flush(frames),
    key: (key, values) => dialog.fire('keydown', { key, ...values }),
    open: (index = 0) => links[index].fire('click')
  }
}

test('opening retains two-frame reveal and close restores opener focus', () => {
  const g = setup()
  g.open(1)
  assert.equal(g.image.alt, 'Arc')
  assert.equal(g.caption.textContent, 'Arc caption')
  assert.equal(g.dialog.classList.contains('is-visible'), false)
  g.frame()
  assert.equal(g.dialog.classList.contains('is-visible'), false)
  g.frame()
  assert.equal(g.dialog.classList.contains('is-visible'), true)
  g.close.fire('click')
  assert.equal(g.dialog.open, false)
  assert.equal(g.focused(), g.links[1])
  assert.equal(g.image.src, undefined)
})

test('arrows wrap in article order, keep focus and synchronize image metadata', () => {
  const g = setup()
  g.open()
  for (const [key, alt, src] of [['ArrowLeft', 'myArc', '2.webp'], ['ArrowRight', 'Card', '0.webp'], ['ArrowRight', 'Arc', '1.webp']]) {
    assert.equal(g.key(key).defaultPrevented, true)
    assert.equal(g.dialog.classList.contains('is-swapping'), true)
    g.tick()
    assert.equal(g.image.src, src)
    assert.equal(g.image.alt, alt)
    assert.equal(g.caption.textContent, `${alt} caption`)
    assert.equal(g.focused(), g.close)
    assert.equal(g.dialog.open, true)
    assert.equal(g.dialog.classList.contains('is-swapping'), false)
  }
  assert.equal(g.key('Tab').defaultPrevented, true)
  assert.equal(g.focused(), g.close)
})

test('rapid arrows keep one pending timer and latest selected image wins', () => {
  const g = setup()
  g.open()
  g.key('ArrowRight')
  const stale = [...g.timers.values()][0]
  g.key('ArrowRight')
  assert.equal(g.timers.size, 1)
  g.tick()
  assert.equal(g.image.alt, 'myArc')
  g.key('ArrowLeft')
  g.close.fire('click')
  assert.equal(g.timers.size, 0)
  assert.equal(g.frames.size, 0)
  stale()
  g.tick()
  g.frame()
  assert.equal(g.image.src, undefined)
  assert.equal(g.dialog.classList.contains('is-visible'), false)
  assert.equal(g.dialog.classList.contains('is-swapping'), false)
  g.open(1)
  g.tick()
  assert.equal(g.image.alt, 'Arc')
})

test('reduced motion opens and swaps immediately without queued work', () => {
  const g = setup(true)
  g.open(2)
  assert.equal(g.dialog.classList.contains('is-visible'), true)
  g.key('ArrowRight')
  assert.equal(g.image.alt, 'Card')
  assert.equal(g.caption.textContent, 'Card caption')
  assert.equal(g.timers.size, 0)
  assert.equal(g.frames.size, 0)
})

test('closed dialog and modified shortcuts remain untouched; link fallbacks survive', () => {
  const g = setup()
  assert.equal(g.key('ArrowRight').defaultPrevented, undefined)
  assert.equal(g.timers.size, 0)
  for (const modifier of ['metaKey', 'ctrlKey', 'shiftKey', 'altKey']) {
    assert.equal(g.links[0].fire('click', { [modifier]: true }).defaultPrevented, undefined)
    assert.equal(g.dialog.open, false)
  }
  assert.equal(g.links[0].fire('click', { button: 1 }).defaultPrevented, undefined)
  g.open()
  for (const modifier of ['metaKey', 'ctrlKey', 'shiftKey', 'altKey']) {
    assert.equal(g.key('ArrowRight', { [modifier]: true }).defaultPrevented, undefined)
  }
  g.key('ArrowLeft', { defaultPrevented: true })
  assert.equal(g.timers.size, 0)
  assert.equal(g.image.alt, 'Card')
  assert.equal(g.key('Escape').defaultPrevented, undefined)
})

test('outside click dismisses and close cancels either opening frame', () => {
  for (const painted of [false, true]) {
    const g = setup()
    g.open()
    if (painted) g.frame()
    g.dialog.fire('click', { target: g.dialog, clientX: 50, clientY: 50 })
    assert.equal(g.dialog.open, true)
    g.dialog.fire('click', { target: g.dialog, clientX: 0, clientY: 0 })
    g.frame()
    assert.equal(g.dialog.open, false)
    assert.equal(g.frames.size, 0)
    assert.equal(g.dialog.classList.contains('is-visible'), false)
    assert.equal(g.focused(), g.links[0])
  }
})
