import {html, LitElement} from "lit";
import {customElement} from "lit/decorators.js";
import {elrsState, saveConfig} from "../utils/state.js";
import {_, _renderOptions} from "../utils/libs.js";
import {postJSON} from "../utils/feedback.js";

@customElement('buttons-panel')
class ButtonsPanel extends LitElement {

    colorTimer = undefined;
    colorUpdated = false;
    buttonActions = [];

    createRenderRoot() {
        this._timeoutCurrentColors = this._timeoutCurrentColors.bind(this);
        this._checkEnableButtonActionSave = this._checkEnableButtonActionSave.bind(this);
        return this;
    }

    render() {
        return html`
            <div class="mui-panel mui--text-title">Button & Actions</div>
            <div class="mui-panel">
                <p>
                    Specify which actions to perform when clicking or long pressing module buttons.
                </p>
                <form class="mui-form">
                    ${elrsState.config['button-actions'] ? html`
                        <table class="mui-table">
                            <tbody id="button-actions">
                            ${this._appendButtonActions()}
                            </tbody>
                        </table>
                    ` : ``}
                    ${this.buttonActions[0] && this.buttonActions[0]['color'] !== undefined ? html`
                        <p>
                            <input id='button1-color' type='color' @input="${(e) => this._changeCurrentColors(e, 0)}"
                                   .value="${this._toRGB(this.buttonActions[0]['color'])}"/>
                            <label for="button1-color">User button 1 color</label>
                        </p>
                    ` : ''}
                    ${this.buttonActions[1] && this.buttonActions[1]['color'] !== undefined ? html`
                        <p>
                            <input id='button2-color' type='color' @input="${(e) => this._changeCurrentColors(e, 1)}"
                                   .value="${this._toRGB(this.buttonActions[1]['color'])}"/>
                            <label for="button2-color">User button 2 color</label>
                        </p>
                    ` : ''}
                    <button class="mui-btn mui-btn--primary" @click="${this._submitButtonActions}"
                            ?disabled="${this._checkEnableButtonActionSave()}">Save
                    </button>
                </form>
            </div>
        `;
    }

    _appendButtonActions() {
        let result = []
        this.buttonActions = elrsState.config['button-actions'];
        for (const [b, _v] of Object.entries(this.buttonActions)) {
            for (const [p, v] of Object.entries(_v.action)) {
                result.push(this._appendButtonActionRow(parseInt(b), parseInt(p), v));
            }
        }
        return result
    }

    _appendButtonActionRow(b, p, v) {
        return html`
            <tr>
                <td>
                    Button ${parseInt(b) + 1}
                </td>
                <td>
                    <div class="mui-select">
                        <select @change="${(e) => this._changeAction(b, p, parseInt(e.target.value))}">
                            ${_renderOptions(['Unused', 'Increase Power', 'Go to VTX Band Menu', 'Go to VTX Channel Menu',
                                'Send VTX Settings', 'Start WiFi', 'Enter Binding Mode', 'Start BLE Joystick'], v.action)}
                        </select>
                        <label>Action</label>
                    </div>
                </td>
                <td>
                    <div class="mui-select">
                        <select id="select-press-${b}-${p}"
                                @change="${(e) => this._changePress(b, p, e.target.value)}"
                                ?disabled="${v.action === 0}"
                        >
                            <option value='' disabled hidden ?selected="${v.action === 0}"></option>
                            <option value='false' ?selected="${v['is-long-press'] === false}">Short press (click)
                            </option>
                            <option value='true' ?selected="${v['is-long-press'] === true}">Long press (hold)</option>
                        </select>
                        <label>Press</label>
                    </div>
                </td>
                <td>
                    <div class="mui-select">
                        <select id="select-timing-${b}-${p}"
                                @change="${(e) => this._changeCount(b, p, parseInt(e.target.value))}"
                                ?disabled="${v.action === 0}"
                        >
                            <option value='' disabled hidden ?selected="${v.action === 0}"></option>
                            ${v['is-long-press'] === true
                                    ? _renderOptions([
                                        'for 0.5 seconds', 'for 1 second', 'for 1.5 seconds', 'for 2 seconds',
                                        'for 2.5 seconds', 'for 3 seconds', 'for 3.5 seconds', 'for 4 seconds',
                                    ], v.count)
                                    : _renderOptions([
                                        '1 time', '2 times', '3 times', '4 times',
                                        '5 times', '6 times', '7 times', '8 times',
                                    ], v.count)}
                        </select>
                        <label>Count</label>
                    </div>
                </td>
            </tr>
        `
    }

    _submitButtonActions(e) {
        e.preventDefault();
        saveConfig({'button-actions': this.buttonActions})
    }

    _toRGB(c) {
        let r = c & 0xE0;
        r = ((r << 16) + (r << 13) + (r << 10)) & 0xFF0000;
        let g = c & 0x1C;
        g = ((g << 11) + (g << 8) + (g << 5)) & 0xFF00;
        let b = ((c & 0x3) << 1) + ((c & 0x3) >> 1);
        b = (b << 5) + (b << 2) + (b >> 1);
        return '#' + (r + g + b).toString(16).padStart(6, '0');
    }

    _to8bit(v) {
        v = parseInt(v.substring(1), 16)
        return ((v >> 16) & 0xE0) + ((v >> (8 + 3)) & 0x1C) + ((v >> 6) & 0x3)
    }

    _changeCurrentColors(e, index) {
        this.buttonActions[index].color = this._to8bit(e.target.value);
        if (this.colorTimer === undefined) {
            this._sendCurrentColors();
            this.colorTimer = setInterval(this._timeoutCurrentColors, 50);
        } else {
            this.colorUpdated = true;
        }
    }

    _sendCurrentColors() {
        let colors = [this.buttonActions[0].color];
        if (this.buttonActions[1] && this.buttonActions[1].color !== undefined) colors.push(this.buttonActions[1].color);
        postJSON('/buttons', colors)
        this.colorUpdated = false;
    }

    _timeoutCurrentColors() {
        if (this.colorUpdated) {
            this._sendCurrentColors();
        } else {
            clearInterval(this.colorTimer);
            this.colorTimer = undefined;
        }
    }

    _checkEnableButtonActionSave() {
        for (const [b, _v] of Object.entries(this.buttonActions)) {
            for (const [p, v] of Object.entries(_v.action)) {
                if (v.action !== 0 && (_(`select-press-${b}-${p}`)?.value === '' || _(`select-timing-${b}-${p}`)?.value === '')) {
                    return true;
                }
            }
        }
        return false;
    }

    _changeAction(b, p, value) {
        (this.buttonActions)[b].action[p].action = value;
        if (value === 0) {
            _(`select-press-${b}-${p}`).value = '';
            _(`select-timing-${b}-${p}`).value = '';
        }
        this.requestUpdate()
    }

    _changePress(b, p, value) {
        (this.buttonActions)[b].action[p]['is-long-press'] = (value === 'true');
        this.requestUpdate()
    }

    _changeCount(b, p, value) {
        (this.buttonActions)[b].action[p].count = parseInt(value);
        this.requestUpdate()
    }
}
