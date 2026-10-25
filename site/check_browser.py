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
finally:
    subprocess.run(['agent-browser', '--session', SESSION, 'close'], capture_output=True)
    subprocess.run(['pkill', '-f', '/[a]gent-browser-darwin-arm64'], capture_output=True)
    subprocess.run(['pkill', '-f', '[G]oogle Chrome for Testing'], capture_output=True)
