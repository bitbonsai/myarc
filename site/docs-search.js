(() => {
  const trigger = document.querySelector('.docs-search-btn')
  const dialog = document.querySelector('#docs-search')
  const index = window.__MYARC_DOCS_INDEX__
  if (!trigger || !dialog || !Array.isArray(index) || typeof dialog.showModal !== 'function') return

  const input = dialog.querySelector('.docs-search-input')
  const list = dialog.querySelector('.docs-search-results')
  const empty = dialog.querySelector('.docs-search-empty')
  const prefix = dialog.dataset.prefix || ''
  const reduced = () => matchMedia('(prefers-reduced-motion: reduce)').matches

  let frame = 0
  let timer = 0
  let opener = null
  let active = -1
  let shown = []

  const escapeHtml = text => text.replace(/[&<>"']/g, ch => ({ '&': '&amp;', '<': '&lt;', '>': '&gt;', '"': '&quot;', "'": '&#39;' }[ch]))

  const highlight = (text, terms) => {
    const safe = escapeHtml(text)
    if (!terms.length) return safe
    const pattern = new RegExp('(' + terms.map(t => t.replace(/[.*+?^${}()|[\]\\]/g, '\\$&')).join('|') + ')', 'gi')
    return safe.replace(pattern, '<mark>$1</mark>')
  }

  const snippet = (text, terms) => {
    if (!text) return ''
    const lower = text.toLowerCase()
    let pos = -1
    for (const term of terms) {
      const idx = lower.indexOf(term)
      if (idx !== -1 && (pos === -1 || idx < pos)) pos = idx
    }
    if (pos === -1) return text.slice(0, 140) + (text.length > 140 ? '…' : '')
    const start = Math.max(0, pos - 48)
    const end = Math.min(text.length, pos + 112)
    return (start > 0 ? '…' : '') + text.slice(start, end).trim() + (end < text.length ? '…' : '')
  }

  const search = raw => {
    const query = raw.trim().toLowerCase()
    const terms = query ? query.split(/\s+/).filter(Boolean) : []
    if (!terms.length) {
      return index.map(doc => ({
        href: prefix + doc.slug + '/',
        title: doc.title,
        meta: doc.area,
        copy: doc.summary,
        terms: []
      }))
    }
    const matches = []
    for (const doc of index) {
      const titleLow = doc.title.toLowerCase()
      const areaLow = doc.area.toLowerCase()
      const summaryLow = doc.summary.toLowerCase()
      const bodyLow = doc.body.toLowerCase()
      const haystack = titleLow + ' ' + areaLow + ' ' + summaryLow + ' ' + bodyLow + ' ' + doc.sections.map(s => s.title.toLowerCase()).join(' ')
      if (!terms.every(term => haystack.includes(term))) continue
      let score = 0
      let bestSection = null
      for (const term of terms) {
        if (titleLow === query) score += 120
        else if (titleLow.startsWith(term)) score += 60
        else if (titleLow.includes(term)) score += 40
        if (areaLow.includes(term)) score += 12
        if (summaryLow.includes(term)) score += 16
        if (bodyLow.includes(term)) score += 8
        for (const section of doc.sections) {
          const secLow = section.title.toLowerCase()
          if (secLow.includes(term)) {
            score += 24
            if (!bestSection) bestSection = section
          }
        }
      }
      matches.push({
        score,
        href: prefix + doc.slug + '/' + (bestSection ? '#' + bestSection.id : ''),
        title: bestSection ? doc.title + ' / ' + bestSection.title : doc.title,
        meta: doc.area,
        copy: snippet(doc.summary + ' ' + doc.body, terms),
        terms
      })
    }
    matches.sort((a, b) => b.score - a.score)
    return matches.slice(0, 12)
  }

  const select = next => {
    const items = [...list.querySelectorAll('a')]
    if (!items.length) {
      active = -1
      return
    }
    active = (next + items.length) % items.length
    items.forEach((item, idx) => item.classList.toggle('is-active', idx === active))
    items[active].scrollIntoView({ block: 'nearest' })
  }

  const render = () => {
    shown = search(input.value)
    active = shown.length ? 0 : -1
    empty.hidden = shown.length > 0
    list.innerHTML = shown.map((item, idx) => (
      '<li><a href="' + escapeHtml(item.href) + '"' + (idx === 0 ? ' class="is-active"' : '') + '>' +
      '<span class="docs-search-meta">' + escapeHtml(item.meta) + '</span>' +
      '<span class="docs-search-title">' + highlight(item.title, item.terms) + '</span>' +
      '<span class="docs-search-copy">' + highlight(item.copy, item.terms) + '</span>' +
      '</a></li>'
    )).join('')
  }

  const hide = () => {
    cancelAnimationFrame(frame)
    dialog.classList.remove('is-visible')
    clearTimeout(timer)
    if (reduced()) dialog.close()
    else timer = setTimeout(() => dialog.close(), 200)
  }

  const open = () => {
    opener = document.activeElement instanceof HTMLElement ? document.activeElement : trigger
    clearTimeout(timer)
    render()
    if (!dialog.open) dialog.showModal()
    if (reduced()) dialog.classList.add('is-visible')
    else {
      frame = requestAnimationFrame(() => {
        frame = requestAnimationFrame(() => {
          if (dialog.open) dialog.classList.add('is-visible')
        })
      })
    }
    input.focus()
    input.select()
  }

  trigger.hidden = false
  trigger.addEventListener('click', open)
  input.addEventListener('input', render)
  input.addEventListener('keydown', event => {
    if (event.key === 'ArrowDown') {
      event.preventDefault()
      select(active + 1)
    } else if (event.key === 'ArrowUp') {
      event.preventDefault()
      select(active - 1)
    } else if (event.key === 'Enter' && active >= 0) {
      const current = list.querySelectorAll('a')[active]
      if (current) {
        event.preventDefault()
        current.click()
      }
    }
  })

  list.addEventListener('click', event => {
    const link = event.target.closest('a')
    if (link) hide()
  })

  dialog.addEventListener('cancel', event => {
    event.preventDefault()
    hide()
  })
  dialog.querySelector('form').addEventListener('submit', event => {
    event.preventDefault()
    hide()
  })
  dialog.addEventListener('click', event => {
    if (event.target !== dialog) return
    const box = dialog.getBoundingClientRect()
    if (event.clientX < box.left || event.clientX > box.right || event.clientY < box.top || event.clientY > box.bottom) hide()
  })
  dialog.addEventListener('close', () => {
    cancelAnimationFrame(frame)
    clearTimeout(timer)
    dialog.classList.remove('is-visible')
    if (opener) opener.focus({ preventScroll: true })
  })

  document.addEventListener('keydown', event => {
    if ((event.metaKey || event.ctrlKey) && event.key.toLowerCase() === 'k') {
      event.preventDefault()
      if (dialog.open) hide()
      else open()
    } else if (event.key === '/' && !dialog.open && !event.metaKey && !event.ctrlKey && !event.altKey) {
      const tag = document.activeElement ? document.activeElement.tagName : ''
      if (tag !== 'INPUT' && tag !== 'TEXTAREA' && !document.activeElement?.isContentEditable) {
        event.preventDefault()
        open()
      }
    }
  })
})()
