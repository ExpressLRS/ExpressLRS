import http from 'http'
import https from 'https'
import { URL } from 'url'

export function devProxyPlugin({ target }) {
  const targetUrl = new URL(target)
  const client = targetUrl.protocol === 'https:' ? https : http
  console.log("Proxying request to:", targetUrl.href)

  return {
    name: 'vite-dev-elrs-proxy',
    apply: 'serve',

    configureServer(server) {
      server.middlewares.use((req, res, next) => {
        const url = req.url || '/'

        // Let Vite handle its own internal requests
        if (
          url.startsWith('/@') ||
          url.startsWith('/src') ||
          url.startsWith('/node_modules') ||
          url.startsWith('/assets') ||
          url.endsWith('.js') ||
          url.endsWith('.html') ||
          url.endsWith('.css') ||
          url.endsWith('.map')
        ) {
          console.log("Forwarding request to Vite:", url)
          return next()
        }

        // Build target request
        console.log("Forwarding request to target:", url)
        const proxyReq = client.request(
          {
            hostname: targetUrl.hostname,
            port: targetUrl.port || (targetUrl.protocol === 'https:' ? 443 : 80),
            method: req.method,
            path: url,
            headers: {
              ...req.headers,
              host: targetUrl.host
            }
          },
          (proxyRes) => {
            res.writeHead(proxyRes.statusCode || 500, proxyRes.headers)
            proxyRes.pipe(res)
          }
        )

        proxyReq.on('error', (err) => {
          res.statusCode = 502
          res.end(`Proxy error: ${err.message}`)
        })

        // Stream body (important for firmware uploads)
        req.pipe(proxyReq)
      })
    }
  }
}
