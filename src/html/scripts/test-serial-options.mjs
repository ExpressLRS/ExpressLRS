import assert from 'node:assert/strict'
import {primarySerialOptions, SERIAL_OPTIONS1, SERIAL_OPTIONS2} from '../src/utils/globals.js'

const options = primarySerialOptions(true)
assert.ok(options.some(option => option.value === 11), 'Smart option missing')
assert.equal(options.find(option => option.value === 11).label, 'Spektrum Smart')
const legacy = ['CRSF', 'Inverted CRSF', 'SBUS', 'Inverted SBUS', 'SUMD', 'DJI RS Pro', 'HoTT Telemetry', 'MAVLink', 'DisplayPort', 'GPS', 'Scorpion Telemetry']
for (const [value, label] of legacy.entries()) {
    assert.equal(options.find(option => option.value === value).label, label)
}
const unsupported = primarySerialOptions(false)
assert.equal(unsupported.some(option => option.value === 11), false)
assert.equal(unsupported.find(option => option.value === 10).label, 'Scorpion Telemetry')
assert.equal(unsupported.find(option => option.label === 'AirPort').value, 12)
assert.equal(SERIAL_OPTIONS1.length - 1, 12)
assert.equal(SERIAL_OPTIONS2[12], 'Scorpion Telemetry')
console.log('Serial option compatibility/capability checks passed')
