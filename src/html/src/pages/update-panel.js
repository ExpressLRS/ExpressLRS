import {html, LitElement} from "lit"
import {customElement, state} from "lit/decorators.js"
import '../assets/mui.js'
import '../components/filedrag.js'
import FEATURES from "../features.js"
import {cuteAlert} from "../utils/feedback.js"

@customElement('update-panel')
class UpdatePanel extends LitElement {
    @state() accessor progress = 0
    @state() accessor progressText = ''

    createRenderRoot() {
        this._completeHandler = this._completeHandler.bind(this)
        this._progressHandler = this._progressHandler.bind(this)
        return this
    }

    render() {
        return html`
            <div class="mui-panel mui--text-title">Firmware Update</div>
            <div class="mui-panel">
                <p>
                    Select the correct <strong>firmware.bin${FEATURES.IS_8285 ? '.gz' : ''}</strong> for your platform otherwise a bad flash may occur.
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
        let expectedFileExt
        let expectedFileExtDesc
        if (FEATURES.IS_8285 && !FEATURES.IS_TX) {
            expectedFileExt = 'gz'
            expectedFileExtDesc = '.bin.gz file. <br />Do NOT decompress/unzip/extract the file!'
        } else {
            expectedFileExt = 'bin'
            expectedFileExtDesc = '.bin file.'
        }
        if (fileExt === expectedFileExt) {
            this._uploadFile(files[0])
        } else {
            cuteAlert({
                type: 'error',
                title: 'Incorrect File Format',
                message: 'You selected the file &quot;' + files[0].name.toString() + '&quot;.<br />The firmware file must be a ' + expectedFileExtDesc
            })
        }
    }

    _uploadFile(file) {
        const formdata = new FormData()
        formdata.append('upload', file, file.name)
        const ajax = new XMLHttpRequest()
        ajax.upload.addEventListener('progress', this._progressHandler, false)
        ajax.addEventListener('load', this._completeHandler, false)
        ajax.addEventListener('error', this._errorHandler, false)
        ajax.addEventListener('abort', this._abortHandler, false)
        ajax.open('POST', '/update')
        ajax.setRequestHeader('X-FileSize', file.size)
        ajax.send(formdata)
    }

    _progressHandler(event) {
        const percent = Math.round((event.loaded / event.total) * 100)
        this.progress = percent
        this.progressText = percent + '% uploaded... please wait'
        this.requestUpdate()
    }

    _completeHandler(event) {
        const self = this
        this.progressText = ''
        this.progress = 0
        const data = JSON.parse(event.target.responseText)
        if (data.status === 'ok') {
            function showMessage() {
                cuteAlert({
                    type: 'success',
                    title: 'Update Succeeded',
                    message: data.msg
                })
            }
            // This is basically a delayed display of the success dialog with a fake progress
            let percent = 0
            const interval = setInterval(()=>{
                if (FEATURES.IS_8285)
                    percent = percent + 1
                else
                    percent = percent + 2

                self.progress = percent
                self.progressText = percent + '% flashed... please wait'
                if (percent === 100) {
                    clearInterval(interval)
                    self.progressText = ''
                    self.progress = 0
                    showMessage()
                }
                self.requestUpdate()
            }, 100)
        } else if (data.status === 'mismatch') {
            cuteAlert({
                type: 'question',
                title: 'Targets Mismatch',
                message: data.msg,
                confirmText: 'Flash anyway',
                cancelText: 'Cancel'
            }).then((e)=>{
                const xmlhttp = new XMLHttpRequest()
                xmlhttp.onreadystatechange = function() {
                    if (this.readyState === 4) {
                        self.progressText = ''
                        self.progress = 0
                        self.requestUpdate()
                        if (this.status === 200) {
                            const data = JSON.parse(this.responseText)
                            cuteAlert({
                                type: 'info',
                                title: 'Force Update',
                                message: data.msg
                            })
                        } else {
                            cuteAlert({
                                type: 'error',
                                title: 'Force Update',
                                message: 'An error occurred trying to force the update'
                            })
                        }
                    }
                }
                xmlhttp.open('POST', '/forceupdate', true)
                const data = new FormData()
                data.append('action', e)
                xmlhttp.send(data)
            })
        } else {
            cuteAlert({
                type: 'error',
                title: 'Update Failed',
                message: data.msg
            })
        }
        this.requestUpdate()
    }

    _errorHandler(event) {
        this.progressText = ''
        this.progress = 0
        return cuteAlert({
            type: 'error',
            title: 'Update Failed',
            message: event.target.responseText
        })
    }

    _abortHandler(event) {
        this.progressText = ''
        this.progress = 0
        return cuteAlert({
            type: 'info',
            title: 'Update Aborted',
            message: event.target.responseText
        })
    }
}
