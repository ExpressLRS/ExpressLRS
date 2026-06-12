// Feature marker processor shared by build-time plugins.
//
// Supported markers:
// - HTML: <!-- FEATURE:NAME --> ... <!-- /FEATURE:NAME -->
// - JS/CSS line: // FEATURE:NAME ... // /FEATURE:NAME
// - JS/CSS block: /* FEATURE:NAME */ ... /* /FEATURE:NAME */
//
// Feature names map to VITE_FEATURE_<NAME> env vars after normalization.
// "NOT NAME" and "!NAME" invert the sense of the flag.

// Simple boolean coercion for env strings
function toBoolEnv(v, def) {
  if (v === undefined || v === null || v === '') return def
  const s = String(v).trim().toLowerCase()
  return s === '1' || s === 'true' || s === 'yes' || s === 'on' || s === 'y'
}

export function createFeatureBlockProcessor(env) {
  // Map free-form feature names such as "NOT IS_TX" inputs down to the
  // environment variable shape used by the build scripts.
  function normalizeName(name) {
    return String(name).trim().replace(/[^A-Za-z0-9]+/g, '_').toUpperCase()
  }

  function parseRawName(raw) {
    let s = String(raw).trim()
    let invert = false
    if (/^NOT\b/i.test(s)) {
      invert = true
      s = s.replace(/^NOT\b\s*/i, '')
    }
    if (s.startsWith('!')) {
      invert = true
      s = s.replace(/^!+\s*/, '')
    }
    return { name: s, invert }
  }

  function getFlagForName(name, defaultValue) {
    const key = normalizeName(name)
    // Add any other "derived" flags here, check features.js
    if (name === 'HAS_SUBGHZ') {
      return getFlagForName('HAS_LR1121', false) || getFlagForName('HAS_SX127X', false)
    }
    const v = env[`VITE_FEATURE_HTML_${key}`] ?? env[`VITE_FEATURE_${key}`]
    return toBoolEnv(v, defaultValue)
  }

  function escapeRegExp(str) {
    return str.replace(/[.*+?^${}()|[\]\\]/g, '\\$&')
  }

  // Feature blocks can be nested, so keep running the same pass until no
  // further replacements occur.
  function repeatUntilStable(input, processor) {
    let current = input
    while (true) {
      const next = processor(current)
      if (next === current) return current
      current = next
    }
  }

  function processWithRegex(code, re, openerFactory, closerFactory) {
    return code.replace(re, (match, rawName) => {
      const { name, invert } = parseRawName(rawName)
      const flag = getFlagForName(name, false)
      const keep = invert ? !flag : flag
      if (!keep) return ''
      const esc = escapeRegExp(rawName)
      const open = openerFactory(esc)
      const close = closerFactory(esc)
      return match.replace(open, '').replace(close, '')
    })
  }

  function processHtml(html) {
    return repeatUntilStable(html, (current) => {
      const re = /<!--\s*FEATURE:([\w\-.:\s]+)\s*-->[\s\S]*?<!--\s*\/FEATURE:\1\s*-->/gi
      return processWithRegex(
        current,
        re,
        (esc) => new RegExp(`<!--\\s*FEATURE:${esc}\\s*-->`, 'gi'),
        (esc) => new RegExp(`<!--\\s*\\/FEATURE:${esc}\\s*-->`, 'gi'),
      )
    })
  }

  function processJs(code) {
    // Lit templates live inside JS modules, so handle embedded HTML markers too.
    code = processHtml(code)

    // Block comment markers are the CSS-safe form, so this path is reused by
    // both JS and CSS transforms.
    code = repeatUntilStable(code, (current) => {
      const blockRe = /\/\*\s*FEATURE:([\w\-.:\s]+)\s*\*\/[\s\S]*?\/\*\s*\/FEATURE:\1\s*\*\//gi
      return processWithRegex(
        current,
        blockRe,
        (esc) => new RegExp(`/\\*\\s*FEATURE:${esc}\\s*\\*/`, 'gi'),
        (esc) => new RegExp(`/\\*\\s*\\/FEATURE:${esc}\\s*\\*/`, 'gi'),
      )
    })
    // Line markers are JS-only in practice, but harmless to scan for here.
    code = repeatUntilStable(code, (current) => {
      const lineRe = /^\s*\/\/\s*FEATURE:([\w\-.:\s]+)\s*$[\s\S]*?^\s*\/\/\s*\/FEATURE:\1\s*$/gim
      return current.replace(lineRe, (match, rawName) => {
        const { name, invert } = parseRawName(rawName)
        const flag = getFlagForName(name, false)
        const keep = invert ? !flag : flag
        if (!keep) return ''
        // Remove only the opening/closing marker lines
        const esc = escapeRegExp(rawName)
        const open = new RegExp(`^\\s*\/\/\\s*FEATURE:${esc}\\s*$`, 'gim')
        const close = new RegExp(`^\\s*\/\/\\s*\\/FEATURE:${esc}\\s*$`, 'gim')
        return match.replace(open, '').replace(close, '')
      })
    })
    return code
  }

  return { processHtml, processJs }
}

// Vite wrapper around the shared processor. Keep this thin so other plugins
// can reuse the same feature evaluation rules without duplicating logic.
export function htmlFeatureBlocksPlugin(env) {
  const { processHtml, processJs } = createFeatureBlockProcessor(env)

  return {
    name: 'html-feature-blocks',
    enforce: 'pre',
    transformIndexHtml(html) {
      return processHtml(html)
    },
    transform(code, id) {
      if (!id) return null
      if (/\.(m?js|css)($|\?)/i.test(id)) {
        const out = processJs(code)
        return out === code ? null : { code: out, map: null }
      }
      return null
    },
  }
}
