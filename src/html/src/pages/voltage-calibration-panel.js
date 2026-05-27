import {html, LitElement} from "lit"
import {customElement, state} from "lit/decorators.js"
import {post, showAlert, saveJSONWithReboot} from "../utils/feedback.js"
import {loadHardware, setHardwareState} from "../utils/state.js"

// FEATURE:IS_8285
const VOLTAGE_SOURCE_DEFS = [{id: 'vbat', label: 'VBat'}]
const CALIBRATION_ATTENUATIONS = [-1]
const ADC_MAX_VALUE = 1023
const ADC_SATURATION_MARGIN = 6
const ADC_MIN_SPAN = 16
// /FEATURE:IS_8285
// FEATURE:NOT IS_8285
const VOLTAGE_SOURCE_DEFS = [
    {id: 'vbat', label: 'VBat'},
    {id: 'vsrc1', label: 'VSrc1'},
    {id: 'vsrc2', label: 'VSrc2'},
    {id: 'vsrc3', label: 'VSrc3'}
]
const CALIBRATION_ATTENUATIONS = [4,5,6,7]
const ADC_MAX_VALUE = 4095
const ADC_SATURATION_MARGIN = 24
const ADC_MIN_SPAN = 64
// /FEATURE:NOT IS_8285
const SAMPLE_REQUEST_TIMEOUT_MS = 500
const SAMPLE_FAILURE_ALERT_THRESHOLD = 5
const STEP_HIGH = 0
const STEP_LOW = 1
const STEP_DISCONNECTED = 2
const STEP_REVIEW = 3

function getOptionalNumber(hardware, key) {
    const value = hardware[key]
    if (value == null || value === '') {
        return null
    }
    const numericValue = Number(value)
    return Number.isNaN(numericValue) ? null : numericValue
}

function getVoltageSourcesFromHardware(hardware) {
    const sources = []
    for (const source of VOLTAGE_SOURCE_DEFS) {
        const pin = hardware[source.id]
        if (pin == null || pin === '' || Number(pin) < 0) {
            continue
        }

        sources.push({
            id: source.id,
            label: source.label,
            pin: Number(pin),
            offset: getOptionalNumber(hardware, `${source.id}_offset`),
            scale: getOptionalNumber(hardware, `${source.id}_scale`),
            atten: getOptionalNumber(hardware, `${source.id}_atten`),
            noReading: getOptionalNumber(hardware, `${source.id}_noreading`),
            calMin: getOptionalNumber(hardware, `${source.id}_cal_min`),
            calMax: getOptionalNumber(hardware, `${source.id}_cal_max`)
        })
    }
    return sources
}

function adcToMilliVolts(adc, offset, scale) {
    if (!scale || scale <= 0 || adc == null) {
        return null
    }
    if (offset < 0 && adc <= -offset) {
        return 0
    }
    return Math.round(((adc - offset) * 10000) / scale)
}

function formatMilliVolts(milliVolts) {
    if (milliVolts == null) {
        return 'unavailable'
    }
    return `${(milliVolts / 1000).toFixed(2)}V`
}

function hasCalibrationRange(source) {
    return Number(source?.calMin) > 0 && Number(source?.calMax) > Number(source?.calMin)
}

function formatLiveVoltage(liveSample, result, source) {
    if (!liveSample?.hasReading) return 'voltage unavailable'
    const milliVolts = adcToMilliVolts(liveSample.adcMedian, result?.offset ?? source?.offset, result?.scale ?? source?.scale)
    return milliVolts === null ? 'voltage unavailable' : formatMilliVolts(milliVolts)
}

class PollingLoop {
    timer = null
    token = 0

    start(task, delayMs, {immediate = false} = {}) {
        this.stop()
        const token = ++this.token

        const schedule = (delay) => {
            this.timer = setTimeout(async () => {
                this.timer = null
                if (token !== this.token) {
                    return
                }
                await task()
                if (token === this.token) {
                    schedule(delayMs)
                }
            }, delay)
        }

        schedule(immediate ? 0 : delayMs)
    }

    stop() {
        this.token += 1
        if (this.timer) {
            clearTimeout(this.timer)
            this.timer = null
        }
    }
}

class SerializedSampleQueue {
    queueTail = Promise.resolve()
    consecutiveFailures = 0
    alertPromise = null

    constructor({timeoutMs, failureThreshold, onFailureThreshold}) {
        this.timeoutMs = timeoutMs
        this.failureThreshold = failureThreshold
        this.onFailureThreshold = onFailureThreshold
    }

