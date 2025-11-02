import {html, LitElement} from "lit"
import {customElement, state} from "lit/decorators.js"
import '../assets/mui.js'
import {_renderOptions} from "../utils/libs.js"
import {elrsState, saveOptionsAndConfig} from "../utils/state.js"

@customElement('serial-panel')
class SerialPanel extends LitElement {
    SERIAL_OPTIONS = ["CRSF", "Inverted CRSF", "SBUS", "Inverted SBUS", "SUMD", "DJI RS Pro", "HoTT Telemetry", "MAVLINK", "DisplayPort", "GPS"]

    @state() accessor serial1Protocol
    @state() accessor serial2Protocol
    @state() accessor baudRate
    @state() accessor sbusFailsafe
    @state() accessor isAirport

    createRenderRoot() {
        this.isAirport = elrsState.options['is-airport']
        this.serial1Protocol = this.isAirport ? 10 : elrsState.config['serial-protocol']
        this.serial2Protocol = elrsState.config['serial1-protocol']
        this.baudRate = elrsState.options['rcvr-uart-baud']
        this.sbusFailsafe = elrsState.config['sbus-failsafe']
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
                            ${_renderOptions([...this.SERIAL_OPTIONS, "AirPort"], this.serial1Protocol)}
                        </select>
                        <label>Serial 1 Protocol</label>
                    </div>
                    ` : ''}
                    ${this._hasSerial2() ? html`
                    <div class="mui-select">
                        <select name='serial1-protocol' @change=${this._updateSerial2}>
                            ${_renderOptions(["Off", ...this.SERIAL_OPTIONS], this.serial2Protocol)}
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
                    <button class="mui-btn mui-btn--small mui-btn--primary"
                            ?disabled="${!this._changed()}"
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
        if (!elrsState.config['pwm']) return true
        for(const pwm of elrsState.config.pwm) {
            const mode = (pwm.config >> 15) & 0xF
            if (mode === 9 || mode === 10 )
                return true
        }
        return false
    }

    _hasSerial2() {
        if (!elrsState.config['pwm']) {
            return elrsState.config['serial1-protocol'] !== undefined
        }
        for(const pwm of elrsState.config.pwm) {
            const mode = (pwm.config >> 15) & 15
            if (mode === 13 || mode === 14)
                return true
        }
        return false
    }

    _updateSerial1(e) {
        this.serial1Protocol = parseInt(e.target.value)
        this.isAirport = this.serial1Protocol === 10
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

    _configChanged() {
        return (!this.isAirport && this.serial1Protocol !== elrsState.config['serial-protocol']) ||
            this.serial2Protocol !== elrsState.config['serial1-protocol'] ||
            this.sbusFailsafe !== elrsState.config['sbus-failsafe']
    }
    _optionsChanged() {
        return this.isAirport !== elrsState.options['is-airport'] ||
            this.baudRate !== elrsState.options['rcvr-uart-baud']
    }

    _changed() {
        return this._configChanged() || this._optionsChanged()
    }

    _saveSerial(e) {
        e.preventDefault()
        saveOptionsAndConfig({
                options: {
                    ...elrsState.options,
                    'is-airport': this.isAirport,
                    'rcvr-uart-baud': this.baudRate
                },
                config: {
                    ...elrsState.config,
                    'serial-protocol': this.isAirport ? 0 : this.serial1Protocol,
                    'serial1-protocol': this.serial2Protocol,
                    'sbus-failsafe': this.sbusFailsafe
                }
            },
            () => {this.requestUpdate()}
        )
    }
}
