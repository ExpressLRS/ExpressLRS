import {html, LitElement} from 'lit';
import {customElement, query, state} from 'lit/decorators.js';
import FEATURES from "../features.js";
import {post} from "../utils/feedback.js";

@customElement('continuous-wave')
export class ContinuousWave extends LitElement {
    @query('#optionsSetSubGHz') accessor optionsSetSubGHz;
    @query('#optionsRadios2') accessor optionsRadios2;

    @state() accessor data = undefined;
    @state() accessor started = false;
    @state() accessor result = {}
    @state() accessor cwFreq;

    _text = "Loading...";

    createRenderRoot() {
        return this;
    }

    render() {
        if (this.data)
            return html`
                <div class="mui-panel mui--text-title">Continuous Wave Generation</div>
                <div class="mui-panel">
                    Put the Semtech chip into a mode where it transmits a continuous wave with a center
                    frequency of ${(this.cwFreq / 1000000)}</span> MHz.
                    <br>
                    You can then measure the actual continuous wave center frequency using a spectrum analyzer and enter
                    the
                    measured value below. This will be used to calculate the accuracy of the crystal used in the device
                    and
                    how far it differs from the ideal frequency.
                    <form class="mui-form">
                        ${this.data.radios === 2 ? html`
                            <div class="mui-radio">
                                <label>
                                    <input type="radio"
                                           name="optionsRadios"
                                           value="1"
                                           ?disabled=${this.started}
                                           checked>
                                    Radio 1
                                </label>
                                <label>
                                    <input type="radio"
                                           name="optionsRadios"
                                           id="optionsRadios2"
                                           value="2"
                                           ?disabled=${this.started}>
                                    Radio 2
                                </label>
                            </div>
                        ` : html``}
                        <br>
                        <!-- FEATURE:HAS_LR1121 -->
                        Basic support is available for the LR1121 and setting 915 MHz.
                        <br>
                        <div class="mui-checkbox">
                            <label>
                                <input type="checkbox"
                                       name="setSubGHz"
                                       id="optionsSetSubGHz"
                                       ?disabled=${this.started}
                                       @click="${this._updateFreq}">
                                Set 915 MHz
                            </label>
                        </div>
                        <br>
                        <!-- /FEATURE:HAS_LR1121 -->
                        <button class="mui-btn mui-btn--primary" ?disabled=${this.started} @click="${this._startCW}">
                            Start Continuous Wave
                        </button>
                    </form>
                    <br>
                    <div class="mui-textfield">
                        <input type='text' required @change="${this._measured}"/>
                        <label>Center Frequency</label>
                    </div>
                    <div style="display: ${this.result.calculated ? 'block' : 'none'};">
                        <table class="mui-table mui-table--bordered">
                            <tr>
                                <td>Calculated XO Freq</td>
                                <td>${this.result.calculated}</td>
                            </tr>
                            <tr>
                                <td>Calculated XO Offset (kHz)</td>
                                <td>${this.result.offset}</td>
                            </tr>
                            <tr>
                                <td>Calculated XO Offset (PPM)</td>
                                <td>${this.result.ppm}</td>
                            </tr>
                            <tr>
                                <td>Raw Offset (kHz)</td>
                                <td>${this.result.raw}</td>
                            </tr>
                            <tr>
                                <td>TL;DR</td>
                                <td>${this.result.tldr}</td>
                            </tr>
                        </table>
                    </div>
                </div>
            `
        else
            return html`
                <link rel="stylesheet" href="../assets/mui.css">
                <div class="mui--text-title">${this._text}</div>
            `
    }

    connectedCallback() {
        super.connectedCallback();
        const xmlhttp = new XMLHttpRequest();
        xmlhttp.onreadystatechange = () => {
            if (xmlhttp.readyState === 4 && xmlhttp.status === 200) {
                this._updateParams(JSON.parse(xmlhttp.responseText));
            } else {
                this._text = "Failed to load data.";
            }
        };
        xmlhttp.open('GET', '/cw', true);
        xmlhttp.send();
    }

    _updateParams(data) {
        this.data = data;
        this._updateFreq();
    }

