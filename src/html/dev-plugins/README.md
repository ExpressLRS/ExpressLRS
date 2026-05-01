# Dev Plugins

These plugins are only for `vite serve`. They exist to make local development
and device testing practical without changing the production bundle.

Use a dev plugin when the job depends on:
- local middleware
- mock endpoints
- proxying requests to real hardware during development

Do not put production build transforms here. Those belong in
[`../build-plugins`](../build-plugins/README.md).

## Plugins

### `dev-mock-plugin.js`

Purpose:
- serves mock ELRS config, hardware, and voltage-sampling endpoints during
  local development

Use when:
- you want to exercise the UI without real hardware
- you need stable mock responses for layout and flow work

Do not use when:
- validating device-specific behavior against a real target
- testing networking, timing, or firmware integration details that depend on
  the real hardware stack

### `dev-proxy-plugin.js`

Purpose:
- proxies non-Vite requests from the local dev server to a real device

Use when:
- you want the local UI code but real device responses
- testing against hardware is more important than deterministic mocks

Do not use when:
- you need offline or repeatable local-only testing
- you want to inspect or change mock API behavior inside the repo
