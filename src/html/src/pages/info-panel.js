import {html, LitElement} from "lit";
import {customElement} from "lit/decorators.js";
import {elrsState} from "../utils/state.js";
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
                    <tr><td><b>Product</b></td><td>${elrsState.target.product_name}</td></tr>
                    <tr><td><b>Lua Name</b></td><td>${elrsState.target.lua_name}</td></tr>
                    <tr><td><b>Version</b></td><td>${elrsState.target.version}</td></tr>
                    <tr><td><b>Binding UID</b></td><td>${elrsState.config.uid.toString()}</td></tr>
                    <!-- FEATURE: NOT IS_TX -->
                    <tr><td><b>Model Match</b></td><td>${elrsState.config.modelid === 255 ? 'Disabled' : `Enabled (ID: ${elrsState.config.modelid})` }</td></tr>
                    <!-- /FEATURE: NOT IS_TX -->
                    <tr><td><b>Domain</b></td><td>${elrsState.target.reg_domain}</td></tr>
                    <tr><td><b>Device Type</b></td><td>${elrsState.target['module-type']}</td></tr>
                    <tr><td><b>Radio</b></td><td>${elrsState.target['radio-type']}</td></tr>
                    <tr><td><b>Firmware</b></td><td>${elrsState.target.target}</td></tr>
                    <tr><td><b>Git Hash</b></td><td>${elrsState.target['git-commit']}</td></tr>
                    </tbody>
                </table>
            </div>
        `
    }
}
