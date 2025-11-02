// HTML & JS Feature Blocks Vite plugin extracted for reuse/maintenance
// Enables conditional inclusion of sections delimited by feature markers.
// HTML markers: <!-- FEATURE:NAME --> ... <!-- /FEATURE:NAME -->
//   - Note: HTML-style markers are supported in .html files AND inside .js/.mjs files
//     (useful for embedded HTML such as lit-html templates). These are processed
//     at build-time and removed along with their contents when the feature is disabled.
// JS markers (either style):
//   // FEATURE:NAME            ... (code) ...            // /FEATURE:NAME
//   /* FEATURE:NAME */         ... (code) ...         /* /FEATURE:NAME */
// Usage: import { htmlFeatureBlocksPlugin } from './feature-blocks-plugin.js'
// and register in Vite plugins with the current env passed into the factory.

// Simple boolean coercion for env strings
function toBoolEnv(v, def) {
  if (v === undefined || v === null || v === '') return def
  const s = String(v).trim().toLowerCase()
  return s === '1' || s === 'true' || s === 'yes' || s === 'on' || s === 'y'
}

export function htmlFeatureBlocksPlugin(env) {
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
    const v = env[`VITE_FEATURE_HTML_${key}`] ?? env[`VITE_FEATURE_${key}`]
    return toBoolEnv(v, defaultValue)
  }
  function escapeRegExp(str) {
    return str.replace(/[.*+?^${}()|[\]\\]/g, '\\$&')
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
    const re = /<!--\s*FEATURE:([\w\-.:\s]+)\s*-->[\s\S]*?<!--\s*\/FEATURE:\1\s*-->/gi
    return processWithRegex(
      html,
      re,
      (esc) => new RegExp(`<!--\\s*FEATURE:${esc}\\s*-->`, 'gi'),
      (esc) => new RegExp(`<!--\\s*\\/FEATURE:${esc}\\s*-->`, 'gi'),
    )
  }

  function processJs(code) {
    // First, also process HTML-style feature markers within JS files (for embedded HTML templates)
    code = processHtml(code)

    // Block comment markers
    const blockRe = /\/\*\s*FEATURE:([\w\-.:\s]+)\s*\*\/[\s\S]*?\/\*\s*\/FEATURE:\1\s*\*\//gi
    code = processWithRegex(
      code,
      blockRe,
      (esc) => new RegExp(`/\\*\\s*FEATURE:${esc}\\s*\\*/`, 'gi'),
      (esc) => new RegExp(`/\\*\\s*\\/FEATURE:${esc}\\s*\\*/`, 'gi'),
    )
    // Line comment markers (must use m flag to anchor at line starts)
    const lineRe = /^\s*\/\/\s*FEATURE:([\w\-.:\s]+)\s*$[\s\S]*?^\s*\/\/\s*\/FEATURE:\1\s*$/gim
    code = code.replace(lineRe, (match, rawName) => {
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
    return code
  }

  return {
    name: 'html-feature-blocks',
    transformIndexHtml(html) {
      return processHtml(html)
    },
    transform(code, id) {
      if (!id) return null
      if (/\.(m?js)($|\?)/i.test(id)) {
        const out = processJs(code)
        return out === code ? null : { code: out, map: null }
      }
      return null
    },
  }
}
