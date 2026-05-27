import { promises as fs } from 'fs'
import path from 'path'

// Build plugin: inline the emitted app CSS, app JS, and favicon back into the
// built HTML entrypoint.
//
// Use this when the final firmware-facing artifact should minimize external
// asset files. Do not use it if downstream tooling expects standalone files in
// `dist/assets`.

function inlineForHtml(code, tagName) {
  return code.replace(new RegExp(`</${tagName}`, 'gi'), `<\\/${tagName}`)
}

function toDataUrl(contentType, data) {
  return `data:${contentType};base64,${data.toString('base64')}`
}

export function inlineStaticHtmlAssetsPlugin() {
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
