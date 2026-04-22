import {html, LitElement, nothing} from 'lit'
import {customElement, state} from 'lit/decorators.js'
import {postWithFeedback, saveJSONWithReboot} from '../utils/feedback.js'
import '../components/filedrag.js'
import HARDWARE_SCHEMA from '../utils/hardware-schema.js'
import {_arrayInput, _floatInput, _intInput, _uintInput} from "../utils/libs.js";

@customElement('hardware-layout')
export class HardwareLayout extends LitElement {

    @state() accessor customised = false
    @state() accessor loadedHardwareJson = null
    @state() accessor currentHardwareJson = '{}'

    static SCHEMA = HARDWARE_SCHEMA

    createRenderRoot() {
        this._onFormEdited = this._onFormEdited.bind(this)
        return this
    }

    render() {
        return html`
            <div class="hardware-layout">
                <div class="mui-panel mui--text-title">Hardware Layout</div>
                <div class="mui-panel">
                    Upload target configuration (remember to press "Save Target Configuration" at the bottom of the page):
                    <p>
                    <file-drop id="filedrag" label="Upload" @file-drop=${this._onFileDrop}>or drop files here</file-drop>
                </div>
                <div class="mui-panel">
                    <div class="mui-panel warning-bg hardware-customised-warning" ?hidden="${!this.customised}">
                        This hardware configuration has been customized. This can be safely ignored if this is a custom hardware
                        build or for testing purposes.<br>
                        You can <a download href="/hardware.json">download</a> the configuration or
                        <a href="/reset?hardware" @click="${postWithFeedback('Hardware Configuration Reset', 'Reset failed', '/reset?hardware')}">reset</a>
                        to pre-configured defaults and reboot.
                    </div>
                    <form id="upload_hardware" class="mui-form"
                          @input=${this._onFormEdited}
                          @change=${this._onFormEdited}>
                        ${this._renderTable()}
                        <br>
                        <input type="button" name="_ignore" value="Save Target Configuration"
                               class="mui-btn mui-btn--primary" @click=${this._submitConfig}
                               ?disabled=${this._isSaveDisabled()} />
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
            case 'float':
                return html`<input id="${row.id}" name="${row.id}" size=${row.size ?? 10} maxlength=${row.size ?? 10} type="text" @keypress="${_floatInput}"/>`
            case 'int':
                return html`<input id="${row.id}" name="${row.id}" size=${row.size ?? 3} maxlength=${row.size ?? 3} type="text" @keypress="${_intInput}"/>`
            case 'uint':
                return html`<input id="${row.id}" name="${row.id}" size=${row.size ?? 3} maxlength=${row.size ?? 3} type="text" @keypress="${_uintInput}"/>`
            case 'array':
                return html`<input id="${row.id}" name="${row.id}" size=${row.size ?? nothing} maxlength=${row.size ?? nothing} type="text" class="array"  @keypress="${_arrayInput}"/>`
        }
    }

    connectedCallback() {
        super.connectedCallback()
        // Add tooltips to icon classes after first paint
        setTimeout(() => this._initTooltips(), 0)
        this._loadData()
    }

    _field(id) {
        return document.getElementById(id)
    }

    _initTooltips() {
        const add = (cls, label) => {
            const images = this.querySelectorAll('.' + cls)
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
                this.loadedHardwareJson = this.currentHardwareJson
            }
        }
        xmlhttp.open('GET', '/hardware.json', true)
        xmlhttp.setRequestHeader('Content-type', 'application/x-www-form-urlencoded')
        xmlhttp.send()
    }

    _onFileDrop(e) {
        const files = e.detail.files
        const form = this._field('upload_hardware')
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
            const el = this._field(key)
            if (el) {
                if (el.type === 'checkbox') {
                    el.checked = !!value
                } else {
                    if (Array.isArray(value)) el.value = value.toString()
                    else el.value = value
                }
            }
        }
        this.currentHardwareJson = this._serializeCurrentConfig()
    }

    _submitConfig() {
        const body = this.currentHardwareJson
        // Use shared helper that prompts for reboot on success
        saveJSONWithReboot('Upload Succeeded', 'Upload Failed', '/hardware.json', {...JSON.parse(body), "customised": true}, () => {
            this.loadedHardwareJson = body
        })
        return false
    }

    _onFormEdited() {
        this.currentHardwareJson = this._serializeCurrentConfig()
    }

    _serializeCurrentConfig() {
        const form = this._field('upload_hardware')
        if (!form) return '{}'
        const formData = new FormData(form)
        return JSON.stringify(Object.fromEntries(formData), (k, v) => {
            if (v === '') return undefined
            const el = this._field(k)
            if (el && el.type === 'checkbox') {
                return v === 'on'
            }
            if (el && el.classList.contains('array')) {
                const arr = v.split(',').map((element) => Number(element))
                return arr.length === 0 ? undefined : arr
            }
            return isNaN(v) ? v : +v
        })
    }

    _isSaveDisabled() {
        if (this.loadedHardwareJson === null) return true
        return this.currentHardwareJson === this.loadedHardwareJson
    }

    checkChanged() {
        return !this._isSaveDisabled()
    }
}
