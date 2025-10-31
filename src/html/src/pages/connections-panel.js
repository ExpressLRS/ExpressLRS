import {html, LitElement} from "lit";
import {customElement} from "lit/decorators.js";
import '../assets/mui.js';
import {elrsState, saveConfig} from "../utils/state.js";
import {_} from "../utils/libs.js";

@customElement('connections-panel')
class ConnectionsPanel extends LitElement {
    pinModes = []
    pinRxIndex = undefined
    pinTxIndex = undefined

    createRenderRoot() {
        return this
    }

    render() {
        return html`
            <style>
                .connections-panel-root { container-type: inline-size; }

                .connections-panel-root .connections-mobile-warning { display: none; }

                /* If the container is too narrow in terms of characters, show the warning and hide the table */
                @container (max-width: 80ch) {
                    .connections-panel-root .connections-mobile-warning { display: block; }
                    .connections-panel-root .connections-panel { display: none !important; }
                }

                /* Fallback for browsers without container queries: use an em-based viewport rule */
                @supports not (container-type: inline-size) {
                    @media (max-width: 48em) {
                        .connections-panel-root .connections-mobile-warning { display: block; }
                        .connections-panel-root .connections-panel { display: none !important; }
                    }
                }
            </style>
            <div class="connections-panel-root">
                <div class="mui-panel mui--text-title">PWM Pin Functions</div>
                <div class="mui-panel warning-bg connections-mobile-warning">
                    <div class="mui--text-title" style="margin-bottom: 8px;">Rotate to landscape</div>
                    <p>
                        The connections panel is too wide for small screens in portrait mode. Please rotate your device to
                        landscape mode to view and edit the settings.
                    </p>
                </div>
                <div class="mui-panel connections-panel">
                    Set PWM output mode and failsafe positions.
                    <form>
                        <div class="mui-panel pwmpnl">
                            <table class="pwmtbl mui-table">
                                <thead>
                                <tr>
                                    <th class="fixed-column">Output</th><th class="mui--text-center fixed-column">Features</th><th>Mode</th><th>Input</th><th class="mui--text-center fixed-column">Invert?</th><th class="mui--text-center fixed-column">750us?</th><th class="mui--text-center fixed-column pwmitm">Failsafe Mode</th><th class="mui--text-center fixed-column pwmitm">Failsafe Pos</th>
                                </tr>
                                </thead>
                                <tbody>
                                    ${this._renderConnectionPins()}
                                </tbody>
                            </table>
                        </div>
                        <button class="mui-btn mui-btn--small mui-btn--primary" @click="${this._savePwmConfig}">Save</button>
                    </form>
                    <div class="mui-divider"></div>
                    <ul>
                        <li><b>Output:</b> Receiver output pin</li>
                        <li><b>Features:</b> If an output is capable of supporting another function, that is indicated
                            here
                        </li>
                        <li><b>Mode:</b> Output frequency, 10KHz 0-100% duty cycle, binary On/Off, DShot, Serial, or I2C
                            (some options are pin dependant)
                        </li>
                        <ul>
                            <li>When enabling serial pins, be sure to select the <b>Serial Protocol</b> below and <b>UART
                                baud</b> on the <b><a href="#serial">Serial</a></b> page in the menu
                            </li>
                        </ul>
                        <li><b>Input:</b> Input channel from the handset</li>
                        <li><b>Invert:</b> Invert input channel position</li>
                        <li><b>750us:</b> Use half pulse width (494-1006us) with center 750us instead of 988-2012us</li>
                        <li><b>Failsafe</b>
                            <ul>
                                <li>"Set Position" sets the servo to an absolute "Failsafe Pos"
                                    <ul>
                                        <li>Does not use "Invert" flag</li>
                                        <li>Value will be halved if "750us" flag is set</li>
                                        <li>Will be converted to binary for "On/Off" mode (>1500us = HIGH)</li>
                                    </ul>
                                </li>
                                <li>"No Pulses" stops sending pulses
                                    <ul>
                                        <li>Unpowers servos</li>
                                        <li>May disarm ESCs</li>
                                    </ul>
                                </li>
                                <li>"Last Position" continues sending last received channel position</li>
                            </ul>
                        </li>
                    </ul>
                </div>
            </div>
        `;
    }

    firstUpdated() {
        elrsState.config.pwm.forEach((item, index) => {
            const modeField = _(`pwm_${index}_mode`)
            this._pinModeChange(modeField, index)
            const failsafeModeField = _(`pwm_${index}_fsmode`)
            this._failsafeModeChange(failsafeModeField, index)
        })
    }

