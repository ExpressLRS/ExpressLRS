import postcss from 'postcss'
import { promises as fs } from 'fs'
import path from 'path'

import { createFeatureBlockProcessor } from './feature-blocks-plugin.js'

// Conservative CSS tree-shaker used for two different jobs:
// - build: prune emitted CSS against the final target-specific bundle
// - dev: prune source CSS against the current target feature set so the UI can
//   be tested locally without reflashing hardware
//
// The heuristic is intentionally simple: extract identifier-like tokens from
// HTML/JS, then remove selectors that require tokens that never appear.

function formatBytes(bytes) {
  return `${bytes} bytes (${(bytes / 1024).toFixed(2)} KiB)`
}

// Classes assembled dynamically at runtime (e.g. `${type}-bg`) won't appear as
// full tokens in static source scans, so keep them explicitly.
const SAFELIST_TOKENS = new Set([
  'error-bg',
  'error-btn',
  'info-bg',
  'info-btn',
  'question-bg',
  'question-btn',
  'success-bg',
  'success-btn',
])

function applySafelist(usedTokens) {
  for (const token of SAFELIST_TOKENS) usedTokens.add(token)
  return usedTokens
}

function toBoolEnv(v, def = false) {
  if (v === undefined || v === null || v === '') return def
  const s = String(v).trim().toLowerCase()
  return s === '1' || s === 'true' || s === 'yes' || s === 'on' || s === 'y'
}

async function* walk(dir) {
  const dirents = await fs.readdir(dir, { withFileTypes: true })
  for (const d of dirents) {
    const res = path.resolve(dir, d.name)
    if (d.isDirectory()) {
      yield* walk(res)
    } else if (d.isFile()) {
      yield res
    }
  }
}

function collectUsedCssTokensFromBundle(bundle) {
  const usedTokens = new Set()

  function addTokens(text) {
    if (!text) return
    const matches = String(text).match(/[A-Za-z_][A-Za-z0-9_-]*/g)
    if (!matches) return
    for (const token of matches) {
      usedTokens.add(token)
    }
  }

  for (const output of Object.values(bundle)) {
    if (output.type === 'asset' && output.fileName.endsWith('.html') && typeof output.source === 'string') {
      addTokens(output.source)
      continue
    }

    if (output.type === 'chunk' && output.fileName.endsWith('.js')) {
      addTokens(output.code)
    }
  }

  return applySafelist(usedTokens)
}

// CSS selector lists can contain commas inside attribute selectors and
// pseudo-class arguments, so split with a small state machine instead of a raw
// .split(',').
function splitSelectorList(selectorText) {
  const selectors = []
  let current = ''
  let bracketDepth = 0
  let parenDepth = 0
  let quote = null

  for (let i = 0; i < selectorText.length; i += 1) {
    const ch = selectorText[i]
    const prev = i > 0 ? selectorText[i - 1] : ''

    if (quote) {
      current += ch
      if (ch === quote && prev !== '\\') {
        quote = null
      }
      continue
    }

    if (ch === '"' || ch === '\'') {
      quote = ch
      current += ch
      continue
    }

    if (ch === '[') bracketDepth += 1
    if (ch === ']') bracketDepth = Math.max(0, bracketDepth - 1)
    if (ch === '(') parenDepth += 1
    if (ch === ')') parenDepth = Math.max(0, parenDepth - 1)

    if (ch === ',' && bracketDepth === 0 && parenDepth === 0) {
      if (current.trim()) selectors.push(current.trim())
      current = ''
      continue
    }

    current += ch
  }

  if (current.trim()) selectors.push(current.trim())
  return selectors
}

// Extract the selector tokens that must exist in the document for the selector
// to match anything at all. We only track element/class/id-like identifiers.
function extractRequiredTokensFromSelector(selector) {
  const requiredTokens = []
  const seen = new Set()
  const strippedClassesAndIds = selector.replace(/[.#](-?[_a-zA-Z]+[_a-zA-Z0-9-]*)/g, (_match, token) => {
    if (!seen.has(token)) {
      seen.add(token)
      requiredTokens.push(token)
    }
    return ' '
  })

  for (const segment of strippedClassesAndIds.split(/[\s>+~]+/)) {
    const trimmed = segment.trim()
    if (!trimmed || trimmed === '*') continue
    const match = trimmed.match(/^(-?[_a-zA-Z]+[_a-zA-Z0-9-]*)/)
    if (!match) continue
    const token = match[1]
    if (!seen.has(token)) {
      seen.add(token)
      requiredTokens.push(token)
    }
  }

  return requiredTokens
}

// Treat removal as a "definitely unused" test, not a best-effort guess. If the
// selector shape gets too dynamic, keep it.
function selectorIsDefinitelyUnused(selector, usedTokens) {
  if (!selector) return false
  if (selector.includes('&')) return false
  if (/:[-_a-zA-Z0-9]+\(/.test(selector)) return false

  const normalized = selector
    .replace(/\/\*[\s\S]*?\*\//g, '')
    .replace(/"(?:\\.|[^"\\])*"|'(?:\\.|[^'\\])*'/g, '')
    .replace(/\[[^\]]*\]/g, '')
    .replace(/::?[-_a-zA-Z0-9]+/g, '')

  const requiredTokens = extractRequiredTokensFromSelector(normalized)
  if (requiredTokens.length === 0) {
    return false
  }

  return requiredTokens.some((token) => !usedTokens.has(token))
}

