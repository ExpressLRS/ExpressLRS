import {html, LitElement} from "lit"
import {customElement, state} from "lit/decorators.js"
import {_renderOptions, _uintInput} from "../utils/libs.js"
import {elrsState, saveOptionsAndConfig} from "../utils/state.js"
import {PWM_MODE_SERIAL, PWM_MODE_SERIAL2RX, PWM_MODE_SERIAL2TX} from "./connections-panel.js"
import {SERIAL_OPTIONS1, SERIAL_OPTIONS2} from "../utils/globals.js"

@customElement('serial-panel')
class SerialPanel extends LitElement {

    PROTOCOL_AIRPORT = SERIAL_OPTIONS1.length - 1

    @state() accessor serial1Protocol
    @state() accessor serial2Protocol
    @state() accessor baudRate
    @state() accessor sbusFailsafe
    @state() accessor isAirport
    @state() accessor djiArmed
    @state() accessor enableMspOsd
    @state() accessor enableOsdRssi
    @state() accessor enableOsdLq
    @state() accessor osdChannelMonitor
    @state() accessor osdChannelUsePercent
    @state() accessor osdRssiRow
    @state() accessor osdRssiCol
    @state() accessor osdLqRow
    @state() accessor osdLqCol
    @state() accessor osdChannelRow
    @state() accessor osdChannelCol

