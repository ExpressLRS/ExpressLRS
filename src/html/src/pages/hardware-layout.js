import {html, LitElement, nothing} from 'lit'
import {customElement, state} from 'lit/decorators.js'
import '../assets/mui.js'
import {postWithFeedback, saveJSONWithReboot} from '../utils/feedback.js'
import '../components/filedrag.js'
import HARDWARE_SCHEMA from '../utils/hardware-schema.js'

@customElement('hardware-layout')
export class HardwareLayout extends LitElement {

    @state() accessor customised = false

    static SCHEMA = HARDWARE_SCHEMA

    createRenderRoot() {
        return this
    }

    render() {
        return html`
            <div class="hardware-layout">
                <div class="mui-panel mui--text-title">Hardware Layout</div>
                <div class="mui-panel">
                    <label>Upload target configuration (remember to press "Save Target Configuration" below):</label>
                    <file-drop id="filedrag" label="Upload" @file-drop=${this._onFileDrop}>or drop files here</file-drop>
                </div>
                <div class="mui-panel">
                    <div class="mui-panel"
                         style="display:${this.customised ? 'block' : 'none'}; background-color: #FFC107;">
                        This hardware configuration has been customized. This can be safely ignored if this is a custom hardware
                        build or for testing purposes.<br>
                        You can <a download href="/hardware.json">download</a> the configuration or
                        <a href="/reset?hardware" @click="${postWithFeedback('Hardware Configuration Reset', 'Reset failed', '/reset?hardware')}">reset</a>
                        to pre-configured defaults and reboot.
                    </div>
                    <form class="mui-form">
                        <input type="hidden" id="customised" name="customised" value="true"/>
                        ${this._renderTable()}
                        <br>
                        <input type="button" value="Save Target Configuration"
                               class="mui-btn mui-btn--primary" @click=${this._submitConfig}/>
                    </form>
                </div>
            </div>
        `
    }

    _renderTable() {
        return html`
            <table>
                <tbody>
                ${this.constructor.SCHEMA.map(section => html`
                    <tr>
                        <td colspan="4"><b>${section.title}</b></td>
                    </tr>
                    ${section.rows.map(row => html`
                        <tr>
                            <td width="30"></td>
                            <td>${row.label}${this._renderIcon(row.icon)}</td>
                            <td>${this._renderField(row)}</td>
                            <td>${row.desc || ''}</td>
                        </tr>
                    `)}
                `)}
                </tbody>
            </table>
        `
    }

    _renderIcon(icon) {
        if (!icon) return html``
        if (icon === 'input-output') {
            return html`<img class="icon-input"/><img class="icon-output"/>`
        }
        return html`<img class="icon-${icon}"/>`
    }

    _renderField(row) {
        switch (row.type) {
            case 'checkbox':
                return html`<input id="${row.id}" name="${row.id}" type="checkbox"/>`
            case 'select':
                return html`<select id="${row.id}" name="${row.id}">
                    ${row.options?.map(opt => html`
                        <option value="${opt.value}">${opt.label}</option>`)}
                </select>`
            case 'text':
            default:
                const cls = row.className ? row.className : ''
                return html`<input size=${row.size ?? nothing} id="${row.id}" name="${row.id}" type="text" class="${cls}"/>`
        }
    }

    connectedCallback() {
        super.connectedCallback()
        // Add tooltips to icon classes after first paint
        setTimeout(() => this._initTooltips(), 0)
        this._loadData()
    }

    _initTooltips() {
        const add = (cls, label) => {
            const images = document.querySelectorAll('.' + cls)
            images.forEach(i => i.setAttribute('title', label))
        }
        add('icon-input', 'Digital Input')
        add('icon-output', 'Digital Output')
        add('icon-analog', 'Analog Input')
        add('icon-pwm', 'PWM Output')
    }

    _loadData() {
        const xmlhttp = new XMLHttpRequest()
        xmlhttp.onreadystatechange = () => {
            if (xmlhttp.readyState === 4 && xmlhttp.status === 200) {
                const data = JSON.parse(xmlhttp.responseText)
                this.customised = !!data.customised
                this._updateHardwareSettings(data)
            }
        }
        xmlhttp.open('GET', '/hardware.json', true)
        xmlhttp.setRequestHeader('Content-type', 'application/x-www-form-urlencoded')
        xmlhttp.send()
    }

    _onFileDrop(e) {
        const files = e.detail.files
        const form = document.getElementById('upload_hardware')
        if (form) form.reset()
        for (const file of files) {
            const reader = new FileReader()
            reader.onload = (ev) => {
                const data = JSON.parse(ev.target.result)
                this._updateHardwareSettings(data)
            }
            reader.readAsText(file)
        }
    }

    _updateHardwareSettings(data) {
        for (const [key, value] of Object.entries(data)) {
            const el = document.getElementById(key)
            if (el) {
                if (el.type === 'checkbox') {
                    el.checked = !!value
                } else {
                    if (Array.isArray(value)) el.value = value.toString()
                    else el.value = value
                }
            }
        }
    }

    _submitConfig() {
        const form = document.getElementById('upload_hardware')
        const formData = new FormData(form)
        // rebuild using original serializer logic
        const body = JSON.stringify(Object.fromEntries(formData), (k, v) => {
            if (v === '') return undefined
            const el = document.getElementById(k)
            if (el && el.type === 'checkbox') {
                return v === 'on'
            }
            if (el && el.classList.contains('array')) {
                const arr = v.split(',').map((element) => Number(element))
                return arr.length === 0 ? undefined : arr
            }
            return isNaN(v) ? v : +v
        })
        // Use shared helper that prompts for reboot on success
        saveJSONWithReboot('Upload Succeeded', 'Upload Failed', '/hardware.json', JSON.parse(body))
        return false
    }
}
