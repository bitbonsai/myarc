import assert from 'node:assert/strict'
import { test } from 'node:test'
import worker, { handleRequest, markdownPath, prefersMarkdown } from './worker.mjs'

const request = (path = '/', options = {}) => new Request('https://myarc.dev' + path, {
  headers: { Accept: 'text/markdown' }, ...options
})
const mockOrigin = (reply = () => new Response('# origin', {
  headers: { 'Content-Type': 'text/plain', ETag: '"markdown"', Vary: 'Accept-Encoding', 'X-Origin': 'pages' }
})) => {
  const calls = []
  const fetch = async (input, options) => {
    calls.push({ request: input, options })
    return reply(input, calls.length)
  }
  return { calls, fetch }
}

test('explicit Markdown preference, quality, specificity and HTML ties', () => {
  const cases = [
    ['text/markdown', true],
    ['TEXT/MARKDOWN;Q=0.9,text/html;q=0.5', true],
    ['text/markdown;q=0.5,text/html;q=0.9', false],
    ['text/markdown,text/html', false],
    ['text/markdown;q=0', false],
    ['text/markdown;q=0,*/*;q=1', false],
    ['text/markdown;q=0.8,text/*;q=0.9', false],
    ['text/markdown;q=0.8,text/html;q=0,text/*;q=1', true],
    ['text/markdown,text/html;q=0.5,*/*;q=1', true],
    ['text/markdown;q=0.5,*/*;q=1', false],
    ['text/*', false],
    ['*/*', false],
    ['text/html,application/xhtml+xml,application/xml;q=0.9,*/*;q=0.8', false],
    ['application/json', false],
    ['', false],
    [null, false],
    ['text/markdown; charset="utf-8"', true],
    ['text/markdown; charset=iso-8859-1', false],
    ['text/markdown; version=2', false],
    ['text/markdown;q=1,text/markdown;charset=utf-8;q=0', false],
    ['text/markdown;q=0,text/markdown;q=1', true]
  ]
  for (const [accept, expected] of cases) assert.equal(prefersMarkdown(accept), expected, String(accept))
})

test('malformed and oversized Accept conservatively keep origin HTML', () => {
  for (const accept of [
    'text/markdown;q=2', 'text/markdown;q=-1', 'text/markdown;q=NaN',
    'text/markdown;q=0.1234', 'text/markdown;q=0.5;q=1', 'text/markdown;bad',
    'text/markdown,broken', 'text/markdown' + ' '.repeat(8192),
    Array(65).fill('text/markdown').join(','),
    'text/markdown;' + Array(200).fill('charset=utf-8').join(';')
  ]) assert.equal(prefersMarkdown(accept), false, accept.slice(0, 100))
})

test('generated directory representations and index.html aliases map without host changes', () => {
  for (const [path, expected] of [
    ['/', '/index.md'], ['/index.html', '/index.md'],
    ['/blog', '/blog/index.md'], ['/blog/', '/blog/index.md'],
    ['/blog/index.html', '/blog/index.md'],
    ['/blog/2026-10-26-update/', '/blog/2026-10-26-update/index.md'],
    ['/favicon.png', null], ['/blog/index.md', null], ['/llms.txt', null],
    ['/blog/_assets/image.webp', null], ['/some%2Easset', null]
  ]) assert.equal(markdownPath(path), expected, path)
})

test('Markdown streams origin bytes, preserves safe headers, query and metadata', async () => {
  const body = new ReadableStream({ start: controller => {
    controller.enqueue(new TextEncoder().encode('# origin'))
    controller.close()
  } })
  const originResponse = new Response(body, { headers: {
    'Content-Type': 'text/plain', ETag: '"markdown"', Vary: 'Accept-Encoding', 'X-Origin': 'pages'
  } })
  const origin = mockOrigin(() => originResponse)
  const response = await handleRequest(request('/blog/?source=test&x=1'), origin.fetch)
  assert.equal(response.body, body)
  assert.equal(originResponse.bodyUsed, false)
  assert.equal(origin.calls.length, 1)
  const call = origin.calls[0]
  assert.equal(call.request.url, 'https://myarc.dev/blog/index.md?source=test&x=1')
  assert.equal(call.request.redirect, 'manual')
  assert.equal(call.request.headers.get('Accept'), 'text/markdown')
  assert.equal(response.headers.get('Content-Type'), 'text/markdown;charset=utf-8')
  assert.equal(response.headers.get('Content-Location'), '/blog/index.md?source=test&x=1')
  assert.equal(response.headers.get('ETag'), '"markdown"')
  assert.equal(response.headers.get('X-Origin'), 'pages')
  assert.equal(await response.text(), '# origin')
})

test('missing Markdown falls back to original HTML URL and headers', async () => {
  const input = request('/blog/missing/?q=one')
  const origin = mockOrigin((input, count) => new Response(count === 1 ? 'missing' : '<h1>HTML</h1>', {
    status: count === 1 ? 404 : 200, headers: { 'Content-Type': 'text/html' }
  }))
  const response = await handleRequest(input, origin.fetch)
  assert.equal(origin.calls.length, 2)
  assert.equal(origin.calls[1].request, input)
  assert.equal(response.headers.get('Content-Location'), null)
  assert.equal(response.headers.get('Content-Type'), 'text/html')
  assert.equal(response.headers.get('Vary'), 'Accept')
  assert.equal(await response.text(), '<h1>HTML</h1>')
})

