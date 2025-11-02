import {html, LitElement} from "lit"
import {customElement} from "lit/decorators.js"
import '../assets/mui.js'
import {saveJSONWithReboot} from "../utils/feedback.js"

@customElement('models-panel')
class ModelsPanel extends LitElement {
    createRenderRoot() {
        return this
    }

    render() {
        return html`
            <div class="mui-panel mui--text-title">Import/Export Module Settings</div>
            <div class="mui-panel">
                <p>Backup your global transmitter module and model configurations stored in the transmitter module.</p>
                <div>
                    <a href="/config?export" download="models.json" target="_blank"
                       class="mui-btn mui-btn--primary">Export module settings</a>
                </div>
            </div>
            <div class="mui-panel">
                <p>Restore your transmitter module and model configurations from a previous export.</p>
                <div>
                    <file-drop label="Import module settings" @file-drop=${this.upload}>or drop model JSON configuration file here</file-drop>
                </div>
            </div>
        `
    }

    upload(e) {
        const files = e.detail.files
        const reader = new FileReader()
        reader.onload = (x) => saveJSONWithReboot(
            'Upload Model Configuration',
            'An error occurred while uploading model configuration file',
            '/import',
            x.target.result,
            () => { return 'Model configuration updated, reboot for them to take effect' }
        )
        reader.readAsText(files[0])
    }
}
