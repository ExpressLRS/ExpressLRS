import { defineConfig, loadEnv } from 'vite'
import { transformAsync } from '@babel/core'
import { promises as fs } from 'fs'
import path from 'path'
import Zopfli from 'node-zopfli-es'
import { minifyTemplateLiterals } from 'rollup-plugin-minify-template-literals';

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

function formatBytes(bytes) {
  return `${bytes} bytes (${(bytes / 1024).toFixed(2)} KiB)`
}

function inlineForHtml(code, tagName) {
  return code.replace(new RegExp(`</${tagName}`, 'gi'), `<\\/${tagName}`)
}

function toDataUrl(contentType, data) {
  return `data:${contentType};base64,${data.toString('base64')}`
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

// HTML feature blocks plugin extracted to separate module
import { htmlFeatureBlocksPlugin } from './feature-blocks-plugin.js'

function babelDecoratorsPlugin() {
  let root = process.cwd()

  return {
    name: 'babel-decorators',
    configResolved(config) {
      root = config.root || process.cwd()
    },
    async transform(code, id) {
      const file = id.split('?')[0]
      const normalized = file.split(path.sep).join('/')
      const srcRoot = path.resolve(root, 'src').split(path.sep).join('/') + '/'

      if (!normalized.startsWith(srcRoot) || !normalized.endsWith('.js')) {
        return null
      }

      const result = await transformAsync(code, {
        cwd: root,
        root,
        filename: file,
        sourceMaps: true,
      })

      return result ? { code: result.code ?? code, map: result.map ?? null } : null
    },
  }
}

function inlineStaticHtmlAssetsPlugin() {
  let outDir = 'dist'
  let root = process.cwd()

  return {
    name: 'inline-static-html-assets',
    apply: 'build',
    configResolved(config) {
      outDir = config.build?.outDir || 'dist'
      root = config.root || process.cwd()
    },
    async writeBundle() {
      const distDir = path.resolve(root, outDir)
      const assetsDir = path.join(distDir, 'assets')
      const indexPath = path.join(distDir, 'index.html')

      let assetNames
      try {
        assetNames = await fs.readdir(assetsDir)
      } catch {
        return
      }

      const appCssName = assetNames.find((name) => /^app-[^.]+\.css$/.test(name))
      const appJsName = assetNames.find((name) => /^app-[^.]+\.js$/.test(name))
      const faviconName = assetNames.find((name) => /^favicon-[^.]+\.svg$/.test(name))

      if (!appCssName && !appJsName && !faviconName) {
        return
      }

      let indexHtml = await fs.readFile(indexPath, 'utf8')

      if (appCssName) {
        const appCssPath = path.join(assetsDir, appCssName)
        const appCss = await fs.readFile(appCssPath, 'utf8')

        indexHtml = indexHtml.replace(
          /<link rel="stylesheet" crossorigin href="\/assets\/app-[^"]+\.css">/,
          `<style>${inlineForHtml(appCss, 'style')}</style>`
        )

        await fs.unlink(appCssPath)
      }

      if (appJsName) {
        const appJsPath = path.join(assetsDir, appJsName)
        const appJs = (await fs.readFile(appJsPath, 'utf8')).replace(
          /\.\/([^"'`]+\.js)/g,
          '/assets/$1'
        )

        indexHtml = indexHtml.replace(
          /<script type="module" crossorigin src="\/assets\/app-[^"]+\.js"><\/script>/,
          `<script type="module">${inlineForHtml(appJs, 'script')}</script>`
        )

        await fs.unlink(appJsPath)
      }

      if (faviconName) {
        const faviconPath = path.join(assetsDir, faviconName)
        const faviconDataUrl = toDataUrl('image/svg+xml', await fs.readFile(faviconPath))

        indexHtml = indexHtml.replace(
          /<link[^>]+href="\/assets\/favicon-[^"]+\.svg"[^>]*>/,
          `<link href="${faviconDataUrl}" rel="icon" type="image/svg+xml" />`
        )

        await fs.unlink(faviconPath)
      }

      await fs.writeFile(indexPath, indexHtml, 'utf8')
      this.info('Inlined static HTML assets into index.html.')
    },
  }
}

function viteEsp32HeaderPlugin(options = {}) {
  const headerName = options.headerName || 'esp32_fs.h'
  const headerOut = options.headerOut || null
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
      files.sort()

      const genBy = 'vite-esp32-header plugin'
      const when = new Date().toISOString()
      const assetEntries = []
      let totalCompressedBytes = 0

      for (const abs of files) {
        const rel = path.relative(distDir, abs).split(path.sep).join('/')
        const webPath = '/' + rel // leading slash for HTTP paths
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
        // The path strings will live in flash as well (implicitly)
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

// Simple dev mock server plugin
import { devMockPlugin } from './dev-mock-plugin.js'

// Proxy plugin for devlopment against real hardware
import { devProxyPlugin } from './dev-proxy-plugin.js'

// Export standard Vite config with the plugin enabled for builds
export default defineConfig(({ command, mode }) => {
  const env = loadEnv(mode, process.cwd(), '')
  return {
    plugins: [
      htmlFeatureBlocksPlugin(env),
      minifyTemplateLiterals({
        include: ['src/**/*.js'],
        exclude: ['node_modules/**'],
      }),
      inlineStaticHtmlAssetsPlugin(),
      viteEsp32HeaderPlugin({ headerOut: env.ELRS_WEB_HEADER_OUT }),
      babelDecoratorsPlugin(),
      ...(command === 'serve'
        ? [
            env.VITE_ELRS_PROXY_TARGET
              ? devProxyPlugin({ target: env.VITE_ELRS_PROXY_TARGET })
              : devMockPlugin()
          ]
        : []),
    ],
    optimizeDeps: {
      rolldownOptions: {
        plugins: [htmlFeatureBlocksPlugin(env)],
      },
    },
    esbuild: {
      legalComments: 'none'
    },
    build: {
      rolldownOptions: {
        input: {
          app: path.resolve(__dirname, 'index.html'),
        },
        output: {
          entryFileNames: 'assets/[name]-[hash].js',
          chunkFileNames: 'assets/[name]-[hash].js',
          manualChunks(id) {
            const p = id.split('\\').join('/')
            if (
              (p.includes('/src/utils/') && !p.endsWith('/hardware-schema.js')) ||
              p.endsWith('/src/components/filedrag.js')
            ) {
              return 'utils'
            }
            return undefined
          },
        },
      },
      cssCodeSplit: true,
      sourcemap: false,
    }
  }
})
