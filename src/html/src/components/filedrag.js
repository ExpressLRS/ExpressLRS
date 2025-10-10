import {html, LitElement} from 'lit';
import {customElement, property} from "lit/decorators.js";
import {unsafeHTML} from 'lit-html/directives/unsafe-html.js';

@customElement('file-drop')
export class FileDrop extends LitElement {
    @property()
    accessor label

    // Preserve light DOM rendering to keep bundle small
    createRenderRoot() {
        return this;
    }

    constructor() {
        super();
        this._projectedHTML = '';
    }

    connectedCallback() {
        // Capture initial light-DOM children before Lit renders and replaces them
        // This emulates <slot> when using light DOM (createRenderRoot returns `this`)
        if (this._projectedHTML === '' && this.innerHTML.trim() !== '') {
            this._projectedHTML = this.innerHTML;
            this.innerHTML = '';
        }
        super.connectedCallback();
    }

    render() {
        const fallback = 'Drop files here';
        return html`
            <button class="mui-btn mui-btn--small mui-btn--primary upload">
                <label>
                    ${this.label}
                    <input type="file" id="fileselect" name="fileselect[]" @change=${this._selectFiles}/>
                </label>
            </button>
            <div
                    class="drop-zone"
                    @dragover=${this._handleDragOver}
                    @dragleave=${this._handleDragLeave}
                    @drop=${this._handleDrop}
            >
                ${this._projectedHTML ? unsafeHTML(this._projectedHTML) : fallback}
            </div>
        `;
    }

    _handleDragOver(event) {
        event.preventDefault(); // This is necessary to allow a drop.
        event.target.classList.add('dragover');
    }

    _handleDragLeave(event) {
        event.target.classList.remove('dragover');
    }

    _handleDrop(event) {
        event.preventDefault(); // Prevent file from being opened by the browser.
        event.target.classList.remove('dragover');
        this._callback(event.dataTransfer.files);
    }

    _selectFiles(event) {
        this._callback(event.target.files);
    }

    _callback(files) {
        if (files.length) {
            // Dispatch a custom 'file-drop' event with the files in the detail property.
            this.dispatchEvent(new CustomEvent('file-drop', {
                detail: {files},
                bubbles: true,
                composed: true
            }));
        }

    }
}
