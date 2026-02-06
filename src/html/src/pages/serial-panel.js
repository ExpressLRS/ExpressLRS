import {html, LitElement} from "lit"
import {customElement, state} from "lit/decorators.js"
import '../assets/mui.js'
import {_renderOptions} from "../utils/libs.js"
import {elrsState, saveOptionsAndConfig} from "../utils/state.js"
import {PWM_MODE_SERIAL, PWM_MODE_SERIAL2RX, PWM_MODE_SERIAL2TX} from "./connections-panel.js";
import {SERIAL_OPTIONS1, SERIAL_OPTIONS2} from "../utils/globals.js";

@customElement('serial-panel')
class SerialPanel extends LitElement {

    PROTOCOL_AIRPORT = SERIAL_OPTIONS1.length - 1

    @state() accessor serial1Protocol
    @state() accessor serial2Protocol
    @state() accessor baudRate
    @state() accessor sbusFailsafe
    @state() accessor isAirport
    @state() accessor djiArmed

    createRenderRoot() {
        this.isAirport = elrsState.options['is-airport']
        this.serial1Protocol = this.isAirport ? this.PROTOCOL_AIRPORT : elrsState.config['serial-protocol']
        this.serial2Protocol = elrsState.config['serial1-protocol']
        this.baudRate = elrsState.options['rcvr-uart-baud']
        this.sbusFailsafe = elrsState.config['sbus-failsafe']
        this.djiArmed = elrsState.options['dji-permanently-armed']
        this._saveSerial = this._saveSerial.bind(this)
        return this
    }

    render() {
        return html`
            <div class="mui-panel mui--text-title">Serial/UART Options</div>
            ${this._hasSerial1() || this._hasSerial2() ? html`
            <div class="mui-panel">
                <p>Set the protocol(s) used to communicate with the flight controller or other external devices.</p>
                <form>
                    ${this._hasSerial1() ? html`
                    <div class="mui-select">
                        <select name='serial-protocol' @change=${this._updateSerial1}>
                            ${_renderOptions(SERIAL_OPTIONS1, this.serial1Protocol)}
                        </select>
                        <label>Serial 1 Protocol</label>
                    </div>
                    ` : ''}
                    ${this._hasSerial2() ? html`
                    <div class="mui-select">
                        <select name='serial1-protocol' @change=${this._updateSerial2}>
                            ${_renderOptions(SERIAL_OPTIONS2, this.serial2Protocol)}
                        </select>
                        <label>Serial 2 Protocol</label>
                    </div>
                    ` : ''}
                    ${this._displayBaudRate() ? html`
                    <div class="mui-textfield">
                        <input size='7' type='number'
                               @input=${(e) => this.baudRate = parseInt(e.target.value)}
                               .value="${this.baudRate}" />
                        <label>CRSF/Airport baud</label>
                    </div>
                    ` : ''}
                    ${this._sbusSelected() ? html`
                    <div id="sbus-config">
                        <div class="mui--text-title">SBUS Failsafe</div>
                        Set the failsafe behaviour when using the SBUS protocol:<br/>
                        <ul>
                            <li>"No Pulses" stops sending SBUS data when a connection to the transmitter is lost
                            </li>
                            <li>"Last Position" continues to send the last received channel data along with the
                                FAILSAFE
                                bit set
                            </li>
                        </ul>
                        <br/>
                        <div class="mui-select">
                            <select name='serial-failsafe'>
                                <option value='0'>No Pulses</option>
                                <option value='1'>Last Position</option>
                            </select>
                            <label>SBUS Failsafe</label>
                        </div>
                    </div>
                    ` : ''}
                    ${this._displayPortSelected() ? html`
                    <div class="mui-checkbox">
                        <input id="dji" type='checkbox'
                               ?checked="${this.djiArmed}"
                               @change="${(e) => {this.djiArmed = e.target.checked}}"/>
                        <label for="dji">Permanently arm DJI air units</label>
                    </div>
                    ` : ''}
                    <button class="mui-btn mui-btn--small mui-btn--primary"
                            ?disabled="${!this.checkChanged()}"
                            @click="${this._saveSerial}"
                    >Save</button>
                </form>
            </div>
            `: html`
            <div class="mui-panel info-bg">
                This is a PWM receiver and none of the pins have been configured as serial IO pins.<br>
                To enable serial IO, go to the <a href="#connections">connections</a> menu and configure one or more pins as Serial RX or TX.
            </div>
            `}
        `
    }

    _hasSerial1() {
        // If there's no PWM pins then serial must be enabled
        if (!elrsState.config['pwm']) return true
        // If a PWM pin is defined as serial, then it should be enabled
        for(const pwm of elrsState.config.pwm) {
            const mode = (pwm.config >> 16) & 0xF
            if (mode === PWM_MODE_SERIAL)
                return true
        }
        // If any of the PWM pins are defined to support serial (but it's not selected) then disabled serial
        for(const pwm of elrsState.config.pwm) {
            if (pwm.features & 3 !== 0)
                return false
        }
        // No PWM pins are defined as serial so use what the hardware dictates
        return !!elrsState.settings.has_serial_pins
    }

    _hasSerial2() {
        if (!elrsState.config['pwm']) {
            return elrsState.config['serial1-protocol'] !== undefined
        }
        for(const pwm of elrsState.config.pwm) {
            const mode = (pwm.config >> 16) & 15
            if (mode === PWM_MODE_SERIAL2RX || mode === PWM_MODE_SERIAL2TX)
                return true
        }
        return false
    }

    _updateSerial1(e) {
        this.serial1Protocol = parseInt(e.target.value)
        this.isAirport = this.serial1Protocol === this.PROTOCOL_AIRPORT
        if (this.serial1Protocol === 0 || this.serial1Protocol === 1) {
            this.baudRate = 420000
            this.requestUpdate()
        }
    }

    _updateSerial2(e) {
        this.serial2Protocol = parseInt(e.target.value)
    }

    _displayBaudRate() {
        return this.isAirport || this.serial1Protocol === 0 || this.serial1Protocol === 1 || this.serial2Protocol === 1 || this.serial2Protocol === 2
    }

    _sbusSelected() {
        return this.serial1Protocol === 2 || this.serial1Protocol === 3 || this.serial2Protocol === 3 || this.serial2Protocol === 4
    }

    _displayPortSelected() {
        return this.serial1Protocol === 8 || this.serial2Protocol === 9
    }

    _configChanged() {
        return (!this.isAirport && this.serial1Protocol !== elrsState.config['serial-protocol']) ||
            this.serial2Protocol !== elrsState.config['serial1-protocol'] ||
            this.sbusFailsafe !== elrsState.config['sbus-failsafe']
    }
    _optionsChanged() {
        return this.isAirport !== elrsState.options['is-airport'] ||
            this.baudRate !== elrsState.options['rcvr-uart-baud'] ||
            this.djiArmed !== elrsState.options['dji-permanently-armed']
    }

    checkChanged() {
        return this._configChanged() || this._optionsChanged()
    }

    _saveSerial(e) {
        e.preventDefault()
        saveOptionsAndConfig({
                options: {
                    'is-airport': this.isAirport,
                    'rcvr-uart-baud': this.baudRate,
                    'dji-permanently-armed': this.djiArmed,
                },
                config: {
                    'serial-protocol': this.isAirport ? 0 : this.serial1Protocol,
                    'serial1-protocol': this.serial2Protocol,
                    'sbus-failsafe': this.sbusFailsafe
                }
            },
            () => {this.requestUpdate()}
        )
    }
}
