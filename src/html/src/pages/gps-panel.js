import {html, LitElement} from "lit"
import {customElement, state} from "lit/decorators.js"
import {loadJSON} from "../utils/feedback.js"

const POLL_INTERVAL_MS = 1000
// Beyond this the module has gone quiet, so the last position is stale and the rate is meaningless
const STALE_AFTER_MS = 3000

// Mirrors gpsState_e in SerialGPS.h
const STATE_LABELS = {
    0: 'Searching for module',
    1: 'Configuring (UBX VALSET)',
    2: 'Configuring (UBX CFG-MSG)',
    3: 'Changing baud rate',
    4: 'Running',
}

const PROTOCOL_LABELS = {0: 'Unknown', 1: 'NMEA', 2: 'UBX (u-blox binary)'}

function intervalToHz(intervalMs) {
    if (!intervalMs || intervalMs <= 0) return null
    return 1000 / intervalMs
}

function formatHz(intervalMs) {
    const hz = intervalToHz(intervalMs)
    return hz === null ? '—' : `${hz.toFixed(hz >= 10 ? 0 : 1)} Hz`
}

function pad2(n) {
    return String(n).padStart(2, '0')
}

function formatCoord(scaled1e7, positive, negative) {
    const deg = scaled1e7 / 1e7
    const hemi = deg >= 0 ? positive : negative
    return `${Math.abs(deg).toFixed(6)}° ${hemi}`
}

function formatAge(ageMs) {
    if (ageMs === undefined || ageMs >= 0xffffffff) return 'never'
    if (ageMs < 1500) return 'just now'
    return `${(ageMs / 1000).toFixed(0)}s ago`
}

@customElement('gps-panel')
class GpsPanel extends LitElement {
    @state() accessor gps = null
    @state() accessor error = ''
    @state() accessor loaded = false

    timer = null

    createRenderRoot() {
        return this
    }

    connectedCallback() {
        super.connectedCallback()
        this._poll()
    }

    disconnectedCallback() {
        if (this.timer) {
            clearTimeout(this.timer)
            this.timer = null
        }
        super.disconnectedCallback()
    }

    async _poll() {
        try {
            this.gps = await loadJSON('/gps', 'Failed to load GPS status')
            this.error = ''
        } catch (e) {
            this.error = e.message || 'Failed to load GPS status'
        } finally {
            this.loaded = true
        }
        this.timer = setTimeout(() => this._poll(), POLL_INTERVAL_MS)
    }

    get _stale() {
        return !this.gps || this.gps.age_ms === undefined || this.gps.age_ms >= STALE_AFTER_MS
    }

    _statusBadge() {
        const gps = this.gps
        if (!gps || !gps.present) {
            return {cls: 'warning-bg', text: 'No GPS running'}
        }
        if (this._stale) {
            return {cls: 'error-bg', text: gps.state === 4 ? 'Signal lost' : STATE_LABELS[gps.state] || 'Searching'}
        }
        if (gps.state !== 4) {
            return {cls: 'info-bg', text: STATE_LABELS[gps.state] || 'Working'}
        }
        if (gps.fix_valid) {
            return {cls: 'success-bg', text: `${gps.fix_type >= 3 ? '3D' : '2D'} fix · ${gps.satellites} sats`}
        }
        return {cls: 'warning-bg', text: `Acquiring · ${gps.satellites} sats`}
    }

    render() {
        return html`
            <div class="mui-panel mui--text-title">GPS Status</div>
            ${this._renderBody()}
        `
    }

