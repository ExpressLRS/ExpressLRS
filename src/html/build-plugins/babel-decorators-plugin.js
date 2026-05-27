import path from 'path'

import { transformAsync } from '@babel/core'

// Build plugin: transpile decorator syntax in application source files before
// Vite continues its normal pipeline.
//
// Use this for `src/**/*.js` modules that rely on the current decorators
// transform. Do not use it as a general-purpose Babel pass for arbitrary files.
export function babelDecoratorsPlugin() {
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