test('fallback preserves final origin 404 and HTML conditional responses', async () => {
  const missing = mockOrigin(() => new Response('origin not found', { status: 404 }))
  const response = await handleRequest(request('/unknown/'), missing.fetch)
  assert.equal(missing.calls.length, 2)
  assert.equal(response.status, 404)
  assert.equal(await response.text(), 'origin not found')

  const input = request('/', { headers: { Accept: 'text/html', 'If-None-Match': '"html"' } })
  const unchanged = mockOrigin(() => new Response(null, { status: 304, headers: { ETag: '"html"' } }))
  const conditional = await handleRequest(input, unchanged.fetch)
  assert.equal(unchanged.calls[0].request, input)
  assert.equal(conditional.status, 304)
  assert.equal(conditional.body, null)
  assert.equal(conditional.headers.get('ETag'), '"html"')
  assert.equal(conditional.headers.get('Vary'), 'Accept')
})

test('other origin failures and redirects retain status, body and headers without fallback', async () => {
  for (const status of [301, 403, 410, 429, 500, 503]) {
    const origin = mockOrigin(() => new Response('origin result', {
      status, headers: { Location: '/maintenance', 'Retry-After': '60' }
    }))
    const response = await handleRequest(request(), origin.fetch)
    assert.equal(origin.calls.length, 1)
    assert.equal(response.status, status)
    assert.equal(response.headers.get('Location'), '/maintenance')
    assert.equal(response.headers.get('Retry-After'), '60')
    assert.equal(response.headers.get('Content-Location'), null)
    assert.equal(await response.text(), 'origin result')
  }
  const failure = new Error('origin unavailable')
  await assert.rejects(handleRequest(request(), async () => { throw failure }), failure)
})

test('negotiated variants permit browser revalidation but bypass shared storage and origin cache reads', async () => {
  for (const accept of ['text/markdown', 'text/html', '*/*']) {
    const origin = mockOrigin(() => new Response('origin', { headers: {
      'Cache-Control': 'public, max-age=600', 'CDN-Cache-Control': 'max-age=600',
      'Cloudflare-CDN-Cache-Control': 'max-age=600', Vary: 'Accept-Encoding'
    } }))
    const response = await handleRequest(request('/', { headers: { Accept: accept } }), origin.fetch)
    assert.equal(origin.calls[0].options.cache, 'no-store')
    assert.equal(response.headers.get('Cache-Control'), 'private, no-cache')
    assert.equal(response.headers.get('CDN-Cache-Control'), null)
    assert.equal(response.headers.get('Cloudflare-CDN-Cache-Control'), null)
    assert.equal(response.headers.get('Vary'), 'Accept-Encoding, Accept')
    assert.equal(new URL(origin.calls[0].request.url).pathname, accept === 'text/markdown' ? '/index.md' : '/')
  }
})

test('Vary Accept is case-insensitive, deduplicated, and wildcard is preserved', async () => {
  for (const vary of ['accept', 'Accept-Encoding, ACCEPT', '*']) {
    const origin = mockOrigin(() => new Response('origin', { headers: { Vary: vary } }))
    const response = await handleRequest(request(), origin.fetch)
    assert.equal(response.headers.get('Vary'), vary)
  }
})

test('Markdown requests remove HTML validators and range without changing incoming request', async () => {
  const headers = { Accept: 'text/markdown', 'If-None-Match': '"html"',
    'If-Modified-Since': 'yesterday', 'If-Match': '"html"', 'If-Unmodified-Since': 'yesterday',
    'If-Range': '"html"', Range: 'bytes=0-5', 'Accept-Encoding': 'gzip' }
  const input = request('/', { headers })
  const origin = mockOrigin()
  await handleRequest(input, origin.fetch)
  for (const name of Object.keys(headers).filter(name => name.startsWith('If-') || name === 'Range')) {
    assert.equal(origin.calls[0].request.headers.get(name), null, name)
    assert.ok(input.headers.has(name), name)
  }
  assert.equal(origin.calls[0].request.headers.get('Accept-Encoding'), 'gzip')
})

test('HEAD retains HEAD and query for Markdown, HTML and missing representation fallback', async () => {
  for (const accept of ['text/markdown', 'text/html']) {
    for (const missing of [false, true]) {
      const origin = mockOrigin((input, count) => new Response(null, {
        status: missing && count === 1 && accept === 'text/markdown' ? 404 : 200,
        headers: { 'Content-Length': '42' }
      }))
      const response = await handleRequest(request('/blog/?q=1', { method: 'HEAD', headers: { Accept: accept } }), origin.fetch)
      assert.equal(response.body, null)
      assert.equal(response.headers.get('Content-Length'), '42')
      assert.equal(response.headers.get('Vary'), 'Accept')
      for (const call of origin.calls) {
        assert.equal(call.request.method, 'HEAD')
        assert.equal(new URL(call.request.url).search, '?q=1')
      }
    }
  }
})

test('assets, direct Markdown and other methods pass through unchanged', async () => {
  const inputs = ['/index.md', '/blog/index.md', '/llms.txt', '/favicon.ico', '/blog/_assets/gallery.js'].map(path => request(path))
  for (const method of ['POST', 'PUT', 'DELETE', 'OPTIONS', 'PATCH']) inputs.push(request('/', { method }))
  for (const input of inputs) {
    const original = new Response('unchanged', { headers: { 'Cache-Control': 'max-age=600' } })
    const origin = mockOrigin(() => original)
    assert.equal(await handleRequest(input, origin.fetch), original)
    assert.equal(origin.calls.length, 1)
    assert.equal(origin.calls[0].request, input)
    assert.equal(origin.calls[0].options, undefined)
  }
})

test('module exports Worker fetch entry point', () => {
  assert.equal(typeof worker.fetch, 'function')
})
