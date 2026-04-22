import {html, LitElement} from "lit";
import {customElement} from "lit/decorators.js";
import {elrsState, saveConfig} from "../utils/state.js";
import {_renderOptions} from "../utils/libs.js";
import {postJSON} from "../utils/feedback.js";

const ACTION_OPTIONS = ['Unused', 'Increase Power', 'Go to VTX Band Menu', 'Go to VTX Channel Menu',
    'Send VTX Settings', 'Start WiFi', 'Send Bind Command', 'Start BLE Joystick'];
const LONG_PRESS_OPTIONS = [
    'for 0.5 seconds', 'for 1 second', 'for 1.5 seconds', 'for 2 seconds',
    'for 2.5 seconds', 'for 3 seconds', 'for 3.5 seconds', 'for 4 seconds',
];
const COUNT_OPTIONS = [
    '1 time', '2 times', '3 times', '4 times',
    '5 times', '6 times', '7 times', '8 times',
];

@customElement('buttons-panel')
class ButtonsPanel extends LitElement {

    colorTimer = undefined;
    colorUpdated = false;
    buttonActions = [];
    loadedButtonActionsJson = '[]';
    currentButtonActionsJson = '[]';
    buttonActionsInitialized = false;

    createRenderRoot() {
        this._timeoutCurrentColors = this._timeoutCurrentColors.bind(this);
        return this;
    }

    render() {
        this._initializeButtonActions();
        return html`
            <div class="mui-panel mui--text-title">Button & Actions</div>
            <div class="mui-panel">
                <p>
                    Specify which actions to perform when clicking or long pressing module buttons.
                </p>
                <form class="mui-form">
                    ${this.buttonActions.length ? html`
                        <table class="mui-table">
                            <tbody id="button-actions">
                            ${this.buttonActions.map((button, b) =>
                                    button.action.map((v, p) => this._appendButtonActionRow(b, p, v))
                            )}
                            </tbody>
                        </table>
                    ` : ``}
                    ${this._renderColorInput(0, 'User button 1 color')}
                    ${this._renderColorInput(1, 'User button 2 color')}
                    <button class="mui-btn mui-btn--primary" @click="${this._submitButtonActions}"
                            ?disabled=${this._isSaveDisabled()}>Save
                    </button>
                </form>
            </div>
        `;
    }

    _renderColorInput(index, label) {
        const color = this.buttonActions[index]?.color;
        if (color === undefined) return '';
        return html`
            <p>
                <input id="button${index + 1}-color" type="color" @input="${(e) => this._changeCurrentColors(e, index)}"
                       .value="${this._toRGB(color)}"/>
                <label for="button${index + 1}-color">${label}</label>
            </p>
        `;
    }

    _appendButtonActionRow(b, p, v) {
        return html`
            <tr>
                <td>
                    Button ${b + 1}
                </td>
                <td>
                    <div class="mui-select">
                        <select @change="${(e) => this._changeAction(b, p, parseInt(e.target.value))}">
                            ${_renderOptions(ACTION_OPTIONS, v.action)}
                        </select>
                        <label>Action</label>
                    </div>
                </td>
                <td>
                    <div class="mui-select">
                        <select @change="${(e) => this._changePress(b, p, e.target.value)}"
                                ?disabled=${v.action === 0}
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
                        <select @change="${(e) => this._changeCount(b, p, Number(e.target.value))}"
                                ?disabled=${v.action === 0}
                        >
                            <option value='' disabled hidden ?selected="${v.action === 0}"></option>
                            ${v['is-long-press'] === true
                                    ? _renderOptions(LONG_PRESS_OPTIONS, v.count)
                                    : _renderOptions(COUNT_OPTIONS, v.count)}
                        </select>
                        <label>Count</label>
                    </div>
                </td>
            </tr>
        `
    }

    _submitButtonActions(e) {
        e.preventDefault();
        saveConfig({'button-actions': this.buttonActions}, () => {
            this._refreshButtonActionsJson();
            this.loadedButtonActionsJson = this.currentButtonActionsJson;
            this.requestUpdate();
        })
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

    _isSaveDisabled() {
        return this.currentButtonActionsJson === this.loadedButtonActionsJson;
    }

    checkChanged() {
        return !this._isSaveDisabled()
    }

    _initializeButtonActions() {
        if (this.buttonActionsInitialized || !elrsState.config['button-actions']) {
            return;
        }
        this.buttonActions = this._cloneButtonActions(elrsState.config['button-actions'] ?? []);
        this._refreshButtonActionsJson();
        this.loadedButtonActionsJson = this.currentButtonActionsJson;
        this.buttonActionsInitialized = true;
    }

    _cloneButtonActions(buttonActions) {
        return JSON.parse(JSON.stringify(buttonActions));
    }

    _refreshButtonActionsJson() {
        this.currentButtonActionsJson = JSON.stringify(this.buttonActions);
    }

    _changeAction(b, p, value) {
        const actionConfig = this.buttonActions[b].action[p];
        actionConfig.action = value;
        if (value === 0) {
            actionConfig['is-long-press'] = undefined;
            actionConfig.count = undefined;
        } else {
            if (actionConfig['is-long-press'] !== true && actionConfig['is-long-press'] !== false) {
                actionConfig['is-long-press'] = false; // default to short press
            }
            if (!Number.isInteger(actionConfig.count)) {
                actionConfig.count = 0; // default to first count option
            }
        }
        this._refreshButtonActionsJson();
        this.requestUpdate()
    }

    _changePress(b, p, value) {
        this.buttonActions[b].action[p]['is-long-press'] = (value === 'true');
        this._refreshButtonActionsJson();
        this.requestUpdate()
    }

    _changeCount(b, p, value) {
        this.buttonActions[b].action[p].count = value;
        this._refreshButtonActionsJson();
        this.requestUpdate()
    }
}
