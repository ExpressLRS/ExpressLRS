import {html, LitElement} from "lit";
import {customElement, state} from "lit/decorators.js";
import '../assets/mui.js';
import {_renderOptions} from "../utils/libs.js";
import {elrsState, saveConfig, saveOptions} from "../utils/state.js";
import {cuteAlert, postJSON} from "../utils/feedback.js";

@customElement('serial-panel')
class SerialPanel extends LitElement {
    SERIAL_OPTIONS = ["CRSF", "Inverted CRSF", "SBUS", "Inverted SBUS", "SUMD", "DJI RS Pro", "HoTT Telemetry", "MAVLINK", "DisplayPort", "GPS"]

    @state() accessor serial1Protocol
    @state() accessor serial2Protocol
    @state() accessor baudRate
    @state() accessor sbusFailsafe
    @state() accessor isAirport

    createRenderRoot() {
        this.isAirport = elrsState.options['is-airport'];
        this.serial1Protocol = this.isAirport ? 10 : elrsState.config['serial-protocol']
        this.serial2Protocol = elrsState.config['serial1-protocol']
        this.baudRate = elrsState.options['rcvr-uart-baud']
        this.sbusFailsafe = elrsState.config['sbus-failsafe']
        this._saveSerial = this._saveSerial.bind(this)
        this._saveConfig = this._saveConfig.bind(this)
        return this;
    }

    render() {
        return html`
            <div class="mui-panel mui--text-title">Serial/UART Protocols</div>
            ${this._hasSerial1() || this._hasSerial2() ? html`
            <div class="mui-panel">
                <p>Set the protocol(s) used to communicate with the flight controller or other external devices.</p>
                <form>
                    ${this._hasSerial1() ? html`
                    <div class="mui-select">
                        <select id='serial-protocol' name='serial-protocol'
                            @change=${this._updateSerial1}
                        >
                            ${_renderOptions([...this.SERIAL_OPTIONS, "AirPort"], this.serial1Protocol)}
                        </select>
                        <label for='serial-protocol'>Serial 1 Protocol</label>
                    </div>
                    ` : ''}
                    ${this._hasSerial2() ? html`
                    <div class="mui-select">
                        <select id='serial1-protocol' name='serial1-protocol'
                            @change=${this._updateSerial2}
                        >
                            ${_renderOptions(["Off", ...this.SERIAL_OPTIONS], this.serial2Protocol)}
                        </select>
                        <label for='serial1-protocol'>Serial 2 Protocol</label>
                    </div>
                    ` : ''}
                    ${this._displayBaudRate() ? html`
                    <div class="mui-textfield">
                        <input size='7' type='number'
                               @input=${(e) => this.baudRate = parseInt(e.target.value)}
                               value=${this.baudRate}/>
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
                            <select id='sbus-failsafe' name='serial-failsafe'>
                                <option value='0'>No Pulses</option>
                                <option value='1'>Last Position</option>
                            </select>
                            <label for="sbus-failsafe">SBUS Failsafe</label>
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
        `;
    }

    _hasSerial1() {
        for(const pwm of elrsState.config.pwm) {
            const fs = pwm.config & 0x3FF
            const fsMode = (pwm.config >> 20) & 0x3
            const ch = (pwm.config >> 10) & 0xF
            const inv = (pwm.config >> 14) & 0x1
            const mode = (pwm.config >> 15) & 0xF
            const narrow = (pwm.config >> 19) & 0x1
            console.log(fs, fsMode, ch, inv, mode, narrow);
            if (mode === 9 || mode === 10 )
                return true
        }
        return false
    }

    _hasSerial2() {
        for(const pwm of elrsState.config.pwm) {
            const mode = (pwm.config >> 15) & 15
            if (mode === 13 || mode === 14)
                return true
        }
        return false
    }

    _updateSerial1(e) {
        this.serial1Protocol = parseInt(e.target.value);
        this.isAirport = this.serial1Protocol === 10;
    }

    _updateSerial2(e) {
        this.serial2Protocol = parseInt(e.target.value);
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

    _saveConfig() {
        if (this._configChanged()) {
            const changes = {
                ...elrsState.config,
                'serial-protocol': this.isAirport ? 0 : this.serial1Protocol,
                'serial1-protocol': this.serial2Protocol,
                'sbus-failsafe': this.sbusFailsafe
            }
            saveConfig(changes, () => {
                elrsState.config = changes
                this.requestUpdate()
            })
        }
    }

    _saveSerial(e) {
        e.preventDefault()
        if (this._optionsChanged()) {
            const changes = {
                ...elrsState.options,
                'is-airport': this.isAirport,
                'rcvr-uart-baud': this.baudRate
            }
            postJSON('/options.json', changes, {
                onload: async () => {
                    elrsState.options = changes
                    this._saveConfig()
                    this.requestUpdate()
                },
                onerror: async (xhr) => {
                    await cuteAlert({ type: 'error', title: 'Configuration Update Failed', message: xhr.responseText || 'Request failed' })
                }
            })
        }
        else {
            this._saveConfig()
        }
    }
}
