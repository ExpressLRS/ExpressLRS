import { defineConfig, loadEnv } from 'vite'
import babel from 'vite-plugin-babel'
import { promises as fs } from 'fs'
import path from 'path'
import Zopfli from 'node-zopfli-es'

function toCIdentifier(p) {
  // Make a valid C identifier from a file path
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
  // Pretty-print into lines of 16 bytes
  const lines = []
  for (let i = 0; i < parts.length; i += 16) {
    lines.push('  ' + parts.slice(i, i + 16).join(', '))
  }
  return lines.join(',\n')
}

function headerPreamble(generatedBy, dateStr) {
  return `#pragma once\n#include <stddef.h>\n#include <stdint.h>\n#include <pgmspace.h>\n\n#ifdef __cplusplus\nextern \"C\" {\n#endif\n\n// Represents one web asset (pre-compressed) stored in PROGMEM\ntypedef struct {\n  const char* path;\n  const char* content_type;\n  const unsigned char* data;\n  const size_t size;\n} WebAsset;\n`
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

// HTML feature blocks plugin extracted to separate module
import { htmlFeatureBlocksPlugin } from './feature-blocks-plugin.js'

function viteEsp32HeaderPlugin(options = {}) {
  const headerName = options.headerName || 'esp32_fs.h'
  const includeMaps = options.includeMaps ?? false // whether to include source maps

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

      // Ensure dist exists
      try {
        const st = await fs.stat(distDir)
        if (!st.isDirectory()) return
      } catch (e) {
        // No dist yet
        return
      }

      const files = []
      for await (const filePath of walk(distDir)) {
        // Optionally skip .map files
        if (!includeMaps && filePath.endsWith('.map')) continue
        files.push(filePath)
      }

      if (files.length === 0) return

      const genBy = 'vite-esp32-header plugin'
      const when = new Date().toISOString()
      let header = headerPreamble(genBy, when)

      const assetEntries = []

      for (const abs of files) {
        const rel = path.relative(distDir, abs).split(path.sep).join('/')
        const webPath = '/' + rel // leading slash for HTTP paths
        const id = toCIdentifier(rel)
        const data = await fs.readFile(abs)
        const compressed = Zopfli.gzipSync(data, { numiterations: 15 })
        const hex = toHexArray(compressed)
        const contentType = guessContentType(rel)
        header += `\n// ${webPath} (original ${data.length} bytes) -> compressed ${compressed.length} bytes\n`;
        header += `static const unsigned char ${id}[] PROGMEM = {\n${hex}\n};\n`
        header += `static const size_t ${id}_len = ${compressed.length};\n`
        assetEntries.push({ webPath, id, contentType})
      }

      header += '\n// File index for convenient iteration\n'
      header += 'static const WebAsset WEB_ASSETS[] PROGMEM = {\n'
      for (const entry of assetEntries) {
        // The path strings will live in flash as well (implicitly)
        header += `  { \"${entry.webPath}\", \"${entry.contentType}\", ${entry.id}, ${entry.id}_len },\n`
      }
      header += '};\n'
      header += 'static const size_t WEB_ASSETS_COUNT = sizeof(WEB_ASSETS) / sizeof(WEB_ASSETS[0]);\n\n'
      header += headerEpilogue()

      const outFile = path.join(distDir, headerName)
      await fs.writeFile(outFile, header, 'utf8')
      // eslint-disable-next-line no-console
      console.log(`\n[${genBy}] Wrote ${path.relative(root, outFile)} with ${assetEntries.length} assets.`)
    },
  }
}

// Simple dev mock server plugin
import { devMockPlugin } from './dev-mock-plugin.js'

// Export standard Vite config with the plugin enabled for builds
export default defineConfig(({ command, mode }) => {
  const env = loadEnv(mode, process.cwd(), '')
  return {
    plugins: [
      htmlFeatureBlocksPlugin(env),
      viteEsp32HeaderPlugin(),
      babel({
        babelConfig: {
          babelrc: false,
          configFile: false,
          plugins: [
            // Configure the decorators plugin with the desired version
            ['@babel/plugin-proposal-decorators', { version: '2023-05' }],
          ],
        },
      }),
      ...(command === 'serve' ? [devMockPlugin()] : []),
    ],
    build: {
      rollupOptions: {
        input: {
          app: path.resolve(__dirname, 'app.html'),
        },
        output: {
          entryFileNames: 'assets/[name]-[hash].js',
          chunkFileNames: 'assets/[name]-[hash].js',
          manualChunks(id) {
            const p = id.split('\\').join('/');
            const is = (name) => p.includes(`/src/pages/${name}.js`);
            // Group General route modules
            if (
              is('binding-panel') ||
              is('wifi-panel') ||
              is('update-panel') ||
              is('tx-options-panel') ||
              is('models-panel') ||
              is('buttons-panel') ||
              is('rx-options-panel') ||
              is('connections-panel') ||
              is('serial-panel')
            ) {
              return 'general';
            }
            // Group Advanced route modules
            if (
              is('hardware-layout') ||
              is('continuous-wave') ||
              is('lr1121-updater')
            ) {
              return 'advanced';
            }
            // Force everything else (including node_modules) into the main chunk
            return 'main';
          },
        },
      },
      // Keep CSS separate; request was specifically about JS files
      cssCodeSplit: true,
      sourcemap: false,
    }
  }
})
