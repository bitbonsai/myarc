(() => {
  const dialog = document.querySelector('#image-lightbox')
  if (!dialog || typeof dialog.showModal !== 'function') return

  const enlarged = dialog.querySelector('img')
  const caption = dialog.querySelector('p')
  let opener = null

  document.querySelectorAll('.post-body figure a').forEach(link => {
    const image = link.querySelector('img')
    if (!image || link.href !== image.src) return

    link.setAttribute('aria-haspopup', 'dialog')
    link.addEventListener('click', event => {
      if (event.button !== 0 || event.metaKey || event.ctrlKey || event.shiftKey || event.altKey) return
      event.preventDefault()
      opener = link
      enlarged.src = link.href
      enlarged.alt = image.alt
      caption.textContent = link.closest('figure').querySelector('figcaption')?.textContent || image.alt
      dialog.showModal()
    })
  })

  dialog.querySelector('button').addEventListener('click', () => dialog.close())
  dialog.addEventListener('click', event => {
    if (event.target !== dialog) return
    const box = dialog.getBoundingClientRect()
    if (event.clientX < box.left || event.clientX > box.right || event.clientY < box.top || event.clientY > box.bottom) dialog.close()
  })
  dialog.addEventListener('close', () => {
    enlarged.removeAttribute('src')
    opener?.focus({ preventScroll: true })
  })
})()
