import { defineConfig, loadEnv } from 'vite'
import path from 'path'
import { minifyTemplateLiterals } from 'rollup-plugin-minify-template-literals';
import { babelDecoratorsPlugin } from './build-plugins/babel-decorators-plugin.js'
import { cssTreeShakePlugin } from './build-plugins/css-tree-shake-plugin.js'
import { htmlFeatureBlocksPlugin } from './build-plugins/feature-blocks-plugin.js'
import { inlineStaticHtmlAssetsPlugin } from './build-plugins/inline-static-html-assets-plugin.js'
import { viteEsp32HeaderPlugin } from './build-plugins/esp32-header-plugin.js'

// Simple dev mock server plugin
import { devMockPlugin } from './dev-plugins/dev-mock-plugin.js'

// Proxy plugin for devlopment against real hardware
import { devProxyPlugin } from './dev-plugins/dev-proxy-plugin.js'

// Export standard Vite config with the plugin enabled for builds
export default defineConfig(({ command, mode }) => {
  const env = loadEnv(mode, process.cwd(), '')
  return {
    plugins: [
      htmlFeatureBlocksPlugin(env),
      minifyTemplateLiterals({
        include: ['src/**/*.js'],
        exclude: ['node_modules/**'],
        failOnError: true,
        options: {
          minifyOptions: {
            collapseWhitespace: true,
            removeComments: true,
            removeAttributeQuotes: true,
            collapseBooleanAttributes: true,
            removeRedundantAttributes: true,
            useShortDoctype: true,
            caseSensitive: true,
            minifyCSS: true
          }
        }
      }),
      cssTreeShakePlugin(env),
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