    _sendSampleRequest(payload) {
        return new Promise((resolve, reject) => {
            post('/voltage-sample', payload, {
                timeoutMs: this.timeoutMs,
                onload: (xhr) => {
                    try {
                        resolve(JSON.parse(xhr.responseText || '{}'))
                    } catch (_e) {
                        reject(new Error('Invalid voltage sample batch response'))
                    }
                },
                onerror: (xhr) => {
                    reject(new Error(xhr.responseText || 'Voltage sampling batch request failed'))
                },
                ontimeout: () => {
                    reject(new Error('Voltage sampling batch request timed out'))
                }
            })
        })
    }

    enqueue(payload) {
        const run = async () => {
            try {
                const response = await this._sendSampleRequest(payload)
                this.consecutiveFailures = 0
                return response
            } catch (error) {
                this.consecutiveFailures += 1
                if (this.consecutiveFailures >= this.failureThreshold) {
                    this.consecutiveFailures = 0
                    if (!this.alertPromise) {
                        this.alertPromise = Promise.resolve()
                            .then(() => this.onFailureThreshold())
                            .finally(() => {
                                this.alertPromise = null
                            })
                    }
                    await this.alertPromise
                }
                throw error
            }
        }

        const queuedRequest = this.queueTail.then(run, run)
        this.queueTail = queuedRequest.catch(() => {})
        return queuedRequest
    }
}

@customElement('voltage-calibration-panel')
class VoltageCalibrationPanel extends LitElement {
    @state() accessor sources = []
    @state() accessor selectedSource = null
    @state() accessor step = STEP_HIGH
    @state() accessor loading = false
    @state() accessor liveSample = null
    @state() accessor lowVoltage = '5000'
    @state() accessor highVoltage = '16000'
    @state() accessor highCapture = null
    @state() accessor result = null
    @state() accessor error = ''
    @state() accessor sourceReadings = {}

    sourceReadingsLoadPromise = null

    constructor() {
        super()
        this.livePoller = new PollingLoop()
        this.sourceReadingsPoller = new PollingLoop()
        this.sampleRequestQueue = new SerializedSampleQueue({
            timeoutMs: SAMPLE_REQUEST_TIMEOUT_MS,
            failureThreshold: SAMPLE_FAILURE_ALERT_THRESHOLD,
            onFailureThreshold: () => this._handleSampleFailureThreshold()
        })
    }

    createRenderRoot() {
        return this
    }

    connectedCallback() {
        super.connectedCallback()
        this._loadSources()
    }

    disconnectedCallback() {
        this._stopLivePolling()
        this.sourceReadingsPoller.stop()
        super.disconnectedCallback()
    }

    render() {
        return html`
            <div class="mui-panel mui--text-title">Voltage Calibration</div>
            <div class="mui-panel">
                <p>
                    Calibrate voltage sources and save the calibration results to the hardware configuration <code>hardware.json</code>.
                </p>
                ${this.error && !this.selectedSource ? html`<div class="mui-panel error-bg">${this.error}</div>` : ''}
                ${this._renderSourceSelector()}
            </div>
            ${this.selectedSource ? this._renderWizard() : ''}
        `
    }

    _renderSourceSelector() {
        if (this.sources.length === 0) {
            return html`<p>No voltage sources are defined on this target.</p>`
        }
        return html`
            ${this.sources.map((source) => html`
                <div class="mui-panel">
                    <div class="mui--text-title">${source.label}</div>
                    ${hasCalibrationRange(source)
                        ? html`
                            <div>Range: ${hasCalibrationRange(source)
                                ? `${formatMilliVolts(source.calMin)} to ${formatMilliVolts(source.calMax)}`
                                : 'not configured'}</div>
                            <div>Reading: ${((reading) => {
                                if (!reading?.sample) return 'loading...'
                                if (!reading.sample.hasReading) return 'no reading'
                                return formatMilliVolts(reading.milliVolts)
                            })(this.sourceReadings[source.id])}</div>
                            <button class="mui-btn mui-btn--primary mui-btn--small" @click=${() => this._startWizard(source)}>Calibrate</button>
                        ` : html`<div class="mui-panel warning-bg">Set cal_min/cal_max in Hardware Layout first.</div>`
                    }
                </div>
            `)}
        `
    }

