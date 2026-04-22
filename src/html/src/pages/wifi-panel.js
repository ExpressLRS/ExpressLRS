import {html, LitElement} from "lit"
import {customElement, query, state} from "lit/decorators.js"
import {elrsState} from "../utils/state.js"
import {postWithFeedback} from "../utils/feedback.js"
import {autocomplete} from "../utils/autocomplete.js"

@customElement('wifi-panel')
class WifiPanel extends LitElement {

    @query('#sethome') accessor form
    @query('[name="network"]') accessor network
    @query('[name="password"]') accessor password

    @state() accessor selectedValue = '0'
    @state() accessor showLoader = true
    @state() accessor wifiOnInterval
    @state() accessor passwordVisible = false

    running = false

    constructor() {
        super()
        this._getNetworks = this._getNetworks.bind(this)
    }

    createRenderRoot() {
        this.wifiOnInterval = elrsState.options['wifi-on-interval'] === undefined ? 60 : elrsState.options['wifi-on-interval']
        return this
    }

    _parseWifiOnInterval(value) {
        if (value === '') return undefined
        const parsed = Number.parseInt(value, 10)
        return Number.isNaN(parsed) ? undefined : parsed
    }

    disconnectedCallback() {
        this.running = false
    }

    updated(_) {
        if (!this.running) this._getNetworks()
        this.running = true
    }

    _togglePasswordVisibility() {
        this.passwordVisible = !this.passwordVisible
    }

    render() {
        return html`
            <div class="mui-panel mui--text-title">WiFi Configuration</div>
            <div class="mui-panel">
                <div class="mui-panel info-bg">
                    ${elrsState.settings.mode !== 'STA' ? 'Currently in Access-Point mode' : 'Currently connected to home network: ' + elrsState.settings.ssid}
                </div>
                <p>
                    Here you can join a network, and save it as your Home network. When you enable WiFi in range of your
                    Home network, ExpressLRS will automatically connect to it. In Access Point (AP) mode, the network
                    name is ExpressLRS TX or ExpressLRS RX with password "expresslrs".
                </p>
                <form id="sethome" class="mui-form">
                    <div class="mui-radio">
                        <input id="home" type="radio" name="networktype" value="0" checked @change="${this._handleChange}">
                        <label for="home">Set new home network</label>
                    </div>
                    <div class="mui-radio">
                        <input id="temp" type="radio" name="networktype" value="1" @change="${this._handleChange}">
                        <label for="temp">Temporarily connect to a network, retain current home network setting</label>
                    </div>
                    <div class="mui-radio">
                        <input id="ap" type="radio" name="networktype" value="2" @change="${this._handleChange}">
                        <label for="ap">Temporarily enable "Access Point" mode, retain current home network setting</label>
                    </div>
                    <div class="mui-radio">
                        <input id="forget" type="radio" name="networktype" value="3" @change="${this._handleChange}">
                        <label for="forget">Forget home network setting, always use "Access Point" mode</label>
                    </div>
                    <br/>
                    <div ?hidden="${this.selectedValue !== '0' && this.selectedValue !== '3'}">
                        <div class="mui-textfield">
                            <input id="interval" size='3' name='wifi-on-interval' type='number' placeholder="Disabled"
                                   @input="${(e) => this.wifiOnInterval = this._parseWifiOnInterval(e.target.value)}"
                                   .value="${this.wifiOnInterval?.toString() ?? ''}"
                            />
                            <label for="interval">WiFi "auto on" interval in seconds (leave blank to disable)</label>
                        </div>
                    </div>
                    <div id="credentials" ?hidden="${this.selectedValue === '2' || this.selectedValue === '3'}">
                        <div class="autocomplete mui-textfield" style="position:relative;">
                            <div style="display: ${this.showLoader ? 'block' : 'none'};" class="loader"></div>
                            <input id="ssid" name="network" type="text" placeholder="SSID" autocomplete="off"
                                .value="${elrsState.options['wifi-ssid']}"
                            />
                            <label for="ssid">WiFi SSID</label>
                        </div>
                        <div class="mui-textfield">
                            <input id="pwd" size='64' name='password' type=${this.passwordVisible ? 'text' : 'password'}
                                .value="${elrsState.options['wifi-password']}"
                            />
                            <label for="pwd">WiFi password</label>
                            <span
                                @click=${this._togglePasswordVisibility}
                                style="position:absolute; right:1px; top:50%;">
                                ${this.passwordVisible ?
                                    html` <!-- eye open -->
                                        <svg xmlns="http://www.w3.org/2000/svg" width="16" height="16" fill="currentColor" class="bi bi-eye" viewBox="0 0 16 16">
                                        <path d="M16 8s-3-5.5-8-5.5S0 8 0 8s3 5.5 8 5.5S16 8 16 8M1.173 8a13 13 0 0 1 1.66-2.043C4.12 4.668 5.88 3.5 8 3.5s3.879 1.168 5.168 2.457A13 13 0 0 1 14.828 8q-.086.13-.195.288c-.335.48-.83 1.12-1.465 1.755C11.879 11.332 10.119 12.5 8 12.5s-3.879-1.168-5.168-2.457A13 13 0 0 1 1.172 8z"/>
                                        <path d="M8 5.5a2.5 2.5 0 1 0 0 5 2.5 2.5 0 0 0 0-5M4.5 8a3.5 3.5 0 1 1 7 0 3.5 3.5 0 0 1-7 0"/>
                                        </svg>`
                                    : html` <!-- eye closed -->
                                        <svg xmlns="http://www.w3.org/2000/svg" width="16" height="16" fill="currentColor" class="bi bi-eye-slash-fill" viewBox="0 0 16 16">
                                        <path d="m10.79 12.912-1.614-1.615a3.5 3.5 0 0 1-4.474-4.474l-2.06-2.06C.938 6.278 0 8 0 8s3 5.5 8 5.5a7 7 0 0 0 2.79-.588M5.21 3.088A7 7 0 0 1 8 2.5c5 0 8 5.5 8 5.5s-.939 1.721-2.641 3.238l-2.062-2.062a3.5 3.5 0 0 0-4.474-4.474z"/>
                                        <path d="M5.525 7.646a2.5 2.5 0 0 0 2.829 2.829zm4.95.708-2.829-2.83a2.5 2.5 0 0 1 2.829 2.829zm3.171 6-12-12 .708-.708 12 12z"/>
                                        </svg>`
                                }
                            </span>
                        </div>
                    </div>
                    <button class="mui-btn mui-btn--primary" @click="${this._setupNetwork}" ?disabled="${!(this.checkChanged() || this.selectedValue!=='0')}">Save</button>
                </form>
            </div>
            <div class="mui-panel" ?hidden="${elrsState.settings.mode === 'STA'}">
                <a id="connect" href="#"
                   @click="${postWithFeedback('Connect to Home Network', 'An error occurred connecting to the Home network', '/connect', null)}">
                    Connect to Home network: ${elrsState.options['wifi-ssid']}
                </a>
            </div>
        `
    }

