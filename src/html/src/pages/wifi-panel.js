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

    running = false

    constructor() {
        super()
        this._getNetworks = this._getNetworks.bind(this)
    }

    createRenderRoot() {
        this.wifiOnInterval = elrsState.options['wifi-on-interval'] === undefined ? 60 : elrsState.options['wifi-on-interval']
        return this
    }

    disconnectedCallback() {
        this.running = false
    }

    updated(_) {
        if (!this.running) this._getNetworks()
        this.running = true
    }

    render() {
        return html`
            <div class="mui-panel mui--text-title">WiFi Configuration</div>
            <div class="mui-panel">
                <div class="mui-panel info-bg">
                    ${elrsState.config.mode !== 'STA' ? 'Currently in Access-Point mode' : 'Currently connected to home network: ' + elrsState.config.ssid}
                </div>
                <p>
                    Here you can join a network, and save it as your Home network. When you enable WiFi in range of your
                    Home network, ExpressLRS will automatically connect to it. In Access Point (AP) mode, the network
                    name is ExpressLRS TX or ExpressLRS RX with password "expresslrs".
                </p>
                <form id="sethome" class="mui-form">
                    <div class="mui-radio">
                        <input type="radio" name="networktype" value="0" checked @change="${this._handleChange}">
                        <label>Set new home network</label>
                    </div>
                    <div class="mui-radio">
                        <input type="radio" name="networktype" value="1" @change="${this._handleChange}">
                        <label>Temporarily connect to a network, retain current home network setting</label>
                    </div>
                    <div class="mui-radio">
                        <input type="radio" name="networktype" value="2" @change="${this._handleChange}">
                        <label>Temporarily enable "Access Point" mode, retain current home network setting</label>
                    </div>
                    <div class="mui-radio">
                        <input type="radio" name="networktype" value="3" @change="${this._handleChange}">
                        <label>Forget home network setting, always use "Access Point" mode</label>
                    </div>
                    <br/>
                    <div ?hidden="${this.selectedValue !== '0'}">
                        <div class="mui-textfield">
                            <input size='3' name='wifi-on-interval' type='text' placeholder="Disabled"
                                   @input="${(e) => this.wifiOnInterval = parseInt(e.target.value)}"
                                   value="${this.wifiOnInterval}"
                            />
                            <label for="wifi-on-interval">WiFi "auto on" interval in seconds (leave blank to
                                disable)</label>
                        </div>
                    </div>
                    <div id="credentials" ?hidden="${this.selectedValue === '2' || this.selectedValue === '3'}">
                        <div class="autocomplete mui-textfield" style="position:relative;">
                            <div style="display: ${this.showLoader ? 'block' : 'none'};" class="loader"></div>
                            <input name="network" type="text" placeholder="SSID" autocomplete="off"/>
                            <label for="network">WiFi SSID</label>
                        </div>
                        <div class="mui-textfield">
                            <input size='64' name='password' type='password'/>
                            <label for="password">WiFi password</label>
                        </div>
                    </div>
                    <button class="mui-btn mui-btn--primary" @click="${this._setupNetwork}">Save</button>
                </form>
            </div>
            <div class="mui-panel" ?hidden="${elrsState.config.mode === 'STA'}">
                <a id="connect" href="#"
                   @click="${() => postWithFeedback('Connect to Home Network', 'An error occurred connecting to the Home network', '/connect', null)}">
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
                    }
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
                postWithFeedback('Forget Home Network', 'An error occurred forgetting the home network', '/forget', null)(event)
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
}