    _renderWizard() {
        return html`
            <div class="alert-wrapper" @click=${this._resetWizard}>
                <div class="alert-frame wizard" @click=${(e) => e.stopPropagation()}>
                    <div class="alert-header">
                            <div class="alert-title-row">
                                <span class="alert-title-icon icon--symbols icon--symbols--voltage" aria-hidden="true"></span>
                                <div class="wizard-title mui--text-title">${this.selectedSource?.label} Calibration</div>
                            </div>
                        <span class="alert-close" @click=${this._resetWizard}>X</span>
                    </div>
                    <div class="alert-body">
                        ${this.error && this.selectedSource ? html`<div class="mui-panel error-bg">${this.error}</div>` : ''}
                        ${this.step === STEP_REVIEW && this.liveSample ? html`
                            <div class="mui-panel info-bg">
                                <div class="mui--text-headline">Live Voltage: <strong>${formatLiveVoltage(this.liveSample, this.result, this.selectedSource)}</strong></div>
                            </div>
                        ` : ''}
                        ${this._renderWizardStep()}
                    </div>
                </div>
            </div>
        `
    }

    _renderPointStep(title, targetMv, value, setValue, capture, buttonLabel) {
        return html`
            <div class="mui-panel">
                <div class="mui--text-title">${title}</div>
                <p>Set a known voltage near ${formatMilliVolts(targetMv)} and capture it.</p>
                <div class="mui-textfield">
                    <input type="number" .value=${value} @input=${setValue} />
                    <label>Known voltage (mV)</label>
                </div>
                <button class="mui-btn mui-btn--primary" ?disabled=${this.loading || !value} @click=${capture}>${buttonLabel}</button>
            </div>
        `
    }

    _renderWizardStep() {
        switch (this.step) {
            case STEP_HIGH:
                return this._renderPointStep(
                    '1. High Voltage',
                    this.selectedSource?.calMax,
                    this.highVoltage,
                    (e) => this.highVoltage = e.target.value,
                    this._captureHigh,
                    'Capture High'
                )
            case STEP_LOW:
                return this._renderPointStep(
                    '2. Low Voltage',
                    this.selectedSource?.calMin,
                    this.lowVoltage,
                    (e) => this.lowVoltage = e.target.value,
                    this._captureLow,
                    'Capture Low'
                )
            case STEP_DISCONNECTED:
                return html`
                    <div class="mui-panel">
                        <div class="mui--text-title">3. No Voltage</div>
                        <p>Disconnect the source and capture the idle reading, or skip if it is always present.</p>
                        <button class="mui-btn mui-btn--primary" ?disabled=${this.loading} @click=${() => this._captureDisconnected(false)}>Capture</button>
                        <button class="mui-btn" ?disabled=${this.loading} @click=${() => this._captureDisconnected(true)}>Skip</button>
                    </div>
                `
            case STEP_REVIEW:
                return html`
                    <div class="mui-panel">
                        <div class="mui--text-title">4. Review</div>
                        <p>Offset ${this.result.offset}, scale ${this.result.scale}, no-reading ${this.result.noReading}</p>
                        <p>Fit error: low ${this.result.lowErrorMv}mV, high ${this.result.highErrorMv}mV</p>
                        <p>Reconnect or adjust the PSU and confirm the live reading before saving.</p>
                        <button class="mui-btn mui-btn--primary" @click=${this._save}>Save Calibration</button>
                    </div>
                `
            default:
                return ''
        }
    }

    async _loadSources() {
        this.error = ''
        try {
            const hardware = await loadHardware()
            this.sources = getVoltageSourcesFromHardware(hardware)
            await this._loadSourceReadings()
            this._startSourceReadingsPolling()
        } catch (_e) {
            this.error = 'Failed to load voltage sources.'
        }
    }

    _startWizard(source) {
        this.sourceReadingsPoller.stop()
        this._stopLivePolling()
        this.selectedSource = source
        this.step = STEP_HIGH
        this.loading = false
        this.highCapture = null
        this.result = null
        this.error = ''
        this.lowVoltage = String(source.calMin || 5000)
        this.highVoltage = String(source.calMax || 16000)
        this._startLivePolling(this.step)
    }

    async _showStep(step) {
        this.step = step
        await this.updateComplete
        this._startLivePolling(step)
    }

    async _runWizardAction(action) {
        this.loading = true
        this.error = ''
        this._stopLivePolling()
        try {
            await action()
        } catch (error) {
            this.error = error.message
        } finally {
            this.loading = false
        }
    }

    _resetWizard = () => {
        this._stopLivePolling()
        this.step = STEP_HIGH
        this.selectedSource = null
        this.liveSample = null
        this.highCapture = null
        this.result = null
        this.error = ''
        this._loadSourceReadings()
        this._startSourceReadingsPolling()
    }

    async _loadSourceReadings() {
        this.sourceReadingsLoadPromise ??= this._loadSourceReadingsInternal().finally(() => {
            this.sourceReadingsLoadPromise = null
        })
        return this.sourceReadingsLoadPromise
    }

