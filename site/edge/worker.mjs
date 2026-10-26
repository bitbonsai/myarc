// A specific Markdown range must beat HTML. Wildcards alone never opt in.
export const prefersMarkdown = accept => {
  if (!accept || accept.length > 8192) return false
  const ranges = accept.toLowerCase().split(',')
  if (ranges.length > 64) return false
  const choices = { 'text/markdown': { specificity: -1, q: 0 }, 'text/html': { specificity: -1, q: 0 } }

  for (const range of ranges) {
    const [type, ...parameters] = range.trim().split(';').map(part => part.trim())
    if (!/^(?:[a-z0-9!#$%&'*+.^_`|~-]+|\*)\/(?:[a-z0-9!#$%&'*+.^_`|~-]+|\*)$/.test(type)) return false
    let q = 1
    let weighted = false
    let matches = true
    let parameterCount = 0
    for (const parameter of parameters) {
      const match = /^([^=\s]+)\s*=\s*(.+)$/.exec(parameter)
      if (!match) return false
      const [, name, value] = match
      if (name === 'q') {
        if (weighted || !/^(?:0(?:\.\d{0,3})?|1(?:\.0{0,3})?)$/.test(value)) return false
        q = Number(value)
        weighted = true
      } else if (!weighted) {
        // Only UTF-8 is offered. Unknown media parameters do not match it.
        matches &&= name === 'charset' && (value === 'utf-8' || value === '"utf-8"')
        parameterCount++
        if (parameterCount > 1) return false
      }
    }
    if (!matches) continue
    for (const [offered, choice] of Object.entries(choices)) {
      const specificity = type === offered ? 2 : type === 'text/*' ? 1 : type === '*/*' ? 0 : -1
      if (specificity < 0) continue
      const rank = specificity * 100 + parameterCount
      if (rank > choice.specificity || (rank === choice.specificity && q > choice.q)) {
        choice.specificity = rank
        choice.q = q
      }
    }
  }
  const markdown = choices['text/markdown']
  return markdown.specificity >= 200 && markdown.q > choices['text/html'].q
}

export const markdownPath = path => {
  if (path.endsWith('/index.html')) return path.slice(0, -10) + 'index.md'
  // Encoded names and dotted paths stay untouched, including all direct .md URLs.
  if (path.includes('.') || path.includes('%')) return null
  return path.endsWith('/') ? path + 'index.md' : path + '/index.md'
}

const pageResponse = (response, request, location) => {
  const headers = new Headers(response.headers)
  const vary = headers.get('Vary')
  if (!vary?.split(',').some(value => ['accept', '*'].includes(value.trim().toLowerCase()))) {
    headers.set('Vary', vary ? vary + ', Accept' : 'Accept')
  }
  // Shared caches ignore arbitrary Vary keys; browsers may retain and revalidate variants.
  headers.set('Cache-Control', 'private, no-cache')
  headers.delete('CDN-Cache-Control')
  headers.delete('Cloudflare-CDN-Cache-Control')
  if (location) {
    headers.set('Content-Type', 'text/markdown;charset=utf-8')
    headers.set('Content-Location', location)
  }
  return new Response(request.method === 'HEAD' ? null : response.body, {
    status: response.status,
    statusText: response.statusText,
    headers
  })
}

export const handleRequest = async (request, originFetch = fetch) => {
  const url = new URL(request.url)
  const path = markdownPath(url.pathname)
  if (!['GET', 'HEAD'].includes(request.method) || !path) return originFetch(request)

  if (prefersMarkdown(request.headers.get('Accept'))) {
    const markdown = new URL(url)
    markdown.pathname = path
    const headers = new Headers(request.headers)
    headers.set('Accept', 'text/markdown')
    // Client validators/ranges refer to the public URL, possibly its HTML variant.
    for (const name of ['If-Match', 'If-None-Match', 'If-Modified-Since', 'If-Unmodified-Since', 'If-Range', 'Range']) headers.delete(name)
    const response = await originFetch(new Request(markdown, {
      method: request.method, headers, redirect: 'manual', signal: request.signal
    }), { cache: 'no-store' })
    if (response.status !== 404) {
      return pageResponse(response, request, response.status === 200 ? markdown.pathname + markdown.search : null)
    }
    await response.body?.cancel()
  }
  const response = await originFetch(request, { cache: 'no-store' })
  return pageResponse(response, request)
}

export default { fetch: request => handleRequest(request) }
