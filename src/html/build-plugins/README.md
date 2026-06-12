# Build Plugins

These plugins are part of the production build pipeline used by `vite build`.
They are responsible for turning the Web UI source tree into the compact target
artifacts that get embedded into firmware headers.

Use a build plugin when the job depends on:
- the selected target feature flags
- the emitted bundle or `dist/` contents
- production-only asset shaping or size reduction

Do not put dev-server middleware or mock hardware behavior here. Those belong
in [`../dev-plugins`](../dev-plugins/README.md).

## Plugins

### `feature-blocks-plugin.js`

Purpose:
- strips `FEATURE:` blocks from HTML, JS, and CSS based on `VITE_FEATURE_*`
  flags

Use when:
- code or assets should exist only for some targets
- a plugin or script needs the same feature filtering rules as the main build

Do not use when:
- the decision depends on runtime state
- the content should remain in the bundle and only be hidden in the UI

### `css-tree-shake-plugin.js`

Purpose:
- removes selectors that cannot match the current target's HTML/JS token set

Use when:
- CSS size should shrink per target
- you want build-time pruning against the final bundle
- you want opt-in dev-time approximation with `VITE_DEV_CSS_TREE_SHAKE=true`

Do not use when:
- selector reachability depends on runtime-generated class names that are not
  visible in source or bundle text
- exact CSS semantics are more important than size reduction

### `babel-decorators-plugin.js`

Purpose:
- transpiles decorators in `src/**/*.js` with Babel before Vite continues

Use when:
- source files in `src/` rely on the current decorators transform

Do not use when:
- processing files outside the application source tree
- the input already went through the same Babel decorators transform

### `inline-static-html-assets-plugin.js`

Purpose:
- inlines the built app CSS, app JS, and favicon back into `dist/index.html`

Use when:
- the final artifact should be a small set of embedded web assets
- reducing HTTP asset count matters for the firmware web server

Do not use when:
- you want the built CSS/JS files to stay separate in `dist/assets`
- downstream tooling expects external asset files to remain on disk

### `esp32-header-plugin.js`

Purpose:
- compresses `dist/` assets and emits a C header for firmware embedding

Use when:
- the build output needs to become a firmware-consumable header

Do not use when:
- you only need a browser-facing web build
- another packaging step already owns asset compression and embedding