    _enumSelectGenerate(id, val, arOptions, onchange) {
        return html`
            <div class="mui-select compact">
                <select id="${id}" class="pwmitm" @change="${onchange}">
                    ${arOptions.map((item, idx) => {
                        if (item) {
                            return html`<option value="${idx}" ?selected=${idx === val} ?disabled=${item === 'Disabled'}>${item}</option>`
                        }
                        return null
                    })}
                </select>
            </div>
        `
    }

    _generateFeatureBadges(features) {
        let str = []
        if (!!(features & 1)) str.push(html`<span style="color: #696969; background-color: #a8dcfa" class="badge">TX</span>`)
        else if (!!(features & 2)) str.push(html`<span style="color: #696969; background-color: #d2faa8" class="badge">RX</span>`)
        if ((features & 12) === 12) str.push(html`<span style="color: #696969; background-color: #fab4a8" class="badge">I2C</span>`)
        else if (!!(features & 4)) str.push(html`<span style="color: #696969; background-color: #fab4a8" class="badge">SCL</span>`)
        else if (!!(features & 8)) str.push(html`<span style="color: #696969; background-color: #fab4a8" class="badge">SDA</span>`)

        // Serial2
        if ((features & 96) === 96) str.push(html`<span style="color: #696969; background-color: #36b5ff" class="badge">Serial2</span>`)
        else if (!!(features & 32)) str.push(html`<span style="color: #696969; background-color: #36b5ff" class="badge">RX2</span>`)
        else if (!!(features & 64)) str.push(html`<span style="color: #696969; background-color: #36b5ff" class="badge">TX2</span>`)

        return str
    }

    _renderConnectionPins() {
        this.pinRxIndex = undefined
        this.pinTxIndex = undefined
        this.pinModes = []
        // arPwm is an array of raw integers [49664,50688,51200]. 10 bits of failsafe position, 4 bits of input channel, 1 bit invert, 4 bits mode, 1 bit for narrow/750us
        const htmlFields = []
        elrsState.config.pwm.forEach((item, index) => {
            const failsafe = (item.config & 1023) + 988 // 10 bits
            const failsafeMode = (item.config >> 20) & 3 // 2 bits
            const ch = (item.config >> 10) & 15 // 4 bits
            const inv = (item.config >> 14) & 1
            const mode = (item.config >> 15) & 15 // 4 bits
            const narrow = (item.config >> 19) & 1
            const features = item.features
            const modes = ['50Hz', '60Hz', '100Hz', '160Hz', '333Hz', '400Hz', '10KHzDuty', 'On/Off']
            if (features & 16) {
                modes.push('DShot')
            } else {
                modes.push(undefined)
            }
            if (features & 1) {
                modes.push('Serial TX')
                modes.push(undefined)  // SCL
                modes.push(undefined)  // SDA
                modes.push(undefined)  // true PWM
                this.pinRxIndex = index
            } else if (features & 2) {
                modes.push('Serial RX')
                modes.push(undefined)  // SCL
                modes.push(undefined)  // SDA
                modes.push(undefined)  // true PWM
                this.pinTxIndex = index
            } else {
                modes.push(undefined)  // Serial
                if (features & 4) {
                    modes.push('I2C SCL')
                } else {
                    modes.push(undefined)
                }
                if (features & 8) {
                    modes.push('I2C SDA')
                } else {
                    modes.push(undefined)
                }
                modes.push(undefined)  // true PWM
            }

            if (features & 32) {
                modes.push('Serial2 RX')
            } else {
                modes.push(undefined)
            }
            if (features & 64) {
                modes.push('Serial2 TX')
            } else {
                modes.push(undefined)
            }

            htmlFields.push(html`
                <tr><td class="mui--text-center mui--text-title">${index + 1}</td>
                <td>${this._generateFeatureBadges(features)}</td>
                <td>${this._enumSelectGenerate(`pwm_${index}_mode`, mode, modes, (e) => {this._pinModeChange(e.target, index)})}</td>
                <td>${this._enumSelectGenerate(`pwm_${index}_ch`, ch,
                        ['ch1', 'ch2', 'ch3', 'ch4',
                            'ch5 (AUX1)', 'ch6 (AUX2)', 'ch7 (AUX3)', 'ch8 (AUX4)',
                            'ch9 (AUX5)', 'ch10 (AUX6)', 'ch11 (AUX7)', 'ch12 (AUX8)',
                            'ch13 (AUX9)', 'ch14 (AUX10)', 'ch15 (AUX11)', 'ch16 (AUX12)'])}</td>
                <td><div class="mui-checkbox mui--text-center"><input type="checkbox" id="pwm_${index}_inv" ?checked="${inv}"></div></td>
                <td><div class="mui-checkbox mui--text-center"><input type="checkbox" id="pwm_${index}_nar" ?checked="${narrow}"}></div></td>
                <td>${this._enumSelectGenerate(`pwm_${index}_fsmode`, failsafeMode, ['Set Position', 'No Pulses', 'Last Position'],
                        (e) => {this._failsafeModeChange(e.target, index)})}</td>
                <td><div class="mui-textfield compact"><input id="pwm_${index}_fs" value="${failsafe}" size="6" class="pwmitm" /></div></td></tr>
            `);
            this.pinModes[index] = mode
        });
        return htmlFields
    }

