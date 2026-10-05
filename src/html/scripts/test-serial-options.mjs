import assert from 'node:assert/strict'
import {primarySerialOptions, SERIAL_OPTIONS1} from '../src/utils/globals.js'

const options = primarySerialOptions(true)
assert.ok(options.some(option => option.value === 10), 'Smart option missing')
assert.equal(options.find(option => option.value === 10).label, 'Spektrum Smart')
const legacy = ['CRSF', 'Inverted CRSF', 'SBUS', 'Inverted SBUS', 'SUMD', 'DJI RS Pro', 'HoTT Telemetry', 'MAVLink', 'DisplayPort', 'GPS']
for (const [value, label] of legacy.entries()) {
    assert.equal(options.find(option => option.value === value).label, label)
}
const unsupported = primarySerialOptions(false)
assert.equal(unsupported.some(option => option.value === 10), false)
assert.equal(unsupported.find(option => option.label === 'AirPort').value, 11)
assert.equal(SERIAL_OPTIONS1.length - 1, 11)
console.log('Serial option compatibility/capability checks passed')
