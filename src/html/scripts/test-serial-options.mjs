import assert from 'node:assert/strict'
import {SERIAL_OPTIONS1, SERIAL_OPTIONS2} from '../src/utils/globals.js'
import {_renderOptions} from '../src/utils/libs.js'
import {readFileSync} from 'node:fs'
import {parseSync} from '@babel/core'

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
assert.equal(SERIAL_OPTIONS2[13], 'Spektrum Smart')
assert.deepEqual(_renderOptions(SERIAL_OPTIONS2, 13)[13].values, ['13', true, 'Spektrum Smart'])
const source = readFileSync(new URL('../src/pages/serial-panel.js', import.meta.url), 'utf8')
const ast = parseSync(source, {configFile: false, babelrc: false, parserOpts: {plugins: ['decorators', 'decoratorAutoAccessors']}})
const panel = ast.program.body.find(node => node.type === 'ClassDeclaration')
const method = name => panel.body.body.find(node => node.key?.name === name)
let saved, alerts = 0
const run = (name, state, event = {preventDefault() {}}, elrs = {settings: {has_serial1_pins: true}}) => {
    const node = method(name)
    const fn = new Function('saveOptionsAndConfig', 'showAlert', 'elrsState', 'PWM_MODE_SERIAL2RX', 'PWM_MODE_SERIAL2TX', `return ({${source.slice(node.start, node.end)}}).${name}`)
    return fn(value => { saved = value }, () => { ++alerts }, elrs, 14, 15).call(state, event)
}
const state = {serial1Protocol: 0, serial2Protocol: 0, isAirport: false, requestUpdate() {}}
run('_updateSerial2', state, {target: {value: '13'}})
assert.equal(state.serial2Protocol, 13)
assert.equal(run('_hasSerial2', state), true)
run('_saveSerial', state)
assert.equal(saved.config['serial-protocol'], 0)
assert.equal(saved.config['serial1-protocol'], 13)
saved = undefined
state.serial1Protocol = 11
run('_saveSerial', state)
assert.equal(saved, undefined, 'Both-Smart selection must be rejected before either HTTP request')
assert.equal(alerts, 1)
state.isAirport = true
state.serial1Protocol = 12
run('_saveSerial', state)
assert.equal(saved.config['serial-protocol'], 0)
assert.equal(saved.config['serial1-protocol'], 13)
state.isAirport = false
state.serial1Protocol = 0
state.serial2Protocol = 1
run('_saveSerial', state)
assert.equal(saved.config['serial-protocol'], 0)
assert.equal(saved.config['serial1-protocol'], 1, 'Both stock CRSF interfaces remain selectable')
assert.equal(run('_hasSerial2', state, undefined, {
    settings: {has_serial1_pins: true},
    config: {'srxl2-supported': false, pwm: [{config: 0, features: 0}]}
}), true, 'Fixed secondary TX-only capability is independent of primary support or PWM selection')
assert.equal(run('_hasSerial2', state, undefined, {
    settings: {}, config: {'serial1-protocol': 0, pwm: [{config: 15 << 16, features: 64}]}
}), true, 'A selected PWM TX alone enables the secondary selector')
console.log('Serial option rendering/protocol ID checks passed')
