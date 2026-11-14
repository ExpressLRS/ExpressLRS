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

export function deriveFeatures(env = ENV) {
  // A multi-radio build has more than one HAS_<chip>; the chip it drives is
  // settings['radio-type'], so anything chip specific is decided at runtime.
  const multiRadio = toBool(env.VITE_FEATURE_MULTI_RADIO, false)
  return {
    IS_TX: toBool(env.VITE_FEATURE_IS_TX, false),
    IS_8285: toBool(env.VITE_FEATURE_IS_8285, false),
    HAS_SX128X: toBool(env.VITE_FEATURE_HAS_SX128X, false),
    HAS_SX127X: toBool(env.VITE_FEATURE_HAS_SX127X, false),
    HAS_LR1121: toBool(env.VITE_FEATURE_HAS_LR1121, false),
    HAS_LR2021: toBool(env.VITE_FEATURE_HAS_LR2021, false),
    HAS_SUBGHZ: toBool(env.VITE_FEATURE_HAS_LR1121, false) || toBool(env.VITE_FEATURE_HAS_LR2021, false) || toBool(env.VITE_FEATURE_HAS_SX127X, false),
    HAS_DUAL_BAND: toBool(env.VITE_FEATURE_HAS_LR1121, false) || toBool(env.VITE_FEATURE_HAS_LR2021, false),
    MULTI_RADIO: multiRadio,
    SX127X_ONLY: toBool(env.VITE_FEATURE_HAS_SX127X, false) && !multiRadio,
    SX128X_ONLY: toBool(env.VITE_FEATURE_HAS_SX128X, false) && !multiRadio,
  }
}

export const FEATURES = deriveFeatures(ENV)

export default FEATURES
