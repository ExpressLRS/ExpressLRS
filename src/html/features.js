// Central place to read build-time feature flags from Vite env.
// This lives outside `src` because it is only used by build/dev tooling now.

const toBool = (v, defaultValue) => {
  if (v === undefined || v === null || v === '') return defaultValue
  if (typeof v === 'boolean') return v
  const s = String(v).toLowerCase().trim()
  return s === '1' || s === 'true' || s === 'yes' || s === 'on' || s === 'y'
}

const ENV = (typeof import.meta !== 'undefined' && import.meta && import.meta.env)
  ? import.meta.env
  : (typeof process !== 'undefined' && process.env ? process.env : {})

export const FEATURES = {
  IS_TX: toBool(ENV.VITE_FEATURE_IS_TX, false),
  IS_8285: toBool(ENV.VITE_FEATURE_IS_8285, false),
  HAS_SX128X: toBool(ENV.VITE_FEATURE_HAS_SX128X, false),
  HAS_SX127X: toBool(ENV.VITE_FEATURE_HAS_SX127X, false),
  HAS_LR1121: toBool(ENV.VITE_FEATURE_HAS_LR1121, false),
  HAS_SUBGHZ: toBool(ENV.VITE_FEATURE_HAS_LR1121, false) || toBool(ENV.VITE_FEATURE_HAS_SX127X, false),
}

export default FEATURES
