import {html, LitElement} from "lit"
import {customElement, state} from "lit/decorators.js"
import '../assets/mui.js'
import {_renderOptions, _uintInput} from "../utils/libs.js"
import {elrsState, saveOptionsAndConfig} from "../utils/state.js"
import {postWithFeedback} from "../utils/feedback.js"

@customElement('rx-options-panel')
class RxOptionsPanel extends LitElement {
    @state() accessor domain
    @state() accessor enableModelMatch
    @state() accessor lockOnFirst
    @state() accessor modelId
    @state() accessor forceTlmOff

    createRenderRoot() {
        this.domain = elrsState.options.domain
        this.lockOnFirst = elrsState.options['lock-on-first-connection']
        this.enableModelMatch = elrsState.config.modelid!==undefined && elrsState.config.modelid !== 255
        this.modelId = elrsState.config.modelid===undefined ? 0 : elrsState.config.modelid
        this.forceTlmOff = elrsState.config['force-tlm']
        this.save = this.save.bind(this)
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
                        <select id="domain" @change="${(e) => this.domain = parseInt(e.target.value)}">
                            ${_renderOptions(['AU915','FCC915','EU868','IN866','AU433','EU433','US433','US433-Wide'], this.domain)}
                        </select>
                        <label for="domain">Regulatory domain</label>
                    </div>
                    <!-- /FEATURE:HAS_SUBGHZ -->
                    <div class="mui-checkbox">
                        <input id="lock" type='checkbox'
                               ?checked="${this.lockOnFirst}"
                               @change="${(e) => {this.lockOnFirst = e.target.checked}}"/>
                        <label for="lock">Lock on first connection</label>
                    </div>
                    <h2>Model Match</h2>
                    Model Match is used to prevent accidentally selecting the wrong model in the handset and flying with an
                    unexpected handset or ELRS configuration. When enabled, Model Match restricts this receiver to only
                    connect fully with the specific Receiver ID below. Set the transmitter's Receiver ID in EdgeTX's model
                    settings, and enable Model Match in the ExpressLRS lua.
                    <br/>
                    <div class="mui-checkbox">
                        <input id="modelMatch" type='checkbox'
                               ?checked="${this.enableModelMatch}"
                               @change="${(e) => {this.enableModelMatch = e.target.checked}}"/>
                        <label for="modelMatch">Enable Model Match</label>
                    </div>
                    ${this.enableModelMatch ? html`
                    <div class="mui-textfield">
                        <input id="modelId" min="0" max="63" type='number' required
                               @change="${(e) => this.modelId = parseInt(e.target.value)}"
                               .value="${this.modelId}"
                               @keypress="${_uintInput}"/>
                        <label for="modelId">Receiver ID (0 - 63)</label>
                    </div>
                    ` : ''}
                    <h2>Force telemetry off</h2>
                    When running multiple receivers simultaneously from the same TX (to increase the number of PWM servo outputs), there can be at most one receiver with telemetry enabled.
                    <br>Enable this option to ignore the "Telem Ratio" setting on the TX and never send telemetry from this receiver.
                    <br/>
                    <div class="mui-checkbox">
                        <input id='force-tlm' name='force-tlm' type='checkbox'
                               ?checked="${this.forceTlmOff}"
                               @change="${(e) => this.forceTlmOff = e.target.checked}"
                        />
                        <label for="force-tlm">Force telemetry OFF on this receiver</label>
                    </div>

                    <button class="mui-btn mui-btn--primary"
                            ?disabled="${!this.checkChanged()}"
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
            options: {
                // FEATURE: HAS_SUBGHZ
                'domain': this.domain,
                // /FEATURE: HAS_SUBGHZ
                'lock-on-first-connection': this.lockOnFirst,
            },
            config: {
                'modelid': this.enableModelMatch ? this.modelId : 255,
                'force-tlm': this.forceTlmOff
            }
        }
        saveOptionsAndConfig(changes, () => {
            this.modelId = changes.config.modelid
            return this.requestUpdate()
        })
    }

    checkChanged() {
        let changed = false
        // FEATURE: HAS_SUBGHZ
        changed |= this.domain !== elrsState.options['domain']
        // /FEATURE: HAS_SUBGHZ
        changed |= this.lockOnFirst !== elrsState.options['lock-on-first-connection']
        changed |= this.enableModelMatch && this.modelId !== elrsState.config['modelid']
        changed |= !this.enableModelMatch && this.modelId !== 255
        changed |= this.forceTlmOff !== elrsState.config['force-tlm']
        return !!changed
    }
}
