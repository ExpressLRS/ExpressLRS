import { promises as fs } from 'fs'
import path from 'path'

import Zopfli from 'node-zopfli-es'

// Build plugin: compress `dist/` assets and emit a firmware-consumable C
// header.
//
// Use this only for build targets that need embedded web assets in flash. Do
// not use it for plain browser-only builds where `dist/` itself is the final
// artifact.

function toCIdentifier(p) {
  let id = p.replace(/[^a-zA-Z0-9]/g, '_')
  if (/^[0-9]/.test(id)) id = '_' + id
  return id
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

function toHexArray(buf) {
  const parts = []
  for (let i = 0; i < buf.length; i++) {
    const hex = '0x' + buf[i].toString(16).padStart(2, '0')
    parts.push(hex)
  }
  const lines = []
  for (let i = 0; i < parts.length; i += 16) {
    lines.push('  ' + parts.slice(i, i + 16).join(', '))
  }
  return lines.join(',\n')
}

function formatBytes(bytes) {
  return `${bytes} bytes (${(bytes / 1024).toFixed(2)} KiB)`
}

function headerPreamble() {
  return `#pragma once\n#include <stddef.h>\n#include <stdint.h>\n#include <pgmspace.h>\n\n#ifdef __cplusplus\nextern \"C\" {\n#endif\n\ntypedef struct {\n  const char* path;\n  const char* content_type;\n  const unsigned char* data;\n  const size_t size;\n} WebAsset;\n`
}

function headerEpilogue() {
  return `\n#ifdef __cplusplus\n} // extern \"C\"\n#endif\n`
}

function guessContentType(filePath) {
  const ext = path.extname(filePath).toLowerCase()
  switch (ext) {
    case '.html': return 'text/html';
    case '.css': return 'text/css';
    case '.js': return 'application/javascript';
    case '.mjs': return 'application/javascript';
    case '.json': return 'application/json';
    case '.svg': return 'image/svg+xml';
    case '.png': return 'image/png';
    case '.jpg':
    case '.jpeg': return 'image/jpeg';
    case '.gif': return 'image/gif';
    case '.ico': return 'image/x-icon';
    case '.woff': return 'font/woff';
    case '.woff2': return 'font/woff2';
    case '.ttf': return 'font/ttf';
    case '.eot': return 'application/vnd.ms-fontobject';
    case '.wasm': return 'application/wasm';
    case '.map': return 'application/json';
    default: return 'application/octet-stream';
  }
}

export function viteEsp32HeaderPlugin(options = {}) {
  const headerName = options.headerName || 'esp32_fs.h'
  const headerOut = options.headerOut || null
  const includeMaps = options.includeMaps ?? false

  let outDir = 'dist'
  let root = process.cwd()

  return {
    name: 'vite-esp32-header',
    apply: 'build',
    configResolved(config) {
      outDir = config.build?.outDir || 'dist'
      root = config.root || process.cwd()
    },
    async closeBundle() {
      const distDir = path.resolve(root, outDir)

      try {
        const st = await fs.stat(distDir)
        if (!st.isDirectory()) return
      } catch (_e) {
        return
      }

      const files = []
      for await (const filePath of walk(distDir)) {
        if (!includeMaps && filePath.endsWith('.map')) continue
        files.push(filePath)
      }

      if (files.length === 0) return
      files.sort()

      const assetEntries = []
      let totalCompressedBytes = 0

      for (const abs of files) {
        const rel = path.relative(distDir, abs).split(path.sep).join('/')
        const webPath = '/' + rel
        const id = toCIdentifier(rel)
        const data = await fs.readFile(abs)
        const compressed = Zopfli.gzipSync(data, { numiterations: 15 })
        totalCompressedBytes += compressed.length
        const contentType = guessContentType(rel)
        const hex = toHexArray(compressed)
        assetEntries.push({ webPath, id, contentType, hex, originalBytes: data.length, compressedBytes: compressed.length })
      }

      let header = headerPreamble()
      const outFile = headerOut
        ? path.resolve(root, headerOut)
        : path.join(distDir, headerName)
      const artifactName = path.relative(root, outFile)

      header += `\n// Artifact: ${artifactName}\n`
      header += `// Total web asset payload: ${formatBytes(totalCompressedBytes)} across ${assetEntries.length} assets\n`

      for (const entry of assetEntries) {
        header += `\n// ${entry.webPath} (original ${entry.originalBytes} bytes) -> compressed ${entry.compressedBytes} bytes\n`
        header += `static const unsigned char ${entry.id}[] PROGMEM = {\n${entry.hex}\n};\n`
        header += `static const size_t ${entry.id}_len = ${entry.compressedBytes};\n`
      }

      header += '\n// File index for convenient iteration\n'
      header += 'static const WebAsset WEB_ASSETS[] PROGMEM = {\n'
      for (const entry of assetEntries) {
        header += `  { \"${entry.webPath}\", \"${entry.contentType}\", ${entry.id}, ${entry.id}_len },\n`
      }
      header += '};\n'
      header += 'static const size_t WEB_ASSETS_COUNT = sizeof(WEB_ASSETS) / sizeof(WEB_ASSETS[0]);\n\n'
      header += headerEpilogue()

      await fs.mkdir(path.dirname(outFile), { recursive: true })
      await fs.writeFile(outFile, header, 'utf8')
      this.info(
        `Wrote ${artifactName} with ${assetEntries.length} assets. ` +
        `Total byte-array payload: ${formatBytes(totalCompressedBytes)}.`
      )
    },
  }
}