// Walk nested at-rules recursively so target-specific dead selectors are
// removed even when they live inside media/supports blocks.
function pruneCssContainer(container, usedTokens, stats) {
  for (const node of [...(container.nodes || [])]) {
    if (node.type === 'rule') {
      const selectors = splitSelectorList(node.selector)
      const keptSelectors = selectors.filter((selector) => !selectorIsDefinitelyUnused(selector, usedTokens))

      if (keptSelectors.length === 0) {
        node.remove()
        stats.removedRules += 1
        continue
      }

      if (keptSelectors.length !== selectors.length) {
        node.selector = keptSelectors.join(', ')
        stats.trimmedSelectors += selectors.length - keptSelectors.length
      }
      continue
    }

    if (node.nodes?.length) {
      pruneCssContainer(node, usedTokens, stats)
      if (node.nodes.length === 0) {
        node.remove()
      }
    }
  }
}

function pruneUnusedCss(css, usedTokens) {
  const root = postcss.parse(css)
  const stats = { removedRules: 0, trimmedSelectors: 0 }
  pruneCssContainer(root, usedTokens, stats)
  return { css: root.toString(), stats }
}

// Dev mode has no final bundle yet, so approximate usage from the source tree
// after applying the same feature-block filtering used elsewhere in the build.
async function collectUsedCssTokensFromSource(root, processHtml, processJs) {
  const usedTokens = new Set()

  function addTokens(text) {
    if (!text) return
    const matches = String(text).match(/[A-Za-z_][A-Za-z0-9_-]*/g)
    if (!matches) return
    for (const token of matches) {
      usedTokens.add(token)
    }
  }

  addTokens(processHtml(await fs.readFile(path.join(root, 'index.html'), 'utf8')))

  const srcRoot = path.join(root, 'src')
  for await (const filePath of walk(srcRoot)) {
    const normalized = filePath.split(path.sep).join('/')
    if (normalized.endsWith('.js') || normalized.endsWith('.mjs')) {
      addTokens(processJs(await fs.readFile(filePath, 'utf8')))
    } else if (normalized.endsWith('.html')) {
      addTokens(processHtml(await fs.readFile(filePath, 'utf8')))
    }
  }

  return applySafelist(usedTokens)
}

// Build-time pruning is the authoritative size optimization. Dev-time pruning
// is opt-in because it is source-based and therefore necessarily less precise.
export function cssTreeShakePlugin(env = {}) {
  const devEnabled = toBoolEnv(env.VITE_DEV_CSS_TREE_SHAKE, false)
  const { processHtml, processJs } = createFeatureBlockProcessor(env)

  return {
    name: 'css-tree-shake',
    apply: (_config, { command }) => command === 'build' || (command === 'serve' && devEnabled),
    async transform(code, id) {
      if (!devEnabled || !/\.css($|\?)/i.test(id) || id.includes('/node_modules/')) {
        return null
      }

      const root = this.environment.config.root || process.cwd()
      const usedTokens = await collectUsedCssTokensFromSource(root, processHtml, processJs)
      const { css, stats } = pruneUnusedCss(code, usedTokens)

      // Warn in dev so it is obvious when the served CSS differs from the
      // source file on disk.
      if (stats.removedRules > 0 || stats.trimmedSelectors > 0) {
        this.warn(
          `CSS tree-shake(dev) pruned ${stats.removedRules} rules and ` +
          `${stats.trimmedSelectors} selectors from ${id.split('?')[0]}`
        )
      }

      return css === code ? null : { code: css, map: null }
    },
    generateBundle(_options, bundle) {
      const usedTokens = collectUsedCssTokensFromBundle(bundle)
      let totalBefore = 0
      let totalAfter = 0
      let totalRemovedRules = 0
      let totalTrimmedSelectors = 0

      for (const output of Object.values(bundle)) {
        if (output.type !== 'asset' || !output.fileName.endsWith('.css') || typeof output.source !== 'string') {
          continue
        }

        const before = Buffer.byteLength(output.source, 'utf8')
        const { css, stats } = pruneUnusedCss(output.source, usedTokens)
        output.source = css
        totalBefore += before
        totalAfter += Buffer.byteLength(css, 'utf8')
        totalRemovedRules += stats.removedRules
        totalTrimmedSelectors += stats.trimmedSelectors
      }

      if (totalBefore !== totalAfter) {
        this.info(
          `CSS tree-shake removed ${formatBytes(totalBefore - totalAfter)} ` +
          `across ${totalRemovedRules} rules and ${totalTrimmedSelectors} selectors.`
        )
      }
    },
  }
}
