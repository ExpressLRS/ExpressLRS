import {html, LitElement} from "lit";
import {customElement} from "lit/decorators.js";
import {elrsState, formatBand, formatWifiRssi} from "../utils/state.js";
import {SERIAL_OPTIONS1} from '../utils/globals.js'
import '../assets/mui.js';

@customElement('info-panel')
class InfoPanel extends LitElement {
    createRenderRoot() {
        return this
    }

    render() {
        return html`
            <div class="mui-panel mui--text-title">Information</div>
            <div class="mui-panel">
                <table class="mui-table mui-table--bordered">
                    <tbody>
                    <tr><td><b>Product</b></td><td>${elrsState.settings.product_name}</td></tr>
                    <tr><td><b>Lua Name</b></td><td>${elrsState.settings.lua_name}</td></tr>
                    <tr><td><b>Version</b></td><td>${elrsState.settings.version}</td></tr>
                    <tr><td><b>Git Hash</b></td><td>${elrsState.settings['git-commit']}</td></tr>
                    <tr><td><b>Device Type</b></td><td>${elrsState.settings['module-type']}</td></tr>
                    <tr><td><b>Firmware</b></td><td>${elrsState.settings.target}</td></tr>
                    <tr><td><b>Radio</b></td><td>${elrsState.settings['radio-type']}</td></tr>
                    <tr><td><b>Domain</b></td><td>${formatBand()}</td></tr>
                    <tr><td><b>Binding UID</b></td><td>${elrsState.config.uid.toString()}</td></tr>
                    <tr><td><b>WiFi State</b></td><td>${formatWifiRssi()}</td></tr>
                    </tbody>
                </table>
            </div>
            ${this._hasCustomSettings() ? html`
                <div class="mui-panel">
                    <div class="mui--text-title">Custom Settings Detected</div>
                    <br>
                    <table class="mui-table mui-table--bordered">
                        <tbody>
                        ${elrsState.options['is-airport'] ?
                                html`<tr><td><b>Airport Mode</b></td><td>Enabled</td></tr>`
                                : ''}
                        ${elrsState.options['wifi-on-interval'] !== 60 ?
                                html`<tr><td><b>Wifi Auto-on Interval</b></td><td>${elrsState.options['wifi-on-interval']}</td></tr>`
                                : ''}
                        <!-- FEATURE: NOT IS_TX -->
                        ${elrsState.options['lock-on-first-connection'] !== true ?
                                html`<tr><td><b>Lock on First Connection</b></td><td>False</td></tr>`
                                : ''}
                        ${elrsState.config.modelid !== 255 ?
                                html`<tr><td><b>Model Match</b></td><td>Enabled (ID: ${elrsState.config.modelid})</td></tr>`
                                : ''}
                        ${elrsState.config.vbind !== 0 ?
                                html`<tr><td><b>Binding Storage</b></td><td>${elrsState.config.vbind === 1 ? 'Volatile' : elrsState.config.vbind === 2 ? 'Returnable' : 'Administered'}</td></tr>`
                                : ''}
                        ${elrsState.config['force-tlm'] !== false ?
                                html`<tr><td><b>Force Telemetry Off</b></td><td>Enabled</td></tr>`
                                : ''}
                        ${elrsState.config['pwm'] === undefined && elrsState.config['serial-protocol'] !== 0 ?
                                html`<tr><td><b>Serial Protocol</b></td><td>${SERIAL_OPTIONS1[elrsState.config['serial-protocol']]}</td></tr>`
                                : ''}
                        ${elrsState.config['pwm'] === undefined && elrsState.options['rcvr-uart-baud'] !== 420000 ?
                                html`<tr><td><b>Baud Rate</b></td><td>${elrsState.options['rcvr-uart-baud']}</td></tr>`
                                : ''}
                        <!-- /FEATURE: NOT IS_TX -->
                        <!-- FEATURE: IS_TX -->
                        ${elrsState.options['tlm-interval'] !== 240 ?
                                html`<tr><td><b>Telemetry Report Interval (ms)</b></td><td>${elrsState.options['tlm-interval']}</td></tr>`
                                : ''}
                        <!-- /FEATURE: IS_TX -->
                        ${elrsState.settings?.custom_hardware ?
                                html`<tr><td><b>Customised Hardware Settings</b></td><td>True</td></tr>`
                                : ''}

                        </tbody>
                    </table>
                </div>
                `:
                ''
            }
        `
    }

    _hasCustomSettings() {
        let custom = false
        // customised hardware settings
        custom = elrsState.options['is-airport'] || elrsState.options['wifi-on-interval'] !== 60

        // FEATURE: NOT IS_TX
        custom |= elrsState.config['pwm'] === undefined && elrsState.config['serial-protocol'] !== 0
        custom |= elrsState.config['pwm'] === undefined && elrsState.options['rcvr-uart-baud'] !== 420000
        custom |= elrsState.options['lock-on-first-connection'] !== true ||
            elrsState.config.modelid !== 255 ||
            elrsState.config.vbind !== 0 ||
            elrsState.config['force-tlm'] !== false
        // /FEATURE: NOT IS_TX

        // FEATURE: IS_TX
        custom |= elrsState.options['tlm-interval'] !== 240
        // /FEATURE: IS_TX
        return custom
    }
}
