import {html, LitElement} from "lit"
import {customElement, state} from "lit/decorators.js"
import '../assets/mui.js'
import {_renderOptions} from "../utils/libs.js"
import {elrsState, saveOptions} from "../utils/state.js"
import {postWithFeedback} from "../utils/feedback.js"

@customElement('rx-options-panel')
class RxOptionsPanel extends LitElement {
    @state() accessor domain
    @state() accessor baudRate
    @state() accessor lockOnFirst
    @state() accessor isAirport
    @state() accessor djiArmed
    @state() accessor enableModelMatch
    @state() accessor modelId
    @state() accessor forceTlmOff

    createRenderRoot() {
        this.enableModelMatch = elrsState.options.modelid!==undefined && elrsState.options.modelid !== 255
        this.lockOnFirst = elrsState.options['lock-on-first-connection']
        this.djiArmed = elrsState.options['dji-permanently-armed']
        this.modelId = elrsState.options['modelid']
        this.forceTlmOff = elrsState.options['force-tlm']
        return this
    }

    render() {
        return html`
            <div class="mui-panel mui--text-title">Runtime Options</div>
            <div class="mui-panel">
                <p><b>Override</b> options provided when the firmware was flashed. These changes will
                    persist across reboots, but <b>will be reset</b> when the firmware is reflashed.</p>
                <form id='upload_options' method='POST' action="/options">
                    <!-- FEATURE:HAS_SUBGHZ -->
                    <div class="mui-select">
                        <select @change="${(e) => this.domain = parseInt(e.target.value)}">
                            ${_renderOptions(['AU915','FCC915','EU868','IN866','AU433','EU433','US433','US433-Wide'], this.domain)}
                        </select>
                        <label for="domain">Regulatory domain</label>
                    </div>
                    <!-- /FEATURE:HAS_SUBGHZ -->
                    <div class="mui-checkbox">
                        <input type='checkbox'
                               ?checked="${this.lockOnFirst}"
                               @change="${(e) => {this.lockOnFirst = e.target.checked}}"/>
                        <label>Lock on first connection</label>
                    </div>
                    <div class="mui-checkbox">
                        <input type='checkbox'
                               ?checked="${this.djiArmed}"
                               @change="${(e) => {this.djiArmed = e.target.checked}}"/>
                        <label>Permanently arm DJI air units</label>
                    </div>
                    <h2>Model Match</h2>
                    Specify the 'Receiver' number in OpenTX/EdgeTX model setup page and turn on the 'Model Match'
                    in the ExpressLRS Lua script for that model. 'Model Match' is between 0 and 63 inclusive.
                    <br/>
                    <div class="mui-checkbox">
                        <input type='checkbox'
                               ?checked="${this.enableModelMatch}"
                               @change="${(e) => {this.enableModelMatch = e.target.checked}}"/>
                        <label>Enable Model Match</label>
                    </div>
                    ${this.enableModelMatch ? html`
                    <div class="mui-textfield">
                        <input type='text' required
                               @change="${(e) => this.modelId = parseInt(e.target.value)}"
                               value="${this.modelId}"/>
                        <label>Model ID</label>
                    </div>
                    ` : ''}
                    <h2>Force telemetry off</h2>
                    When running multiple receivers simultaneously from the same TX (to increase the number of PWM servo outputs), there can be at most one receiver with telemetry enabled.
                    <br>Enable this option to ignore the "Telem Ratio" setting on the TX and never send telemetry from this receiver.
                    <br/>
                    <div class="mui-checkbox">
                        <input id='force-tlm' name='force-tlm' type='checkbox' value="1"/>
                        <label for="force-tlm">Force telemetry OFF on this receiver</label>
                    </div>

                    <button class="mui-btn mui-btn--primary"
                            ?disabled="${!this.hasChanges()}"
                            @click="${this.save}"
                    >
                        Save
                    </button>
                    ${elrsState.options.customised ? html`
                        <button class="mui-btn mui-btn--small mui-btn--danger mui--pull-right"
                                @click="${postWithFeedback('Reset Runtime Options', 'An error occurred resetting runtime options', '/reset?options', null)}"
                        >
                            Reset to defaults
                        </button>
                    ` : ''}
                </form>
            </div>
        `
    }

    save(e) {
        e.preventDefault()
        const changes = {
            ...elrsState.options,
            // FEATURE: HAS_SUBGHZ
            'domain': this.domain,
            // /FEATURE: HAS_SUBGHZ
            'rcvr-uart-baud': this.baudRate,
            'lock-on-first-connection': this.lockOnFirst,
            'is-airport': this.isAirport,
            'dji-permanently-armed': this.djiArmed,
            'modelid': this.enableModelMatch ? this.modelId : 255,
            'force-tlm': this.forceTlmOff
        }
        saveOptions(changes, () => {
            elrsState.options = changes
            return this.requestUpdate()
        })
    }

    hasChanges() {
        let changed = false
        // FEATURE: HAS_SUBGHZ
        changed |= this.domain !== elrsState.options['domain']
        // /FEATURE: HAS_SUBGHZ
        changed |= this.baudRate !== elrsState.options['rcvr-uart-baud']
        changed |= this.lockOnFirst !== elrsState.options['lock-on-first-connection']
        changed |= this.isAirport !== elrsState.options['is-airport']
        changed |= this.djiArmed !== elrsState.options['dji-permanently-armed']
        changed |= this.modelId !== elrsState.options['modelid']
        changed |= this.forceTlmOff !== elrsState.options['force-tlm']
        return changed
    }
}
