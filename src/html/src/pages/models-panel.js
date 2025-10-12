import {html, LitElement} from "lit"
import {customElement} from "lit/decorators.js"
import '../assets/mui.js'
import {postWithFeedback} from "../utils/feedback.js"

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
                    <button class="mui-btn mui-btn--accent upload">
                        <label>
                            <input type="file" id="fileselect" name="fileselect[]" @change="${this.upload}"/>
                            Import module settings
                        </label>
                    </button>
                </div>
            </div>
        `
    }

    upload(e) {
        const files = e.target.files || e.dataTransfer.files
        const reader = new FileReader()
        reader.onload = (x) => postWithFeedback(
            'Upload Model Configuration',
            'An error occurred while uploading model configuration file',
            '/import',
            () => {return x.target.result}
        )(e)
        reader.readAsText(files[0])
    }
}
