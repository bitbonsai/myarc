"""Exercise the standalone page offline. Requires agent-browser and Chrome."""
from pathlib import Path
import json
import subprocess

ROOT = Path(__file__).resolve().parent
SESSION = 'myarc-landing'


def browser(*args, expression=None):
    command = ['agent-browser', '--session', SESSION, '--json', *args]
    if expression is not None:
        command += ['eval', '--stdin']
    result = subprocess.run(command, input=expression, capture_output=True, text=True, timeout=30)
    if result.returncode:
        raise RuntimeError(result.stderr or result.stdout)
    data = json.loads(result.stdout)
    if not data.get('success'):
        raise RuntimeError(data.get('error'))
    return data.get('data', {}).get('result')


try:
    browser('open', (ROOT / 'index.html').as_uri())
    browser('set', 'media', 'light')
    browser('set', 'offline', 'on')
    browser('reload')
    browser(expression="""(async () => {
      await document.fonts.ready
      await Promise.all(Array.from(document.images).map(image => image.decode()))
      await Promise.all(document.getAnimations().filter(a => Number.isFinite(a.effect.getTiming().iterations)).map(a => a.finished))
      if (!Array.from(document.fonts).some(font => font.family === 'Outfit' && font.status === 'loaded')) throw new Error('Outfit missing')
      if (performance.getEntriesByType('resource').some(r => /^https?:/.test(r.name))) throw new Error('External resource request')
      return true
    })()""")
    for width in (1440, 1024, 900, 768, 390, 320):
        browser('set', 'viewport', str(width), '1000')
        result = browser(expression="""(() => {
          const base = parseFloat(getComputedStyle(document.body).fontSize)
          const text = Array.from(document.querySelectorAll('h1,h2,h3,p,dt,dd,a')).filter(el => el.getClientRects().length)
          const minimum = Math.min(...text.map(el => parseFloat(getComputedStyle(el).fontSize)))
          if (base !== 18 || minimum < 16) throw new Error('Typography size regression')
          if (innerWidth <= 768 && getComputedStyle(document.querySelector('.wrap')).paddingLeft !== '24px') throw new Error('Mobile site gutter')
          if (document.documentElement.scrollWidth > innerWidth) throw new Error('Horizontal overflow')
          for (const el of text) {
            const range = document.createRange()
            range.selectNodeContents(el)
            for (const rect of range.getClientRects()) {
              if (rect.width && (rect.left < -1 || rect.right > innerWidth + 1)) throw new Error('Clipped text: ' + el.textContent.slice(0, 60))
            }
          }
          for (const link of document.querySelectorAll('.top nav a:not(.button)')) {
            if (!link.getClientRects().length) continue
            const style = getComputedStyle(link)
            if (style.color !== 'rgb(24, 36, 50)' || style.textDecorationLine !== 'none') throw new Error('Header navigation style regression')
          }
          const panel = document.querySelector('.panel')
          const rows = Array.from(panel.querySelectorAll('.status dd'))
          if (Array.from(panel.querySelectorAll('.status dt')).some(label => getComputedStyle(label).fontSize !== '24px')) throw new Error('Status label hierarchy regression')
          const padding = getComputedStyle(panel)
          const artwork = panel.querySelector('.panel-art').getBoundingClientRect()
          const overlaps = rect => rect.left < artwork.right - 1 && rect.right > artwork.left + 1 && rect.top < artwork.bottom - 1 && rect.bottom > artwork.top + 1
          if (Array.from(panel.querySelectorAll('h2,.lead,dt,dd')).some(el => overlaps(el.getBoundingClientRect()))) throw new Error('Panel artwork overlaps text')
          if (document.querySelector('.split,.eyebrow')) throw new Error('Redundant sidebar headings returned')
          const headings = Array.from(document.querySelectorAll('main h2'))
          if (new Set(headings.map(h => getComputedStyle(h).fontSize)).size !== 1) throw new Error('Section heading sizes differ')
          for (const container of document.querySelectorAll('.top,.end')) {
            const ribbon = container.querySelector('.ribbon').getBoundingClientRect()
            const wordmark = container.querySelector('.wordmark').getBoundingClientRect()
            if (ribbon.right > wordmark.left || Math.abs(ribbon.width - ribbon.height) > 1) throw new Error('Logo lockup geometry')
          }
          if (padding.backgroundImage !== getComputedStyle(document.body).backgroundImage ||
              !padding.backgroundImage.startsWith('url("data:image/png;')) throw new Error('Panel grain missing')
          return {width: innerWidth, base, minimum, headings: 'consistent', logos: 'aligned', grain: 'shared'}
        })()""")
        print('Layout:', result)
    browser('set', 'viewport', '1440', '1000')
    browser('screenshot', '--full', '/tmp/myarc-theme-light.png')
    browser('set', 'media', 'dark')
    for width in (1440, 390):
        browser('set', 'viewport', str(width), '1000')
        print('Dark theme:', browser(expression="""(async () => {
          await Promise.all(Array.from(document.images).map(image => image.decode()))
          if (!matchMedia('(prefers-color-scheme: dark)').matches) throw new Error('System preference not detected')
          const body = getComputedStyle(document.body)
          if (body.backgroundColor !== 'rgb(11, 20, 32)' || body.color !== 'rgb(245, 242, 236)') throw new Error('Dark palette missing')
          for (const lockup of document.querySelectorAll('.top .mark,.end .mark')) {
            if (lockup.querySelector('.wordmark').currentSrc !== lockup.querySelector('source').srcset) throw new Error('Dark wordmark missing')
          }
          const luminance = color => {
            const rgb = color.match(/[\\d.]+/g).slice(0,3).map(Number).map(v => {
              v /= 255
              return v <= .04045 ? v / 12.92 : ((v + .055) / 1.055) ** 2.4
            })
            return rgb[0] * .2126 + rgb[1] * .7152 + rgb[2] * .0722
          }
          const background = luminance(body.backgroundColor)
          for (const el of [document.body, document.querySelector('.hero-copy p'), document.querySelector('.cta a:not(.button)')]) {
            const values = getComputedStyle(el).color.match(/[\\d.]+/g).map(Number)
            const alpha = values[3] ?? 1
            const bg = body.backgroundColor.match(/[\\d.]+/g).slice(0,3).map(Number)
            const blended = values.slice(0,3).map((v,i) => v * alpha + bg[i] * (1-alpha))
            if ((luminance('rgb(' + blended.join(',') + ')') + .05) / (background + .05) < 4.5) throw new Error('Dark text contrast')
          }
          if (document.documentElement.scrollWidth > innerWidth) throw new Error('Dark horizontal overflow')
          if (document.querySelectorAll('.button .github-icon[aria-hidden="true"]').length !== 2) throw new Error('GitHub CTA icons missing')
          for (const icon of document.querySelectorAll('.github-icon')) {
            if (icon.viewBox.baseVal.width !== 1024 || getComputedStyle(icon.querySelector('path')).fill !== 'rgb(255, 255, 255)') throw new Error('GitHub supplied mark or color regression')
          }
          return {width: innerWidth, preference: 'dark', wordmarks: 'ivory', contrast: 'AA'}
        })()"""))
        browser('screenshot', '--full', '/tmp/myarc-theme-dark-' + str(width) + '.png')
    browser('set', 'media', 'dark', 'reduced-motion')
    browser(expression="""(() => {
      if (document.getAnimations().some(a => a.playState === 'running')) throw new Error('Dark reduced motion failed')
      return true
    })()""")
    browser('set', 'media', 'light')
    browser('set', 'viewport', '1440', '1000')
    browser('hover', '.end-copy a')
    print('Hover:', browser(expression="""(async () => {
      const link = document.querySelector('.end-copy a')
      await Promise.all(link.getAnimations().map(a => a.finished))
      const style = getComputedStyle(link)
      const heart = getComputedStyle(document.querySelector('.heart'))
      if (style.color !== 'rgb(3, 75, 244)' || style.textUnderlineOffset !== '5px') throw new Error('Link hover regression')
      if (heart.animationName !== 'heartbeat' || heart.animationDuration !== '1.5s') throw new Error('Heartbeat regression')
      return {color: style.color, underline: style.textUnderlineOffset, bpm: 60 / parseFloat(heart.animationDuration)}
    })()"""))
    browser('hover', 'h1')
    browser('focus', '.end .mark')
    browser('press', 'Tab')
    browser(expression="""(() => {
      if (!document.querySelector('.end-copy a').matches(':focus-visible')) throw new Error('Keyboard focus missing')
      if (getComputedStyle(document.querySelector('.heart')).animationName !== 'heartbeat') throw new Error('Focus heartbeat missing')
      return true
    })()""")
    browser('set', 'media', 'light', 'reduced-motion')
    print('Reduced motion:', browser(expression="""(() => {
      if (!matchMedia('(prefers-reduced-motion: reduce)').matches) throw new Error('Reduced motion emulation failed')
      if (document.getAnimations().some(a => a.playState === 'running')) throw new Error('Animation still running')
      if (getComputedStyle(document.documentElement).scrollBehavior !== 'auto') throw new Error('Smooth scroll still active')
      for (const link of document.querySelectorAll('a')) {
        if (getComputedStyle(link).transitionDuration !== '0s') throw new Error('Link transition still active')
      }
      return 'PASS'
    })()"""))
    for page in [(ROOT / 'blog/index.html'), *sorted((ROOT / 'blog').glob('*/index.html'))]:
        browser('open', page.as_uri())
        for theme in ('light', 'dark'):
            browser('set', 'media', theme)
            for width in (1440, 1024, 768, 390, 320):
                browser('set', 'viewport', str(width), '1000')
                result = browser(expression="""(async () => {
                  await document.fonts.ready
                  await Promise.all(Array.from(document.images).map(image => {
                    image.loading = 'eager'
                    return image.decode()
                  }))
                  if (!Array.from(document.fonts).some(font => font.family === 'Outfit' && font.status === 'loaded')) throw new Error('Blog Outfit not loaded')
                  if (document.querySelectorAll('h1').length !== 1) throw new Error('Blog heading structure')
                  if (innerWidth <= 768 && getComputedStyle(document.querySelector('.wrap')).paddingLeft !== '24px') throw new Error('Mobile blog gutter')
                  if (document.documentElement.scrollWidth > innerWidth) throw new Error('Blog overflow')
                  const nav = document.querySelector('.blog-link')
                  if (!nav.getClientRects().length) throw new Error('Blog navigation hidden')
                  for (const el of document.querySelectorAll('main h1,main h2,main h3,main p,main li,main time,main code,main figcaption')) {
                    const style = getComputedStyle(el)
                    if (parseFloat(style.fontSize) < 16 || !style.fontFamily.includes('Outfit')) throw new Error('Blog typography regression')
                    const rect = el.getBoundingClientRect()
                    if (rect.left < -1 || rect.right > innerWidth + 1) throw new Error('Blog content clipped')
                  }
                  if (performance.getEntriesByType('resource').some(r => /^https?:/.test(r.name))) throw new Error('Blog remote resource request')
                  const canonical = document.querySelector('link[rel="canonical"]').href
                  if (!canonical.startsWith('https://myarc.dev/blog/')) throw new Error('Blog canonical URL')
                  const first = document.querySelector('.post-body img')
                  if (first) {
                    const expected = new URL(first.getAttribute('src'), canonical).href
                    for (const selector of ['meta[property="og:image"]', 'meta[name="twitter:image"]']) {
                      if (document.querySelector(selector).content !== expected) throw new Error('Post social image mismatch')
                    }
                    if (Number(document.querySelector('meta[property="og:image:width"]').content) !== Number(first.getAttribute('width'))) throw new Error('Social image dimensions')
                  }
                  const schemas = Array.from(document.querySelectorAll('script[type="application/ld+json"]')).flatMap(node => JSON.parse(node.textContent)['@graph'] || [])
                  if (!schemas.some(node => node['@type'] === 'BreadcrumbList')) throw new Error('Breadcrumb schema missing')
                  if (!document.querySelector('link[rel="alternate"][type="text/markdown"]')) throw new Error('Markdown discovery missing')
                  const thumbnail = document.querySelector('.post-thumb img')
                  if (thumbnail) {
                    const image = thumbnail.getBoundingClientRect()
                    const title = thumbnail.closest('.post-preview').querySelector('h2').getBoundingClientRect()
                    if (image.right > title.left) throw new Error('Thumbnail overlaps title')
                    if (thumbnail.naturalWidth > 384) throw new Error('Thumbnail not optimized')
                  }
                  for (const card of document.querySelectorAll('.post-preview')) {
                    const title = card.querySelector('h2').getBoundingClientRect()
                    const date = card.querySelector('.post-date').getBoundingClientRect()
                    const summary = card.querySelector('.post-summary')
                    const rect = summary.getBoundingClientRect()
                    const style = getComputedStyle(summary)
                    if (date.top < title.bottom - 1 || rect.top < date.bottom - 1) throw new Error('Post text order')
                    if (Math.abs(title.left - date.left) > 1 || Math.abs(title.left - rect.left) > 1) throw new Error('Post text alignment')
                    if (style.whiteSpace !== 'nowrap' || style.textOverflow !== 'ellipsis' || style.overflowX !== 'hidden') throw new Error('Abstract truncation missing')
                    if (rect.height > parseFloat(style.lineHeight) + 1) throw new Error('Abstract wraps')
                  }
                  const luminance = color => {
                    const values = color.match(/[\\d.]+/g).slice(0, 3).map(Number).map(value => {
                      value /= 255
                      return value <= .04045 ? value / 12.92 : ((value + .055) / 1.055) ** 2.4
                    })
                    return values[0] * .2126 + values[1] * .7152 + values[2] * .0722
                  }
                  for (const month of document.querySelectorAll('.post-month')) {
                    const foreground = luminance(getComputedStyle(month).color)
                    const background = luminance(getComputedStyle(document.body).backgroundColor)
                    if ((Math.max(foreground, background) + .05) / (Math.min(foreground, background) + .05) < 4.5) throw new Error('Month text contrast')
                  }
                  return {width: innerWidth, title: document.title, images: document.images.length}
                })()""")
                print('Blog:', theme, result)
                if width == 1440:
                    name = 'index' if page.parent.name == 'blog' else 'post'
                    browser('screenshot', '/tmp/myarc-blog-' + name + '-' + theme + '.png')
        has_caption = browser(expression="Boolean(document.querySelector('.post-body figcaption'))")
        if has_caption:
            browser('set', 'viewport', '1440', '1000')
            for theme in ('light', 'dark'):
                browser('set', 'media', theme)
                browser('focus', '.top .mark')
                browser('scrollintoview', '.post-body figcaption')
                browser('mouse', 'move', '0', '0')
                print('Caption rest:', browser(expression="""(async () => {
                  const caption = document.querySelector('.post-body figcaption')
                  await Promise.all(caption.getAnimations().map(animation => animation.finished))
                  const style = getComputedStyle(caption)
                  if (style.fontSize !== '16px' || style.fontStyle !== 'normal' || style.textAlign !== 'center') throw new Error('Caption typography')
                  if (style.color !== getComputedStyle(document.querySelector('.post-date')).color) throw new Error('Caption muted color')
                  return {color: style.color, fontSize: style.fontSize}
                })()"""))
                browser('screenshot', '/tmp/myarc-caption-rest-' + theme + '.png')
                browser('hover', '.post-body figure:has(figcaption)')
                print('Caption hover:', browser(expression="""(async () => {
                  const caption = document.querySelector('.post-body figcaption')
                  const image = caption.parentElement.querySelector('img')
                  await Promise.all([...caption.getAnimations(), ...image.getAnimations()].map(animation => animation.finished))
                  if (getComputedStyle(caption).color !== getComputedStyle(document.body).color || getComputedStyle(image).opacity !== '1') throw new Error('Figure hover treatment')
                  return 'PASS'
                })()"""))
                browser('screenshot', '/tmp/myarc-caption-hover-' + theme + '.png')
                browser('mouse', 'move', '0', '0')
                browser('focus', '.post-body figure:has(figcaption) a')
                browser(expression="""(async () => {
                  const caption = document.querySelector('.post-body figcaption')
                  await Promise.all(caption.getAnimations().map(animation => animation.finished))
                  if (!caption.parentElement.matches(':focus-within') || getComputedStyle(caption).color !== getComputedStyle(document.body).color) throw new Error('Figure focus treatment')
                  return true
                })()""")
        browser('set', 'media', 'light', 'reduced-motion')
        browser('focus', '.skip')
        browser('press', 'Tab')
        browser(expression="""(() => {
          if (!document.activeElement.matches(':focus-visible')) throw new Error('Blog keyboard focus missing')
          if (document.getAnimations().some(a => a.playState === 'running')) throw new Error('Blog reduced motion failed')
          for (const element of document.querySelectorAll('.post-body figure img,.post-body figcaption')) {
            if (getComputedStyle(element).transitionDuration !== '0s') throw new Error('Figure reduced motion failed')
          }
          return true
        })()""")
finally:
    subprocess.run(['agent-browser', '--session', SESSION, 'close'], capture_output=True)
    subprocess.run(['pkill', '-f', '/[a]gent-browser-darwin-arm64'], capture_output=True)
    subprocess.run(['pkill', '-f', '[G]oogle Chrome for Testing'], capture_output=True)