    _renderBody() {
        if (!this.loaded) {
            return html`<div class="mui-panel">Loading…</div>`
        }
        if (this.error) {
            return html`<div class="mui-panel error-bg">${this.error}</div>`
        }
        const gps = this.gps
        if (!gps || !gps.present) {
            return html`
                <div class="mui-panel">
                    <p>No GPS driver is running on this receiver.</p>
                    <p>Set one of the serial protocols to <b>GPS</b> on the Serial page and reboot,
                       then wire a GPS module to that UART.</p>
                </div>`
        }

        const badge = this._statusBadge()
        return html`
            <div class="mui-panel ${badge.cls}">
                <div class="mui--text-headline">${badge.text}</div>
                <div class="mui--text-caption">Last update ${formatAge(gps.age_ms)}</div>
            </div>

            <div class="mui-panel">
                <div class="mui--text-title">Link</div>
                <table class="mui-table mui-table--bordered">
                    <tbody>
                        <tr><td><b>Protocol</b></td><td>${PROTOCOL_LABELS[gps.protocol] ?? 'Unknown'}</td></tr>
                        <tr><td><b>Baud rate</b></td><td>${gps.baud.toLocaleString()} bps</td></tr>
                        <tr><td><b>Update rate</b></td><td>${this._stale ? '—' : formatHz(gps.update_interval_ms)}</td></tr>
                        <tr><td><b>Requested rate</b></td><td>${gps.nav_interval_ms > 0 ? formatHz(gps.nav_interval_ms) : 'default (NMEA)'}</td></tr>
                        <tr><td><b>Auto-configuration</b></td><td>${this._configText(gps)}</td></tr>
                    </tbody>
                </table>
                ${!gps.can_configure ? html`
                    <div class="mui-panel info-bg">
                        Read-only: no TX line is wired to the module, so it can only be listened to
                        (NMEA) and cannot be reconfigured.
                    </div>` : ''}
            </div>

            <div class="mui-panel">
                <div class="mui--text-title">Fix</div>
                <table class="mui-table mui-table--bordered">
                    <tbody>
                        <tr><td><b>Fix</b></td><td>${this._fixText(gps)}</td></tr>
                        <tr><td><b>Satellites</b></td><td>${gps.satellites}</td></tr>
                        <tr><td><b>Latitude</b></td><td>${gps.fix_valid ? formatCoord(gps.lat, 'N', 'S') : '—'}</td></tr>
                        <tr><td><b>Longitude</b></td><td>${gps.fix_valid ? formatCoord(gps.lon, 'E', 'W') : '—'}</td></tr>
                        <tr><td><b>Altitude</b></td><td>${gps.fix_valid ? `${(gps.alt_cm / 100).toFixed(1)} m` : '—'}</td></tr>
                        <tr><td><b>Ground speed</b></td><td>${gps.fix_valid ? `${(gps.speed_kmh100 / 100).toFixed(1)} km/h` : '—'}</td></tr>
                        <tr><td><b>Heading</b></td><td>${gps.fix_valid ? `${(gps.heading100 / 100).toFixed(1)}°` : '—'}</td></tr>
                        <tr><td><b>UTC time</b></td><td>${this._timeText(gps)}</td></tr>
                    </tbody>
                </table>
                ${gps.fix_valid ? html`
                    <a class="mui-btn mui-btn--small mui-btn--primary"
                       href=${this._mapUrl(gps)}
                       target="_blank" rel="noopener noreferrer">View on map</a>` : ''}
            </div>
        `
    }

    _configText(gps) {
        if (!gps.can_configure) return 'Not possible (no TX line)'
        if (gps.ubx_configured) return `Applied via ${gps.used_valset ? 'VALSET' : 'CFG-MSG'}`
        if (gps.state <= 3) return 'In progress…'
        return 'Not a u-blox, left as NMEA'
    }

    _fixText(gps) {
        if (!gps.fix_valid) return `No fix (${gps.satellites} sats)`
        if (gps.fix_type >= 3) return '3D fix'
        if (gps.fix_type === 2) return '2D fix'
        return 'Fix'
    }

    _mapUrl(gps) {
        const lat = (gps.lat / 1e7).toFixed(6)
        const lon = (gps.lon / 1e7).toFixed(6)
        return `https://www.openstreetmap.org/?mlat=${lat}&mlon=${lon}&zoom=15`
    }

    _timeText(gps) {
        if (!gps.time_valid || !gps.year) return '—'
        return `${gps.year}-${pad2(gps.month)}-${pad2(gps.day)} ` +
               `${pad2(gps.hour)}:${pad2(gps.minute)}:${pad2(gps.second)} UTC`
    }
}