    _pinModeChange(pinMode, index) {
        const setDisabled = (index, onoff) => {
            _(`pwm_${index}_ch`).disabled = onoff
            _(`pwm_${index}_inv`).disabled = onoff
            _(`pwm_${index}_nar`).disabled = onoff
            _(`pwm_${index}_fs`).disabled = onoff
            _(`pwm_${index}_fsmode`).disabled = onoff
        }

        setDisabled(index, pinMode.value > 9);
        const updateOthers = (value, enable) => {
            if (value > 9) { // disable others
                elrsState.config.pwm.forEach((item, other) => {
                    if (other !== index) {
                        document.querySelectorAll(`#pwm_${other}_mode option`).forEach(opt => {
                            if (opt.value === value) {
                                opt.disabled = enable
                            }
                        })
                    }
                })
            }
        }
        updateOthers(pinMode.value, true) // disable others
        updateOthers(this.pinModes[index], false) // enable others
        this.pinModes[index] = pinMode.value

        // put some constraints on pinRx/Tx mode selects
        if (this.pinRxIndex !== undefined && this.pinTxIndex !== undefined) {
            const pinRxMode = _(`pwm_${this.pinRxIndex}_mode`)
            const pinTxMode = _(`pwm_${this.pinTxIndex}_mode`)
            if (index === this.pinRxIndex) {
                if (pinRxMode.value === '9') { // Serial
                    pinTxMode.value = 9
                    setDisabled(this.pinRxIndex, true)
                    setDisabled(this.pinTxIndex, true)
                    pinTxMode.disabled = true
                }
                else {
                    pinTxMode.value = 0
                    setDisabled(this.pinRxIndex, false)
                    setDisabled(this.pinTxIndex, false)
                    pinTxMode.disabled = false
                }
            }
            if (index === this.pinTxIndex) {
                if (pinTxMode.value === '9') { // Serial
                    pinRxMode.value = 9
                    setDisabled(this.pinRxIndex, true)
                    setDisabled(this.pinTxIndex, true)
                    pinTxMode.disabled = true
                }
            }
            const pinTx = pinTxMode.value
            if (pinRxMode.value !== '9') pinTxMode.value = pinTx
        }

    }

    _failsafeModeChange(failsafeMode, index) {
        const failsafeField = _(`pwm_${index}_fs`)
        if (failsafeMode.value === '0') {
            failsafeField.disabled = false
            failsafeField.style.display = 'block'
        }
        else {
            failsafeField.disabled = true
            failsafeField.style.display = 'none'
        }
    }

    _getPwmFormData() {
        let ch = 0
        let inField
        const outData = []
        while (inField = _(`pwm_${ch}_ch`)) {
            const inChannel = inField.value
            const mode = _(`pwm_${ch}_mode`).value
            const invert = _(`pwm_${ch}_inv`).checked ? 1 : 0
            const narrow = _(`pwm_${ch}_nar`).checked ? 1 : 0
            const failsafeField = _(`pwm_${ch}_fs`)
            const failsafeModeField = _(`pwm_${ch}_fsmode`)
            let failsafe = failsafeField.value
            if (failsafe > 2011) failsafe = 2011
            if (failsafe < 988) failsafe = 988
            failsafeField.value = failsafe
            let failsafeMode = failsafeModeField.value

            const raw = (narrow << 19) | (mode << 15) | (invert << 14) | (inChannel << 10) | (failsafeMode << 20) | (failsafe - 988)
            // console.log(`PWM ${ch} mode=${mode} input=${inChannel} fs=${failsafe} fsmode=${failsafeMode} inv=${invert} nar=${narrow} raw=${raw}`)
            outData.push(raw)
            ++ch
        }
        return outData
    }

    _savePwmConfig(e) {
        e.preventDefault();
        const data = this._getPwmFormData()
        saveConfig({
            ...elrsState.config,
            'pwm': data
        }, () => {
            let i = 0
            elrsState.config.pwm.forEach((item) => {
                item.config = data[i++]
            })
        })
    }
}
