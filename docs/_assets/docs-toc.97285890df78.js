(() => {
  const links = [...document.querySelectorAll('.docs-toc a[href^="#"]')]
  const headings = links.map(link => document.getElementById(decodeURIComponent(link.hash.slice(1))))
  if (!links.length || headings.some(heading => !heading)) return

  let frame = 0
  // The section in view is the last heading above a line near the top of the window.
  // Once scrolled to the very bottom the last heading wins, since a short final section never reaches that line.
  const update = () => {
    frame = 0
    const line = Math.min(innerHeight * .3, 160)
    let current = 0
    headings.forEach((heading, index) => {
      if (heading.getBoundingClientRect().top <= line) current = index
    })
    if (scrollY > 0 && innerHeight + scrollY >= document.documentElement.scrollHeight - 2) current = headings.length - 1
    links.forEach((link, index) => link.classList.toggle('is-active', index === current))
  }
  const schedule = () => {
    frame ||= requestAnimationFrame(update)
  }
  addEventListener('scroll', schedule, {passive: true})
  addEventListener('resize', schedule)
  update()
})()