    async _loadSourceReadingsInternal() {
        const readings = {}
        const batchRequests = []

        for (const source of this.sources) {
            if (!hasCalibrationRange(source))
                readings[source.id] = {sample: null, milliVolts: null, skipped: true}
            else
                batchRequests.push({source: source.id, atten: source.atten, samples: 12})
        }

        if (!batchRequests.length) {
            this.sourceReadings = readings
            return
        }

        try {
            const response = await this.sampleRequestQueue.enqueue({requests: batchRequests})
            const samples = response?.samples ?? {}

            for (const source of this.sources) {
                if (!hasCalibrationRange(source)) {
                    continue
                }
                const sample = samples[source.id] ?? null
                readings[source.id] = {
                    sample: sample,
                    milliVolts: sample ? adcToMilliVolts(sample.adcMedian, source.offset, source.scale) : null
                }
            }
        } catch (_e) {
            for (const source of this.sources) {
                if (!hasCalibrationRange(source)) {
                    continue
                }
                readings[source.id] = {sample: null, milliVolts: null}
            }
        }

        this.sourceReadings = readings
    }

    _startSourceReadingsPolling() {
        if (this.selectedSource || this.sources.length === 0) {
            return
        }
        this.sourceReadingsPoller.start(async () => {
            if (!this.selectedSource) {
                await this._loadSourceReadings()
            }
        }, 1000, {immediate: true})
    }

    async _sample(stage, atten, samples = 24, source = this.selectedSource?.id) {
        const response = await this.sampleRequestQueue.enqueue({
            requests: [{source, atten, samples, stage}]
        })
        const sample = response?.samples?.[source] ?? null
        if (!sample) {
            throw new Error('Voltage sample request failed')
        }
        return sample
    }

    async _handleSampleFailureThreshold() {
        this._stopLivePolling()
        this.sourceReadingsPoller.stop()
        this.error = 'WiFi communications lost while sampling voltage.'
        await showAlert(
            'error',
            'WiFi Communications Lost',
            'Voltage sampling failed repeatedly. Check the WiFi connection and try again.'
        )
    }

    async _sampleCalibrationCandidates(stage, requestedAttens) {
        const candidates = []
        const viableAttens = []
        let previewAtten = null

        for (const atten of requestedAttens) {
            const {adcMedian, saturated} = await this._sample(stage, atten, 24)
            const candidate = { atten, adcMedian, saturated }
            candidates.push(candidate)
            if (!candidate.saturated) {
                viableAttens.push(atten)
                previewAtten ??= atten
            }
        }

        return {
            voltageMv: Number(stage === STEP_HIGH ? this.highVoltage : this.lowVoltage),
            candidates,
            viableAttens,
            previewAtten
        }
    }

    _computeCalibrationResult(lowCapture, highCapture) {
        const lowVoltageMv = Number(lowCapture?.voltageMv || 0)
        const highVoltageMv = Number(highCapture?.voltageMv || 0)
        if (highVoltageMv <= lowVoltageMv) {
            throw new Error('High voltage must be greater than low voltage')
        }

        let supportedMinVoltageMv = Number(this.selectedSource?.calMin || 0)
        let supportedMaxVoltageMv = Number(this.selectedSource?.calMax || 0)
        if (supportedMinVoltageMv <= 0 || supportedMinVoltageMv >= supportedMaxVoltageMv) {
            supportedMinVoltageMv = lowVoltageMv
            supportedMaxVoltageMv = highVoltageMv
        }

        const voltageSpan = highVoltageMv - lowVoltageMv
        let bestAtten = null
        let bestSelectionSpan = -1
        let bestCaptureSpan = -1
        let bestLowAdc = 0
        let bestHighAdc = 0

        for (const lowCandidate of lowCapture.candidates || []) {
            if (lowCandidate.saturated) continue

            for (const highCandidate of highCapture.candidates || []) {
                const adcSpan = highCandidate.adcMedian - lowCandidate.adcMedian
                const projectedLowAdc = lowCandidate.adcMedian + ((supportedMinVoltageMv - lowVoltageMv) * adcSpan) / voltageSpan
                const projectedHighAdc = lowCandidate.adcMedian + ((supportedMaxVoltageMv - lowVoltageMv) * adcSpan) / voltageSpan
                const adcCeiling = ADC_MAX_VALUE - ADC_SATURATION_MARGIN
                const supportedSpan = projectedHighAdc - projectedLowAdc

                if (highCandidate.saturated ||
                    highCandidate.atten !== lowCandidate.atten ||
                    highCandidate.adcMedian <= lowCandidate.adcMedian ||
                    adcSpan < ADC_MIN_SPAN ||
                    projectedHighAdc >= adcCeiling ||
                    projectedHighAdc <= projectedLowAdc ||
                    (projectedHighAdc - projectedLowAdc) < ADC_MIN_SPAN) continue

                if (supportedSpan > bestSelectionSpan || (supportedSpan === bestSelectionSpan && lowCandidate.atten < bestAtten)) {
                    bestSelectionSpan = supportedSpan
                    bestCaptureSpan = adcSpan
                    bestAtten = lowCandidate.atten
                    bestLowAdc = lowCandidate.adcMedian
                    bestHighAdc = highCandidate.adcMedian
                }
            }
        }

        if (bestSelectionSpan < ADC_MIN_SPAN || bestCaptureSpan < ADC_MIN_SPAN || bestAtten === null) {
            throw new Error('No usable attenuation/calibration solution found')
        }

        const scale = Math.trunc((bestCaptureSpan * 10000) / voltageSpan)
        if (scale <= 0) {
            throw new Error('No usable calibration')
        }

        const offset = Math.trunc(bestLowAdc - ((lowVoltageMv * scale) / 10000))
        return {
            atten: bestAtten,
            offset,
            scale,
            noReading: -1,
            lowErrorMv: Math.trunc((((bestLowAdc - offset) * 10000) / scale) - lowVoltageMv),
            highErrorMv: Math.trunc((((bestHighAdc - offset) * 10000) / scale) - highVoltageMv)
        }
    }