    createRenderRoot() {
        this.isAirport = elrsState.options['is-airport']
        this.serial1Protocol = this.isAirport ? this.PROTOCOL_AIRPORT : elrsState.config['serial-protocol']
        this.serial2Protocol = elrsState.config['serial1-protocol']
        this.baudRate = elrsState.options['rcvr-uart-baud']
        this.sbusFailsafe = elrsState.config['sbus-failsafe']
        this.djiArmed = elrsState.options['dji-permanently-armed']
        this.enableMspOsd = !!elrsState.options['enable-msp-osd']
        this.enableOsdRssi = !!elrsState.options['enable-osd-rssi']
        this.enableOsdLq = !!elrsState.options['enable-osd-lq']
        this.osdChannelMonitor = elrsState.options['osd-channel-monitor'] || 0
        this.osdChannelUsePercent = !!elrsState.options['osd-channel-use-percent']
        this.osdRssiRow = elrsState.options['osd-rssi-row'] || 1
        this.osdRssiCol = elrsState.options['osd-rssi-col'] || 1
        this.osdLqRow = elrsState.options['osd-lq-row'] || 2
        this.osdLqCol = elrsState.options['osd-lq-col'] || 1
        this.osdChannelRow = elrsState.options['osd-channel-row'] || 5
        this.osdChannelCol = elrsState.options['osd-channel-col'] || 1
        this._saveSerial = this._saveSerial.bind(this)
        this._setSbusFailsafe = this._setSbusFailsafe.bind(this)
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
                            <select name='serial-failsafe'
                                    @change=${this._setSbusFailsafe}>
                                ${_renderOptions(['No Pulses', 'Last Position'], this.sbusFailsafe)}
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
                    <div class="mui--text-title">MSP OSD Settings</div>
                    <div class="mui-checkbox">
                        <input id="enable-msp-osd" type='checkbox'
                                ?checked="${this.enableMspOsd}"
                                @change="${(e) => {this.enableMspOsd = e.target.checked}}"/>
                        <label for="enable-msp-osd">Enable MSP DisplayPort OSD</label>
                    </div>
                    ${this.enableMspOsd ? html`
                        <div class="mui-checkbox">
                            <input id="enable-osd-rssi" type='checkbox'
                                    ?checked="${this.enableOsdRssi}"
                                    @change="${(e) => {this.enableOsdRssi = e.target.checked}}"/>
                            <label for="enable-osd-rssi">Show RSSI dBm on OSD</label>
                        </div>
                        ${this.enableOsdRssi ? html`
                            <div style="display: flex; gap: 1rem; margin-left: 1.5rem;">
                                <div class="mui-textfield">
                                    <input id="osd-rssi-row" min="0" max="50" type='number'
                                        @input=${(e) => {const v = parseInt(e.target.value); this.osdRssiRow = isNaN(v) ? 0 : v > 50 ? 50 : v;}}
                                        .value="${this.osdRssiRow}"
                                        @keypress="${_uintInput}"/>
                                    <label for="osd-rssi-row">Row</label>
                                </div>
                                <div class="mui-textfield">
                                    <input id="osd-rssi-col" min="0" max="53" type='number'
                                        @input=${(e) => {const v = parseInt(e.target.value); this.osdRssiCol = isNaN(v) ? 0 : v > 53 ? 53 : v;}}
                                        .value="${this.osdRssiCol}"
                                        @keypress="${_uintInput}"/>
                                    <label for="osd-rssi-col">Col</label>
                                </div>
                            </div>
                        ` : ''}
                        <div class="mui-checkbox">
                            <input id="enable-osd-lq" type='checkbox'
                                    ?checked="${this.enableOsdLq}"
                                    @change="${(e) => {this.enableOsdLq = e.target.checked}}"/>
                            <label for="enable-osd-lq">Show Link Quality (LQ) on OSD</label>
                        </div>
                        ${this.enableOsdLq ? html`
                            <div style="display: flex; gap: 1rem; margin-left: 1.5rem;">
                                <div class="mui-textfield">
                                    <input id="osd-lq-row" min="0" max="50" type='number'
                                        @input=${(e) => {const v = parseInt(e.target.value); this.osdLqRow = isNaN(v) ? 0 : v > 50 ? 50 : v;}}
                                        .value="${this.osdLqRow}"
                                        @keypress="${_uintInput}"/>
                                    <label for="osd-lq-row">Row</label>
                                </div>
                                <div class="mui-textfield">
                                    <input id="osd-lq-col" min="0" max="53" type='number'
                                        @input=${(e) => {const v = parseInt(e.target.value); this.osdLqCol = isNaN(v) ? 0 : v > 53 ? 53 : v;}}
                                        .value="${this.osdLqCol}"
                                        @keypress="${_uintInput}"/>
                                    <label for="osd-lq-col">Col</label>
                                </div>
                            </div>
                        ` : ''}
                        <div id="channel-monitor-config">
                            <div>RC Channel Monitor</div>
                            Set 0 to disable, or greater than 0 to specify how many channels to monitor on OSD:<br/>
                            <div class="mui-textfield">
                                <input id="osd-channel-monitor" min="0" max="16" type='number'
                                    @input=${(e) => {const v = parseInt(e.target.value); this.osdChannelMonitor = isNaN(v) ? 0 : v > 16 ? 16 : v;}}
                                    .value="${this.osdChannelMonitor}"
                                    @keypress="${_uintInput}"/>
                                <label for="osd-channel-monitor">Channels to Monitor (0 = Disabled)</label>
                            </div>
                            ${this.osdChannelMonitor > 0 ? html`
                                <div style="display: flex; gap: 1rem;">
                                    <div class="mui-textfield">
                                        <input id="osd-channel-row" min="0" max="50" type='number'
                                            @input=${(e) => {const v = parseInt(e.target.value); this.osdChannelRow = isNaN(v) ? 0 : v > 50 ? 50 : v;}}
                                            .value="${this.osdChannelRow}"
                                            @keypress="${_uintInput}"/>
                                        <label for="osd-channel-row">Start Row</label>
                                    </div>
                                    <div class="mui-textfield">
                                        <input id="osd-channel-col" min="0" max="53" type='number'
                                            @input=${(e) => {const v = parseInt(e.target.value); this.osdChannelCol = isNaN(v) ? 0 : v > 53 ? 53 : v;}}
                                            .value="${this.osdChannelCol}"
                                            @keypress="${_uintInput}"/>
                                        <label for="osd-channel-col">Start Col</label>
                                    </div>
                                </div>
                                <div class="mui-checkbox">
                                    <input id="osd-channel-use-percent" type='checkbox'
                                            ?checked="${this.osdChannelUsePercent}"
                                            @change="${(e) => {this.osdChannelUsePercent = e.target.checked}}"/>
                                    <label for="osd-channel-use-percent">Display channel values as percentage (-100% to 100%)</label>
                                </div>
                            ` : ''}
                        </div>
                        ` : ''}
                    ` : ''}
                    <button class="mui-btn mui-btn--small mui-btn--primary"
                            ?disabled="${!this.checkChanged()}"
                            @click="${this._saveSerial}"
                    >Save</button>
                </form>
            </div>
            `: html`
            <div class="mui-panel info-bg">
                ${this._canConfigureSerialOnPwm() || elrsState.settings.has_serial_pins ? 
                    html`This is a PWM receiver and none of the pins have been configured as serial IO pins.<br>
                    To enable serial IO, go to the <a href="#connections">connections</a> menu and configure one or more pins as Serial RX, Serial TX, Serial2 RX, or Serial2 TX.
                    ` : html`This receiver does not have any serial-capable PWM pins available.`
                }
            </div>
            `}
        `
    }

    _canConfigureSerialOnPwm() {
        if (!elrsState.config['pwm']) return false
        return elrsState.config.pwm.some((pwm) => (pwm.features & (3 | 96)) !== 0)
    }

    _hasSerial1() {
        // If theres no PWM pins then serial must be enabled
        if (!elrsState.config['pwm']) return true
        // If a PWM pin is defined as serial, then it should be enabled
        for(const pwm of elrsState.config.pwm) {
            const mode = (pwm.config >> 16) & 0xF
            if (mode === PWM_MODE_SERIAL && (pwm.features & 3) !== 0)
                return true
        }
        // If any of the PWM pins are defined to support serial (but it's not selected) then disabled serial
        for(const pwm of elrsState.config.pwm) {
            if ((pwm.features & 3) !== 0)
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
            if ((mode === PWM_MODE_SERIAL2RX || mode === PWM_MODE_SERIAL2TX) && (pwm.features & 96) !== 0)
                return true
        }
        return false
    }

    _updateSerial1(e) {
        this.serial1Protocol = Number(e.target.value)
        this.isAirport = this.serial1Protocol === this.PROTOCOL_AIRPORT
        if (this.serial1Protocol === 0 || this.serial1Protocol === 1) {
            this.baudRate = 420000
            this.requestUpdate()
        }
    }

    _updateSerial2(e) {
        this.serial2Protocol = Number(e.target.value)
    }

    _setSbusFailsafe(e) {
        this.sbusFailsafe = Number(e.target.value)
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
            this.djiArmed !== elrsState.options['dji-permanently-armed'] ||
            this.enableMspOsd !== !!elrsState.options['enable-msp-osd'] ||
            this.enableOsdRssi !== !!elrsState.options['enable-osd-rssi'] ||
            this.enableOsdLq !== !!elrsState.options['enable-osd-lq'] ||
            this.osdChannelMonitor !== (elrsState.options['osd-channel-monitor'] || 0) ||
            this.osdChannelUsePercent !== !!elrsState.options['osd-channel-use-percent'] ||
            this.osdRssiRow !== (elrsState.options['osd-rssi-row'] || 1) ||
            this.osdRssiCol !== (elrsState.options['osd-rssi-col'] || 1) ||
            this.osdLqRow !== (elrsState.options['osd-lq-row'] || 2) ||
            this.osdLqCol !== (elrsState.options['osd-lq-col'] || 1) ||
            this.osdChannelRow !== (elrsState.options['osd-channel-row'] || 5) ||
            this.osdChannelCol !== (elrsState.options['osd-channel-col'] || 1)
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
                    'enable-msp-osd': this.enableMspOsd,
                    'enable-osd-rssi': this.enableOsdRssi,
                    'enable-osd-lq': this.enableOsdLq,
                    'osd-channel-monitor': this.osdChannelMonitor,
                    'osd-channel-use-percent': this.osdChannelUsePercent,
                    'osd-rssi-row': this.osdRssiRow,
                    'osd-rssi-col': this.osdRssiCol,
                    'osd-lq-row': this.osdLqRow,
                    'osd-lq-col': this.osdLqCol,
                    'osd-channel-row': this.osdChannelRow,
                    'osd-channel-col': this.osdChannelCol,
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