    _handleChange(event) {
        this.selectedValue = event.target.value
    }

    _setupNetwork(event) {
        event.preventDefault()
        const self = this
        switch (this.selectedValue) {
            case '0':
                postWithFeedback('Set Home Network', 'An error occurred setting the home network', '/sethome?save', function () {
                    return new FormData(self.form)
                }, function () {
                    elrsState.options = {
                        ...elrsState.options,
                        'wifi-ssid': self.network.value,
                        'wifi-password': self.password.value,
                        'wifi-on-interval': self.wifiOnInterval,
                        customised: true
                    }
                    self.requestUpdate()
                })(event)
                break
            case '1':
                postWithFeedback('Connect To Network', 'An error occurred connecting to the network', '/sethome', function () {
                    return new FormData(self.form)
                })(event)
                break
            case '2':
                postWithFeedback('Start Access Point', 'An error occurred starting the Access Point', '/access', null)(event)
                break
            case '3':
                postWithFeedback('Forget Home Network', 'An error occurred forgetting the home network', '/forget', function () {
                    return new FormData(self.form)
                }, function () {
                    elrsState.options = {
                        ...elrsState.options,
                        'wifi-on-interval': self.wifiOnInterval,
                        customised: true
                    }
                    self.requestUpdate()
                })(event)
                break
        }
    }

    _getNetworks() {
        const self = this
        const xmlhttp = new XMLHttpRequest()
        xmlhttp.onload = function () {
            if (self.running) {
                if (this.status === 204) {
                    setTimeout(self._getNetworks, 2000)
                } else {
                    const data = JSON.parse(this.responseText)
                    if (data.length > 0) {
                        self.showLoader = false
                        autocomplete(self.network, data)
                    }
                }
            }
        }
        xmlhttp.onerror = function () {
            if (self.running) {
                setTimeout(self._getNetworks, 2000)
            }
        }
        xmlhttp.open('GET', 'networks.json', true)
        xmlhttp.send()
    }

    checkChanged() {
        let changed = false
        const currentNetwork = this.network?.value ?? elrsState.options['wifi-ssid']
        const currentPassword = this.password?.value ?? elrsState.options['wifi-password']
        changed |= this.wifiOnInterval !== elrsState.options['wifi-on-interval']
        changed |= currentNetwork !== elrsState.options['wifi-ssid']
        changed |= currentPassword !== elrsState.options['wifi-password']
        return !!changed
    }
}