    _startLivePolling(stage) {
        this.livePoller.start(async () => {
            await this._pollLive(stage)
        }, SAMPLE_REQUEST_TIMEOUT_MS, {immediate: true})
    }

    _stopLivePolling() {
        this.livePoller.stop()
    }

    async _pollLive(stage) {
        if (!this.selectedSource || this.step !== stage) {
            return
        }
        try {
            const atten =
                ((stage === STEP_DISCONNECTED || stage === STEP_REVIEW) && this.result?.atten != null)
                    ? this.result.atten
                    : (stage === STEP_LOW && this.highCapture?.previewAtten != null)
                        ? this.highCapture.previewAtten
                        : this.selectedSource?.atten ?? -1
            const sample = await this._sample(stage, atten, 12)
            if (this.selectedSource && this.step === stage) {
                this.liveSample = sample
            }
        } catch (_e) {
        }
    }

    async _captureDisconnected(skip) {
        if (!this.result) {
            return
        }

        await this._runWizardAction(async () => {
            if (skip) {
                this.result = {
                    ...this.result,
                    noReading: -1
                }
            } else {
                const resp = await this._sample(STEP_DISCONNECTED, this.result.atten)
                const noReading = resp.rawMax + 8
                this.result = {
                    ...this.result,
                    noReading
                }
            }
            await this._showStep(STEP_REVIEW)
        })
    }

    _captureLow = async () => {
        await this._runWizardAction(async () => {
            const lowCapture = await this._sampleCalibrationCandidates(STEP_LOW, this.highCapture?.viableAttens)
            this.result = this._computeCalibrationResult(lowCapture, this.highCapture)
            await this._showStep(STEP_DISCONNECTED)
        })
    }

    _captureHigh = async () => {
        await this._runWizardAction(async () => {
            this.highCapture = await this._sampleCalibrationCandidates(STEP_HIGH, CALIBRATION_ATTENUATIONS)
            if (!this.highCapture.viableAttens.length) {
                throw new Error('All calibrated attenuations saturated at the high point')
            }
            await this._showStep(STEP_LOW)
        })
    }

    _save = async () => {
        const selectedSourceId = this.selectedSource?.id
        if (!selectedSourceId || !this.result) {
            return
        }

        let nextHardware
        try {
            nextHardware = {
                ...await loadHardware(),
                customised: true,
                [`${selectedSourceId}_offset`]: this.result.offset,
                [`${selectedSourceId}_scale`]: this.result.scale,
                [`${selectedSourceId}_atten`]: this.result.atten,
                [`${selectedSourceId}_noreading`]: this.result.noReading
            }
        } catch (_e) {
            this.error = 'Failed to load hardware.'
            return
        }

        return saveJSONWithReboot(
            'Voltage Calibration Saved',
            'Voltage Calibration Failed',
            '/hardware.json',
            nextHardware,
            () => {
                setHardwareState(nextHardware)
                this.sources = getVoltageSourcesFromHardware(nextHardware)
                this._resetWizard()
            }
        )
    }
}
