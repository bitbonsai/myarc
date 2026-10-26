(() => {
  const dialog = document.querySelector('#image-lightbox')
  if (!dialog || typeof dialog.showModal !== 'function') return

  const enlarged = dialog.querySelector('img')
  const caption = dialog.querySelector('p')
  let opener = null
  let revealFrame = 0
  let swapTimer = 0
  let selected = 0
  const links = [...document.querySelectorAll('.post-body figure a')].filter(link => {
    const image = link.querySelector('img')
    return image && link.href === image.src
  })
  const showImage = () => {
    const link = links[selected]
    const image = link.querySelector('img')
    enlarged.src = link.href
    enlarged.alt = image.alt
    caption.textContent = link.closest('figure').querySelector('figcaption')?.textContent || image.alt
  }

  links.forEach((link, index) => {
    link.setAttribute('aria-haspopup', 'dialog')
    link.addEventListener('click', event => {
      if (event.button !== 0 || event.metaKey || event.ctrlKey || event.shiftKey || event.altKey) return
      event.preventDefault()
      opener = link
      selected = index
      showImage()
      dialog.showModal()
      if (matchMedia('(prefers-reduced-motion: reduce)').matches) {
        dialog.classList.add('is-visible')
      } else {
        // Paint the backdrop before starting the content transition.
        revealFrame = requestAnimationFrame(() => {
          if (!dialog.open) return
          revealFrame = requestAnimationFrame(() => {
            if (dialog.open) dialog.classList.add('is-visible')
          })
        })
      }
    })
  })

  const close = dialog.querySelector('button')
  close.addEventListener('click', () => dialog.close())
  dialog.addEventListener('keydown', event => {
    if (!dialog.open) return
    if (!event.defaultPrevented && !event.metaKey && !event.ctrlKey && !event.shiftKey && !event.altKey && (event.key === 'ArrowRight' || event.key === 'ArrowLeft')) {
      event.preventDefault()
      selected = (selected + (event.key === 'ArrowRight' ? 1 : -1) + links.length) % links.length
      clearTimeout(swapTimer)
      if (matchMedia('(prefers-reduced-motion: reduce)').matches) {
        dialog.classList.remove('is-swapping')
        showImage()
      } else {
        dialog.classList.add('is-swapping')
        swapTimer = setTimeout(() => {
          if (!dialog.open) return
          showImage()
          dialog.classList.remove('is-swapping')
        }, 150)
      }
    }
    // The close button is the modal's only focusable control.
    if (event.key === 'Tab') {
      event.preventDefault()
      close.focus()
    }
  })
  dialog.addEventListener('click', event => {
    if (event.target !== dialog) return
    const box = dialog.getBoundingClientRect()
    if (event.clientX < box.left || event.clientX > box.right || event.clientY < box.top || event.clientY > box.bottom) dialog.close()
  })
  dialog.addEventListener('close', () => {
    cancelAnimationFrame(revealFrame)
    clearTimeout(swapTimer)
    dialog.classList.remove('is-visible', 'is-swapping')
    enlarged.removeAttribute('src')
    opener?.focus({ preventScroll: true })
  })
})()