    _updateFreq() {
        this.cwFreq = this.data.center;
        if (FEATURES.HAS_LR1121 && !this.optionsSetSubGHz?.checked) {
            this.cwFreq = this.data.center2;
        }
    }

    _startCW(e) {
        e.stopPropagation();
        e.preventDefault();
        this.started = true;
        const formdata = new FormData();
        formdata.append('radio', this.optionsRadios2?.checked ? 2 : 1);
        if (FEATURES.HAS_LR1121) {
            formdata.append('subGHz', this.optionsSetSubGHz.checked ? 1 : 0);
        }
        post('/cw', formdata)
    }

    _measured(e) {
        let xtalNominal = 52000000;
        let warn_offset = 90000;
        let bad_offset = 180000;

        if (FEATURES.HAS_SX127X || FEATURES.HAS_LR1121) {
            xtalNominal = 32000000;
            warn_offset = 100000;
            bad_offset = 125000;
        }

        const calc = (e.target.value / this.cwFreq) * xtalNominal;
        const rawShift = Math.round(e.target.value - this.cwFreq);
        let icon;
        if (Math.abs(rawShift) < warn_offset) {
            icon = html`
                <svg width="64" height="64" viewBox="0 0 96 96">
                    <path fill-rule="evenodd" fill="#7c6"
                          d="M48,0c26.51,0,48,21.49,48,48S74.51,96,48,96S0,74.51,0,48 S21.49,0,48,0L48,0z M26.764,49.277c0.644-3.734,4.906-5.813,8.269-3.79c0.305,0.182,0.596,0.398,0.867,0.646l0.026,0.025 c1.509,1.446,3.2,2.951,4.876,4.443l1.438,1.291l17.063-17.898c1.019-1.067,1.764-1.757,3.293-2.101 c5.235-1.155,8.916,5.244,5.206,9.155L46.536,63.366c-2.003,2.137-5.583,2.332-7.736,0.291c-1.234-1.146-2.576-2.312-3.933-3.489 c-2.35-2.042-4.747-4.125-6.701-6.187C26.993,52.809,26.487,50.89,26.764,49.277L26.764,49.277z"/>
                </svg>`;
        } else if (Math.abs(rawShift) < bad_offset) {
            icon = html`
                <svg width="64" height="64" viewBox="0 0 20 20">
                    <path fill="#fc3"
                          d="M19.64 16.36L11.53 2.3A1.85 1.85 0 0 0 10 1.21 1.85 1.85 0 0 0 8.48 2.3L.36 16.36C-.48 17.81.21 19 1.88 19h16.24c1.67 0 2.36-1.19 1.52-2.64zM11 16H9v-2h2zm0-4H9V6h2z"/>
                </svg>`;
        } else {
            icon = html`
                <svg width="64" height="64" viewBox="0 0 122.88 122.88">
                    <path fill="#e00" d="M61.44,0A61.44,61.44,0,1,1,0,61.44,61.44,61.44,0,0,1,61.44,0Z"/>
                    <path fill-rule="evenodd" fill="#fff"
                          d="M35.38,49.72c-2.16-2.13-3.9-3.47-1.19-6.1l8.74-8.53c2.77-2.8,4.39-2.66,7,0L61.68,46.86,73.39,35.15c2.14-2.17,3.47-3.91,6.1-1.2L88,42.69c2.8,2.77,2.66,4.4,0,7L76.27,61.44,88,73.21c2.65,2.58,2.79,4.21,0,7l-8.54,8.74c-2.63,2.71-4,1-6.1-1.19L61.68,76,49.9,87.81c-2.58,2.64-4.2,2.78-7,0l-8.74-8.53c-2.71-2.63-1-4,1.19-6.1L47.1,61.44,35.38,49.72Z"/>
                </svg>`;
        }

        this.result = {
            calculated: Math.round(calc),
            offset: Math.round(calc - xtalNominal) / 1000,
            ppm: Math.abs(Math.round(calc - xtalNominal)) / (xtalNominal / 1000000),
            raw: rawShift / 1000,
            tldr: icon
        };
    }
}
