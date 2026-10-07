import assert from 'node:assert/strict'
import {SERIAL_OPTIONS1, SERIAL_OPTIONS2} from '../src/utils/globals.js'
import {_renderOptions} from '../src/utils/libs.js'

const options = _renderOptions(SERIAL_OPTIONS1, 11)
assert.deepEqual(options[11].values, ['11', true, 'Spektrum Smart'])
const legacy = ['CRSF', 'Inverted CRSF', 'SBUS', 'Inverted SBUS', 'SUMD', 'DJI RS Pro', 'HoTT Telemetry', 'MAVLink', 'DisplayPort', 'GPS', 'Scorpion Telemetry']
for (const [value, label] of legacy.entries()) {
    assert.equal(options[value].values[2], label)
}
assert.deepEqual(options[10].values, ['10', false, 'Scorpion Telemetry'])
assert.deepEqual(options[12].values, ['12', false, 'AirPort'])
assert.equal(SERIAL_OPTIONS1.length - 1, 12)
assert.equal(SERIAL_OPTIONS2[12], 'Scorpion Telemetry')
console.log('Serial option rendering/protocol ID checks passed')
