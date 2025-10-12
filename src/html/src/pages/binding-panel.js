import {html, LitElement} from "lit";
import {customElement, query, state} from "lit/decorators.js";
import {elrsState, saveConfig, saveOptions} from "../utils/state.js";
import '../assets/mui.js';
import {calcMD5} from "../utils/md5.js";

@customElement('binding-panel')
class BindingPanel extends LitElement {
    @query('#phrase') accessor phrase

    @state() accessor uid = []
    @state() accessor bindType = "0"
    @state() accessor uidData = {}

    originalUIDType = ''
    originalUID = []

    createRenderRoot() {
        return this;
    }

    firstUpdated(_changedProperties) {
        this.uid = elrsState.config.uid
        this.originalUID = elrsState.config.uid
        this.originalUIDType = (elrsState.config && elrsState.config.uidtype) ? elrsState.config.uidtype : ''
        this._updateUIDType(this.originalUIDType)
    }

    render() {
        return html`
            <div class="mui-panel mui--text-title">Binding</div>
            <div class="mui-panel">
                <form class="mui-form">
                    <!-- FEATURE:IS_TX -->
                    <div class="mui-select">
                        <select @change="${(e) => {this.bindType = e.target.value}}">
                            <option value="0">Persistent (Default) - Bind information is stored across reboots
                            </option>
                            <option value="1">Volatile - Never store bind information across reboots</option>
                            <option value="2">Returnable - Unbinding a receiver reverts to flashed binding phrase
                            </option>
                            <option value="3">Administered - Binding information can only be edited through web UI
                            </option>
                        </select>
                        <label>Binding storage</label>
                    </div>
                    <!-- /FEATURE:IS_TX -->
                    ${this.bindType !== "1" ? html`
                        <div>
                            Enter a new binding phrase to replace the current binding information.
                            This will persist across reboots, but <b>will be reset</b> if the firmware is flashed with a
                            binding phrase.
                            Note: The Binding phrase is not remembered, it is a temporary field used to generate the
                            binding UID.
                            <br/><br/>
                            <div class="mui-textfield">
                                <input type="text" id="phrase" placeholder="Binding Phrase"
                                       @input="${this._updateBindingPhrase}"/>
                                <label for="phrase">Binding Phrase</label>
                            </div>
                        </div>
                        <div class="mui-textfield">
                            ${this.bindType !== "1" ? html`
                                <span class="badge" id="uid-type"
                                      style="background-color: ${this.uidData.bg}; color: ${this.uidData.fg}">${this.uidData.uidtype}</span>
                            ` : ''}
                            <input size='40' type='text' class='array' readonly
                                   value="${this.uid}"/>
                            <label>Binding UID</label>
                        </div>
                    ` : ''}
                    <button class="mui-btn mui-btn--primary"
                            ?disabled=${(this.bindType !== '1') && this.uidData.uidtype !== 'Modified'}
                            @click="${this._submitOptions}">Save
                    </button>
                </form>
            </div>
        `
    }


    _isValidUidByte(s) {
        let f = parseFloat(s)
        return !isNaN(f) && isFinite(s) && Number.isInteger(f) && f >= 0 && f < 256
    }

    _uidBytesFromText(text) {
        // If text is 4-6 numbers separated with [commas]/[spaces] use as a literal UID
        // This is a strict parser to not just extract numbers from text, but only accept if text is only UID bytes
        if (/^[0-9, ]+$/.test(text)) {
            let asArray = text.split(',').filter(this._isValidUidByte).map(Number)
            if (asArray.length >= 4 && asArray.length <= 6) {
                while (asArray.length < 6)
                    asArray.unshift(0)
                return asArray
            }
        }

        const bindingPhraseFull = `-DMY_BINDING_PHRASE="${text}"`
        const bindingPhraseHashed = calcMD5(bindingPhraseFull)
        return [...bindingPhraseHashed.subarray(0, 6)]
    }

    _updateBindingPhrase(e) {
        let text = e.target.value
        if (text.length === 0) {
            this.uid = this.originalUID
            this._updateUIDType(this.originalUIDType)
        } else {
            this.uid = this._uidBytesFromText(text.trim())
            this._updateUIDType('Modified')
        }
    }

    #UID_CONFIG = {
        // --- Specific Named Types ---
        'Flashed': { bg: '#1976D2', fg: 'white', desc: 'The binding UID was generated from a binding phrase set at flash time' },
        'Overridden': { bg: '#689F38', fg: 'black', desc: 'The binding UID has been generated from a binding phrase previously entered into the "binding phrase" field above' },
        'Modified': { bg: '#7c00d5', fg: 'white', desc: 'The binding UID has been modified, but not yet saved' },
        'Volatile': { bg: '#FFA000', fg: 'white', desc: 'The binding UID will be cleared on boot' },
        'Loaned': { bg: '#FFA000', fg: 'white', desc: 'This receiver is on loan and can be returned using Lua or three-plug' },

        // --- Special Case (Fallback 1) ---
        'DEFAULT_NOT_SET': { bg: '#D50000', fg: 'white', uidtype: 'Not set', desc: 'Using autogenerated binding UID' },

        // --- RX Fallbacks (Fallback 2) ---
        'RX_NOT_BOUND': { bg: '#FFA000', fg: 'white', uidtype: 'Not bound', desc: 'This receiver is unbound and will boot to binding mode' },
        'RX_BOUND': { bg: '#1976D2', fg: 'white', uidtype: 'Bound', desc: 'This receiver is bound and will boot waiting for connection' }
    }

    _updateUIDType(uidtype) {
        let config

        if (!uidtype || uidtype.startsWith('Not set')) {
            config = this.#UID_CONFIG.DEFAULT_NOT_SET
        }
        else if (this.#UID_CONFIG[uidtype]) {
            config = this.#UID_CONFIG[uidtype]
        }
        else {
            const configKey = this.uid.toString().endsWith('0,0,0,0') ? 'RX_NOT_BOUND' : 'RX_BOUND'
            config = this.#UID_CONFIG[configKey]
        }

        this.uidData = {
            uidtype: config.uidtype || uidtype,
            bg: config.bg,
            fg: config.fg,
            desc: config.desc
        }
    }

    _submitOptions(e) {
        e.stopPropagation()
        e.preventDefault()

        // FEATURE:IS_TX
        let tx_changes = {
            ...elrsState.options,
            customised: true,
            uid: this.uid
        }
        saveOptions(tx_changes, () => {
            this.originalUID = this.uid
            this.originalUIDType = 'Overridden'
            this.phrase.value = ''
            this._updateUIDType(this.originalUIDType)
            elrsState.options = tx_changes
            return this.requestUpdate()
        })
        // /FEATURE:IS_TX
        // FEATURE:NOT IS_TX
        const rx_changes =  {
            ...elrsState.config,
            uid: this.uid,
            vbind: this.bindType
        }
        saveConfig(rx_changes, () => {
            elrsState.config = rx_changes
            return this.requestUpdate()
        })
        // /FEATURE:NOT IS_TX
    }
}
