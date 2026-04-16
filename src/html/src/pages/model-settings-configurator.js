import {html, LitElement} from "lit"
import {customElement, state} from "lit/decorators.js"
import '../assets/mui.js'
import {errorAlert, infoAlert} from '../utils/feedback.js'

@customElement('model-settings-configurator')
class ModelSettingsConfigurator extends LitElement {
    @state() configData = null
    @state() loading = false

    createRenderRoot() {
        return this
    }

    async readConfig() {
        this.loading = true
        try {
            const resp = await fetch('/config')
            if (!resp.ok) throw new Error('Failed to fetch configuration')
            this.configData = await resp.json()
            await infoAlert('Configuration Loaded', 'Configuration data has been loaded successfully')
        } catch (e) {
            await errorAlert('Load Failed', `Failed to load configuration: ${e.message}`)
        } finally {
            this.loading = false
        }
    }

    render() {
        return html`
            <div class="mui-panel mui--text-title">Model Settings</div>
            <div class="mui-panel">
                <p>Read and display all model configuration parameters from the device.</p>
                <div>
                    <button class="mui-btn mui-btn--primary" @click=${this.readConfig} ?disabled=${this.loading}>
                        ${this.loading ? 'Loading...' : 'Read'}
                    </button>
                </div>
            </div>
            ${this.configData ? this._renderConfigData() : ''}
        `
    }

    _renderConfigData() {
        return html`
            <div class="mui-panel">
                <div class="mui--text-title">Configuration Data</div>
                <br/>
                ${this.configData.settings ? this._renderSection('Device Settings', this.configData.settings) : ''}
                ${this.configData.options ? this._renderSection('Firmware Options', this.configData.options) : ''}
                ${this.configData.config ? this._renderSection('Device Configuration', this.configData.config) : ''}
            </div>
        `
    }

    _renderSection(title, data) {
        return html`
            <div style="margin-bottom: 20px;">
                <h3>${title}</h3>
                <table class="mui-table mui-table--bordered">
                    <tbody>
                        ${this._renderTableRows(data)}
                    </tbody>
                </table>
            </div>
        `
    }

    _renderTableRows(obj, prefix = '') {
        const rows = []
        
        if (Array.isArray(obj)) {
            obj.forEach((item, index) => {
                if (typeof item === 'object' && item !== null) {
                    rows.push(...this._renderTableRows(item, `[${index}]`))
                } else {
                    rows.push(html`<tr><td><b>${prefix}[${index}]</b></td><td>${this._formatValue(item)}</td></tr>`)
                }
            })
        } else if (typeof obj === 'object' && obj !== null) {
            Object.entries(obj).forEach(([key, value]) => {
                const fullKey = prefix ? `${prefix}.${key}` : key
                
                if (Array.isArray(value)) {
                    rows.push(html`
                        <tr>
                            <td colspan="2">
                                <b>${fullKey}</b> (Array)
                                <table class="mui-table mui-table--bordered" style="margin-top: 10px;">
                                    <tbody>
                                        ${value.map((item, index) => {
                                            if (typeof item === 'object' && item !== null) {
                                                return html`
                                                    <tr>
                                                        <td colspan="2">
                                                            <b>[${index}]</b>
                                                            <table class="mui-table mui-table--bordered" style="margin-top: 5px;">
                                                                <tbody>
                                                                    ${this._renderTableRows(item)}
                                                                </tbody>
                                                            </table>
                                                        </td>
                                                    </tr>
                                                `
                                            } else {
                                                return html`<tr><td style="padding-left: 20px;">[${index}]</td><td>${this._formatValue(item)}</td></tr>`
                                            }
                                        })}
                                    </tbody>
                                </table>
                            </td>
                        </tr>
                    `)
                } else if (typeof value === 'object' && value !== null) {
                    rows.push(html`
                        <tr>
                            <td colspan="2">
                                <b>${fullKey}</b> (Object)
                                <table class="mui-table mui-table--bordered" style="margin-top: 10px;">
                                    <tbody>
                                        ${this._renderTableRows(value, fullKey)}
                                    </tbody>
                                </table>
                            </td>
                        </tr>
                    `)
                } else {
                    rows.push(html`<tr><td><b>${key}</b></td><td>${this._formatValue(value)}</td></tr>`)
                }
            })
        }
        
        return rows
    }

    _formatValue(value) {
        if (value === null || value === undefined) {
            return html`<span style="color: #999;">null</span>`
        }
        if (typeof value === 'boolean') {
            return value ? html`<span style="color: green;">true</span>` : html`<span style="color: red;">false</span>`
        }
        if (typeof value === 'number') {
            return html`<span style="color: #0066cc;">${value}</span>`
        }
        return String(value)
    }
}
