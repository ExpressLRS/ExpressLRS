import {html, LitElement} from "lit"
import {customElement, state} from "lit/decorators.js"
import {elrsState, saveOptions} from "../utils/state.js"
import {_renderOptions} from "../utils/libs.js"
import {postWithFeedback} from "../utils/feedback.js"

@customElement('tx-options-panel')
class TxOptionsPanel extends LitElement {
    @state() accessor domain
    @state() accessor isAirport
    @state() accessor baudRate
    @state() accessor tlmInterval
    @state() accessor fanRuntime

    createRenderRoot() {
        this.domain = elrsState.options.domain
        this.isAirport = elrsState.options['is-airport']
        this.baudRate = elrsState.options['airport-uart-baud']
        this.tlmInterval = elrsState.options['tlm-interval']
        this.fanRuntime = elrsState.options['fan-runtime']
        return this
    }

    render() {
        return html`
            <div class="mui-panel mui--text-title">Runtime Options</div>
            <div class="mui-panel">
                <form class="mui-form">
                    <p><b>Override</b> options provided when the firmware was flashed. These changes will
                        persist across reboots, but <b>will be reset</b> when the firmware is reflashed.</p>
                    <!-- FEATURE:HAS_SUBGHZ -->
                    <div class="mui-select">
                        <select @change="${(e) => this.domain = parseInt(e.target.value)}">
                            ${_renderOptions(['AU915', 'FCC915', 'EU868', 'IN866', 'AU433', 'EU433', 'US433', 'US433-Wide'], this.domain)}
                        </select>
                        <label>Regulatory domain</label>
                    </div>
                    <!-- /FEATURE:HAS_SUBGHZ -->
                    <div class="mui-textfield">
                        <input size='5' type='number'
                               @input="${(e) => this.tlmInterval = parseInt(e.target.value)}"
                               value="${this.tlmInterval}">
                            <label>TLM report interval (ms)</label>
                        </input>
                    </div>
                    <div class="mui-textfield">
                        <input size='3' type='number'
                               @input="${(e) => this.fanRuntime = parseInt(e.target.value)}"
                               value="${this.fanRuntime}">
                        <label>Fan runtime (s)</label>
                        </input>
                    </div>
                    <div class="mui-checkbox">
                        <input type='checkbox'
                               @change="${(e) => this.isAirport = e.target.checked}"
                               ?checked="${this.isAirport}">
                        <label>Use as AirPort Serial device</label>
                        </input>
                    </div>
                    ${this.isAirport ? html`
                        <div class="mui-textfield"">
                        <input size='7' type='number'
                               @input="${(e) => this.baudRate = parseInt(e.target.value)}"
                               value="${this.baudRate}">
                        <label>AirPort UART baud</label>
                        </input>
                        </div>
                    ` : ''}

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
            'tlm-interval': this.tlmInterval,
            'fan-runtime': this.fanRuntime,
            'is-airport': this.isAirport,
            'airport-uart-baud': this.baudRate
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
        changed |= this.tlmInterval !== elrsState.options['tlm-interval']
        changed |= this.fanRuntime !== elrsState.options['fan-runtime']
        changed |= this.isAirport !== elrsState.options['is-airport']
        changed |= this.baudRate !== elrsState.options['airport-uart-baud']
        return changed
    }
}
