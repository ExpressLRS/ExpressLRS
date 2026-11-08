// Build plugin: normalize CRLF/CR line endings to LF in all text inputs.
//
// Windows checkouts (git core.autocrlf=true) turn every source file into CRLF.
// That changes the bytes Vite/Rolldown hash and emit (index.html, CSS, SVG,
// template literals), so the generated firmware header differs from a Linux
// build. Normalizing at the very start of the pipeline makes the output
// independent of the checkout's line endings.

const EOL_RE = /\r\n?/g
const TEXT_RE = /\.(m?js|css|html|svg|json)($|\?)/i

export function normalizeEolPlugin() {
  return {
    name: 'normalize-eol',
    enforce: 'pre',
    transformIndexHtml: {
      order: 'pre',
      handler(html) {
        return html.replace(EOL_RE, '\n')
      },
    },
    transform(code, id) {
      if (!id || id.includes('/node_modules/') || !TEXT_RE.test(id)) return null
      if (!code.includes('\r')) return null
      return { code: code.replace(EOL_RE, '\n'), map: null }
    },
  }
}
