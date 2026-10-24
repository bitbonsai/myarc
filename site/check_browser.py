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
    for width in (1440, 768, 390, 320):
        browser('set', 'viewport', str(width), '1000')
        result = browser(expression="""(() => {
          const base = parseFloat(getComputedStyle(document.body).fontSize)
          const text = Array.from(document.querySelectorAll('h1,h2,h3,p,dt,dd,a')).filter(el => el.getClientRects().length)
          const minimum = Math.min(...text.map(el => parseFloat(getComputedStyle(el).fontSize)))
          if (base !== 16 || minimum < 14) throw new Error('Typography size regression')
          if (document.documentElement.scrollWidth > innerWidth) throw new Error('Horizontal overflow')
          for (const el of text) {
            const range = document.createRange()
            range.selectNodeContents(el)
            for (const rect of range.getClientRects()) {
              if (rect.width && (rect.left < -1 || rect.right > innerWidth + 1)) throw new Error('Clipped text: ' + el.textContent.slice(0, 60))
            }
          }
          return {width: innerWidth, base, minimum}
        })()""")
        print('Layout:', result)
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
