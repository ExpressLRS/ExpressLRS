import {html, LitElement} from 'lit';
import {customElement, query, state} from 'lit/decorators.js';
import '../assets/mui.js'
import '../components/filedrag.js'
import {cuteAlert, postWithFeedback} from "../utils/feedback.js";

@customElement('lr1121-updater')
export class LR1121Updater extends LitElement {
    @query('#radio2') accessor radio2;

    @state() accessor data = undefined;
    @state() accessor status = '';
    @state() accessor progress = 0;
    @state() accessor manual = false;

    createRenderRoot() {
        return this;
    }

    render() {
        return html`
            <div class="mui-panel mui--text-title">LR1121 Firmware Flashing</div>
            <div class="mui-panel" style="display: ${this.manual ? 'block' : 'none'}; background-color: #FFC107;">
                LR1121 firmware has been manually flashed before, to revert to the ExpressLRS provided version you can
                click the button below.<br>
                <button class="mui-btn mui-btn--small" @click=${this._reset}>Reset and reboot</button>
            </div>
            <div class="mui-panel">
                ${this._renderRadios()}
                <label>Upload LR1121 firmware binary:</label>
                <file-drop label="Upload" @file-drop=${this._fileSelected}>or drop firmware file here</file-drop>
                <br/>
                <h3>${this.status}</h3>
                <progress value="${this.progress}" max="100" style="width:100%;"></progress>
            </div>
            <div class="mui-panel">
                ${this._renderInfoTable()}
            </div>
        `
    }

    _renderRadios() {
        if (!this.data?.radio2) return html``;
        return html`
            <div class="mui-radio">
                <label>
                    <input type="radio" name="optionsRadio" value="1" checked>
                    Update Radio 1
                </label>
            </div>
            <div class="mui-radio">
                <label>
                    <input id="radio2" type="radio" name="optionsRadio" value="2">
                    Update Radio 2
                </label>
            </div>
        `;
    }

    _renderInfoTable() {
        if (!this.data) return html``;
        const r1 = this.data.radio1;
        const r2 = this.data.radio2;
        return html`
            <table class="mui-table mui-table--bordered">
                <thead>
                <tr>
                    <th>Parameter</th>
                    <th>Radio 1</th>
                    <th>Radio 2</th>
                </tr>
                </thead>
                <tbody>
                <tr>
                    <td>Type</td>
                    <td><span>${this._dec2hex(r1?.type, 2)}</span></td>
                    <td><span>${r2 ? this._dec2hex(r2.type, 2) : ''}</span></td>
                </tr>
                <tr>
                    <td>Hardware</td>
                    <td><span>${this._dec2hex(r1?.hardware, 2)}</span></td>
                    <td><span>${r2 ? this._dec2hex(r2.hardware, 2) : ''}</span></td>
                </tr>
                <tr>
                    <td>Firmware</td>
                    <td><span>${this._dec2hex(r1?.firmware, 4)}</span></td>
                    <td><span>${r2 ? this._dec2hex(r2.firmware, 4) : ''}</span></td>
                </tr>
                </tbody>
            </table>
        `;
    }

    connectedCallback() {
        super.connectedCallback();
        this._loadData();
    }

    _dec2hex(i, len) {
        if (i === undefined || i === null) return '';
        return "0x" + (i + 0x10000).toString(16).substr(-len).toUpperCase();
    }

    _reset(e) {
        e.preventDefault();
        e.stopPropagation();
        return postWithFeedback('LR1121 Reset', 'Reset failed', '/reset?lr1121', null)(e);
    }

    _loadData() {
        const xmlhttp = new XMLHttpRequest();
        xmlhttp.onreadystatechange = () => {
            if (xmlhttp.readyState === 4 && xmlhttp.status === 200) {
                const data = JSON.parse(xmlhttp.responseText);
                this.data = data;
                this.manual = !!data.manual;
            }
        };
        xmlhttp.open('GET', '/lr1121.json', true);
        xmlhttp.setRequestHeader('Content-type', 'application/x-www-form-urlencoded');
        xmlhttp.send();
    }

    _fileSelected(e) {
        const files = e.detail.files;
        if (files && files[0]) this._uploadFile(files[0]);
    }

    _uploadFile(file) {
        try {
            // Show overlay
            mui.overlay('on', {keyboard: false, static: true});
            const ajax = new XMLHttpRequest();
            ajax.upload.addEventListener('progress', (event) => this._progressHandler(event), false);
            ajax.addEventListener('load', (event) => this._completeHandler(event), false);
            ajax.addEventListener('error', (event) => this._errorHandler(event), false);
            ajax.addEventListener('abort', (event) => this._abortHandler(event), false);
            ajax.open('POST', '/lr1121');
            ajax.setRequestHeader('X-FileSize', file.size);
            const radio = document.querySelector('input[name=optionsRadio]:checked')?.value || '1';
            ajax.setRequestHeader('X-Radio', radio);
            const formdata = new FormData();
            formdata.append('upload', file, file.name);
            ajax.send(formdata);
        } catch (e) {
            this._resetProgress();
            mui.overlay('off');
        }
    }

    _progressHandler(event) {
        const percent = Math.round((event.loaded / event.total) * 100);
        this.progress = percent;
        this.status = percent + '% uploaded... please wait';
    }

    async _completeHandler(event) {
        this.status = '';
        this.progress = 0;
        const data = JSON.parse(event.target.responseText || '{}');
        if (data.status === 'ok') {
            // Simulate flashing progress
            let percent = 0;
            const interval = setInterval(async () => {
                percent = percent + 2;
                this.progress = percent;
                this.status = percent + '% flashed... please wait';
                if (percent >= 100) {
                    clearInterval(interval);
                    await this._showAlert('success', 'Update Succeeded', data.msg);
                }
            }, 100);
        } else {
            await this._showAlert('error', 'Update Failed', data.msg || '');
        }
    }

    _errorHandler(event) {
        return this._showAlert('error', 'Update Failed', event?.target?.responseText || '');
    }

    _abortHandler(event) {
        return this._showAlert('info', 'Update Aborted', event?.target?.responseText || '');
    }

    _resetProgress() {
        this.status = '';
        this.progress = 0;
    }

    _showAlert(type, title, message) {
        this._resetProgress();
        try {
            mui.overlay('off');
        } catch (e) {
        }
        return cuteAlert({type, title, message});
    }
}
