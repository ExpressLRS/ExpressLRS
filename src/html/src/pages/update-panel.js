import {html, LitElement} from "lit"
import {customElement, state} from "lit/decorators.js"
import '../components/filedrag.js'
import {post, showAlert, showConfirm} from "../utils/feedback.js"

@customElement('update-panel')
class UpdatePanel extends LitElement {
    @state() accessor progress = 0
    @state() accessor progressText = ''

    createRenderRoot() {
        this._completeHandler = this._completeHandler.bind(this)
        this._progressHandler = this._progressHandler.bind(this)
        this._errorHandler = this._errorHandler.bind(this)
        this._abortHandler = this._abortHandler.bind(this)
        return this
    }

    render() {
        return html`
            <div class="mui-panel mui--text-title">Firmware Update</div>
            <div class="mui-panel">
                <p>
                    Select the correct
                    <!-- FEATURE:IS_8285 -->
                    <strong>firmware.bin.gz</strong>
                    <!-- /FEATURE:IS_8285 -->
                    <!-- FEATURE:NOT IS_8285 -->
                    <strong>firmware.bin</strong>
                    <!-- /FEATURE:NOT IS_8285 -->
                    for your platform otherwise a bad flash may occur.
                    If this happens you will need to recover via USB/Serial. You may also download the <a
                        href="firmware.bin" title="Click to download firmware">currently running firmware</a>.
                </p>
                <file-drop id="firmware-upload" label="Select firmware file" @file-drop="${this._fileSelectHandler}">or drop firmware file here</file-drop>
                <br/>
                <h3 id="status">${this.progressText}</h3>
                <progress id="progressBar" value="0" max="100" style="width:100%;" .value="${this.progress}"></progress>
            </div>
        `
    }


    _fileSelectHandler(e) {
        // ESP32 expects .bin, ESP8285 RX expect .bin.gz
        const files = e.detail.files
        const fileExt = files[0].name.split('.').pop()
        // FEATURE:IS_8285
        const expectedFileExt = 'gz'
        const expectedFileExtDesc = '.bin.gz file. <br />Do NOT decompress/unzip/extract the file!'
        // /FEATURE:IS_8285
        // FEATURE:NOT IS_8285
        const expectedFileExt = 'bin'
        const expectedFileExtDesc = '.bin file.'
        // /FEATURE:NOT IS_8285
        if (fileExt === expectedFileExt) {
            this._uploadFile(files[0])
        } else {
            showAlert('error', 'Incorrect File Format', 'You selected the file &quot;' + files[0].name.toString() + '&quot;.<br />The firmware file must be a ' + expectedFileExtDesc)
        }
    }

    _uploadFile(file) {
        post('/update', file, {
            onprogress: this._progressHandler,
            onload: this._completeHandler,
            onerror: this._errorHandler,
            onabort: this._abortHandler,
        })
    }

    _progressHandler(event) {
        const percent = Math.round((event.loaded / event.total) * 100)
        this.progress = percent
        this.progressText = percent + '% uploaded... please wait'
    }

    _completeHandler(request) {
        this._resetProgress()
        const data = JSON.parse(request.responseText)
        if (data.status === 'ok') {
            this._showFlashingProgress(data.msg)
        } else if (data.status === 'mismatch') {
            this._confirmForceUpdate(data.msg)
        } else {
            this._showAlert('error', 'Update Failed', data.msg)
        }
    }

    _errorHandler(request) {
        return this._showAlert('error', 'Update Failed', request.responseText)
    }

    _abortHandler(request) {
        return this._showAlert('info', 'Update Aborted', request.responseText)
    }

    _resetProgress() {
        this.progressText = ''
        this.progress = 0
    }

    _showAlert(type, title, message) {
        this._resetProgress()
        return showAlert(type, title, message)
    }

    _showFlashingProgress(message) {
        let percent = 0
        const interval = setInterval(() => {
            // FEATURE:IS_8285
            percent = percent + 1
            // /FEATURE:IS_8285
            // FEATURE:NOT IS_8285
            percent = percent + 2
            // /FEATURE:NOT IS_8285
            this.progress = percent
            this.progressText = percent + '% flashed... please wait'
            if (percent === 100) {
                clearInterval(interval)
                this._showAlert('success', 'Update Succeeded', message)
            }
        }, 100)
    }

    _confirmForceUpdate(message) {
        showConfirm('Targets Mismatch', message, 'Flash anyway', 'Cancel').then((action) => {
            if (action !== 'confirm') return
            const data = new FormData()
            data.append('action', action)
            post('/forceupdate', data, {
                onload: (xhr) => {
                    const response = JSON.parse(xhr.responseText)
                    this._showAlert('info', 'Force Update', response.msg)
                },
                onerror: () => {
                    this._showAlert('error', 'Force Update', 'An error occurred trying to force the update')
                }
            })
        })
    }
}
