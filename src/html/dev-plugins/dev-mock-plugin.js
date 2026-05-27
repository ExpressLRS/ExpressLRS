import FEATURES from "../features.js";

// Dev plugin: provide mock ELRS endpoints to the local Vite server.
//
// Use this when you need a hardware-free UI workflow or deterministic test
// data. Do not use it to validate behavior that depends on a real device or
// real network timing.
export function devMockPlugin() {
    const PAGE_LOAD_DELAY_MS = 450
    const MODULE_LOAD_DELAY_MS = 800
    let cwGetRequestCount = 0

    function sendJSON(res, obj, status = 200) {
        res.statusCode = status
        res.setHeader('Content-Type', 'application/json')
        res.end(JSON.stringify(obj))
    }

    function sendText(res, text, status = 200) {
        res.statusCode = status
        res.setHeader('Content-Type', 'text/plain')
        res.end(text)
    }

    function sendStatus(res, status = 204) {
        res.statusCode = status
        res.end()
    }

    function sendDelayed(delayMs, sendResponse) {
        setTimeout(sendResponse, delayMs)
    }

    // Basic stub data used by multiple endpoints
    const stubState = {
        settings: {
            product_name: 'ELRS Mock Device',
            lua_name: "ELRS+PWM 2400RX",
            uidtype: 'Flashed',
            ssid: 'ExpressLRS TX',
            mode: 'AP',
            wifi_dbm: -60,
            custom_hardware: true,
            has_low_band: true,
            has_high_band: true,
            reg_domain_low: 'EU868',
            reg_domain_high: 'CE_LBT',
            target: "Unified_ESP32_LR1121",
            version: "25.0.0",
            "git-commit": "3468759",
            "module-type": FEATURES.IS_TX ? "TX" : "RX",
            "radio-type": FEATURES.HAS_SX128X ? "SX128X" : (FEATURES.HAS_LR1121 ? "LR1121" : "SX127X"),
        },
        options: {
            customised: true,
            "uid": [1, 2, 3, 4, 5, 6],   // this is the 'flashed' UID and may be empty if using traditional binding on an RX.
            "tlm-interval": 240,
            "fan-runtime": 30,
            "is-airport": true,
            "rcvr-uart-baud": 420000,
            "airport-uart-baud": 9600,
            "lock-on-first-connection": true,
            "domain": 1,
            "wifi-on-interval": 60,
            "wifi-password": "w1f1-pAssw0rd",
            "wifi-ssid": "network-ssid"
        },
        config: {
            uid: [5, 4, 3, 2, 1, 0],  // current UID, different to options if traditional binding or on-loan
            // RX config
            modelid: 62,
            'force-tlm': true,
            'serial-protocol': 1,
            'serial1-protocol': 0,
            'sbus-failsafe': 0,
            "pwm": [
                {"config": 0, "pin": 0, "features": 12},
                {"config": 1536, "pin": 4, "features": 12 + 16},
                {"config": 2048, "pin": 5, "features": 12 + 16},
                {"config": 3584, "pin": 1, "features": 1 + 4 + 8 + 16 + 32+ 64},
                {"config": 4608, "pin": 3, "features": 2 + 4 + 8 + 16 + 32+ 64}
            ],
            vbind: 0,
            // TX config
            "button-actions": [
                {
                    "color": 255,
                    "action": [
                        {"is-long-press": false, "count": 3, "action": 6},
                        {"is-long-press": true, "count": 5, "action": 1}
                    ]
                },
                {
                    "color": 224,
                    "action": [
                        {"is-long-press": false, "count": 2, "action": 3},
                        {"is-long-press": true, "count": 0, "action": 4}
                    ]
                }
            ]
        }
    }
    let networkQueryCount = 0
    const hardwareState = {
        serial_rx: 3,
        serial_tx: 1,
        radio_miso: 33,
        radio_mosi: 32,
        radio_sck: 25,
        radio_busy: 36,
        radio_dio1: 37,
        radio_nss: 27,
        radio_rst: 15,
        radio_busy_2: 39,
        radio_dio1_2: 34,
        radio_nss_2: 13,
        radio_rst_2: 21,
        radio_rfo_hf: true,
        power_apc2: 26,
        power_min: 0,
        power_high: 6,
        power_max: 6,
        power_default: 3,
        power_control: 3,
        power_values: [120, 120, 120, 120, 120, 120, 100],
        power_values2: [-17, -15, -12, -9, -5, 0, 7],
        power_values_dual: [-18, -18, -15, -10, -6, -2, 2],
        radio_rfsw_ctrl: [31, 0, 20, 24, 24, 2, 0, 1],
        led_rgb: 22,
        led_rgb_isgrb: true,
        ledidx_rgb_status: [0],
        ledidx_rgb_boot: [0],
        radio_dcdc: true,
        use_backpack: true,
        debug_backpack_baud: 460800,
        debug_backpack_rx: 18,
        debug_backpack_tx: 5,
        backpack_boot: 23,
        backpack_en: 19,
        misc_fan_en: 2,
        vbat: 36,
        vbat_offset: 0,
        vbat_scale: 230,
        vbat_atten: 3,
        vbat_noreading: -1,
        vbat_cal_min: 5000,
        vbat_cal_max: 16000,
        vsrc1: 39,
        vsrc1_offset: 0,
        vsrc1_scale: 180,
        vsrc1_atten: 1,
        vsrc1_noreading: 8,
        // vsrc1_cal_min: 3300,
        // vsrc1_cal_max: 12600
    }
    const voltageSources = []

    function countVoltageSourcesFromHardware(hardware) {
        let count = 0
        if (hardware.vbat !== undefined && hardware.vbat !== '' && Number(hardware.vbat) >= 0) count++
        if (hardware.vsrc1 !== undefined && hardware.vsrc1 !== '' && Number(hardware.vsrc1) >= 0) count++
        if (hardware.vsrc2 !== undefined && hardware.vsrc2 !== '' && Number(hardware.vsrc2) >= 0) count++
        if (hardware.vsrc3 !== undefined && hardware.vsrc3 !== '' && Number(hardware.vsrc3) >= 0) count++
        return count
    }

    function syncVoltageSourcesFromHardware(hardware) {
        const sourceDefs = [
            {id: 'vbat', label: 'VBat'},
            {id: 'vsrc1', label: 'VSrc1'},
            {id: 'vsrc2', label: 'VSrc2'},
            {id: 'vsrc3', label: 'VSrc3'},
        ]
        voltageSources.length = 0
        for (const sourceDef of sourceDefs) {
            const pin = hardware[sourceDef.id]
            if (pin === undefined || pin === '' || Number(pin) < 0) continue
            voltageSources.push({
                id: sourceDef.id,
                label: sourceDef.label,
                pin: Number(pin),
                defined: true,
                offset: Number(hardware[`${sourceDef.id}_offset`] ?? 0),
                scale: Number(hardware[`${sourceDef.id}_scale`] ?? 200),
                atten: Number(hardware[`${sourceDef.id}_atten`] ?? -1),
                noReading: Number(hardware[`${sourceDef.id}_noreading`] ?? -1),
                calMin: Number(hardware[`${sourceDef.id}_cal_min`] ?? 0),
                calMax: Number(hardware[`${sourceDef.id}_cal_max`] ?? 0),
            })
        }
        if (stubState.settings['module-type'] === 'RX') {
            stubState.settings.voltage_source_count = countVoltageSourcesFromHardware(hardware)
        } else {
            delete stubState.settings.voltage_source_count
        }
    }

    syncVoltageSourcesFromHardware(hardwareState)

    function jitter(amount = 1) {
        return Math.round((Math.random() * amount * 2) - amount)
    }

    function mockVoltageSample(atten, base, span, readingFloor = 0) {
        const attenGain = atten <= 3 ? (4 - atten) : (8 - atten)
        const rawMedian = Math.max(readingFloor, Math.min(4095, Math.round(base + span * attenGain + jitter(2))))
        const spread = 2 + Math.abs(jitter(1))
        const rawMax = Math.min(4095, rawMedian + spread + Math.abs(jitter(1)))
        const adcMedian = Math.max(0, Math.min(4095, rawMedian + jitter(1)))
        return {
            rawMax,
            adcMedian,
            saturated: rawMax >= 4071,
            hasReading: rawMedian > readingFloor
        }
    }

    return {
        name: 'vite-dev-mock',
        apply: 'serve',
        configureServer(server) {
            server.middlewares.use((req, res, next) => {
                const url = req.url || '/'
                const method = (req.method || 'GET').toUpperCase()

                // Add delay for lazy-loaded page group modules to simulate network latency
                if (method === 'GET' && (url.includes('general-group.js') || url.includes('advanced-group.js'))) {
                    return setTimeout(() => next(), MODULE_LOAD_DELAY_MS)
                }

                // Utilities to collect request body (json or text)
                const readBody = () => new Promise((resolve) => {
                    let data = ''
                    req.on('data', (chunk) => {
                        data += chunk
                    })
                    req.on('end', () => resolve(data))
                })

                if (method === 'GET' && url === '/config?export') {
                    return sendJSON(res, stubState.config)
                }
                if (method === 'GET' && url === '/config') {
                    // Reset the networks scan delay counter whenever config is fetched
                    networkQueryCount = 0
                    if (stubState.settings['module-type'] === 'RX') {
                        stubState.settings.voltage_source_count = voltageSources.length
                    } else {
                        delete stubState.settings.voltage_source_count
                    }
                    return sendDelayed(PAGE_LOAD_DELAY_MS, () => sendJSON(res, stubState))
                }
                if (method === 'GET' && (url === '/networks.json' || url.startsWith('/networks.json'))) {
                    networkQueryCount++
                    if (networkQueryCount <= 3) {
                        return sendStatus(res, 204)
                    }
                    return sendJSON(res, ['ExpressLRS TX', 'MockHomeWiFi', 'OfficeNet'])
                }
                if (method === 'POST' && (url === '/options' || url === '/options.json')) {
                    return readBody().then(() => sendText(res, 'Options saved'))
                }
                if (method === 'POST' && url === '/config') {
                    return readBody().then((body) => {
                        try {
                            const data = JSON.parse(body || '{}')
                            const pwm = stubState.config.pwm
                            stubState.config = {...data, pwm}
                            if (data.pwm) {
                                let i = 0
                                stubState.config.pwm.forEach((item) => {
                                    item.config = data.pwm[i++]
                                })
                            }
                        } catch (e) {
                            // ignore parse errors in mock
                        }
                        return sendText(res, 'Config saved')
                    })
                }
                if (method === 'GET' && url === '/cw') {
                    cwGetRequestCount++
                    // Fail every third request to test error handling
                    if (cwGetRequestCount % 3 === 0) {
                        return sendDelayed(PAGE_LOAD_DELAY_MS, () => sendStatus(res, 500))
                    }
                    const cwConfig = {
                        radios: hardwareState.radio_nss_2 === undefined ? 1 : 2,
                        center: FEATURES.HAS_SX128X ? 2440000000 : 868000000
                    }
                    if (FEATURES.HAS_LR1121) {
                        cwConfig.center2 = 2400000000
                    }
                    return sendDelayed(PAGE_LOAD_DELAY_MS, () => sendJSON(res, cwConfig))
                }
                if (method === 'POST' && url === '/cw') {
                    return readBody().then(() => sendText(res, 'CW started'))
                }
                if (method === 'POST' && url === '/buttons') {
                    return readBody().then(() => sendText(res, 'ok'))
                }
                if (method === 'POST' && url === '/update') {
                    return readBody().then(() => sendJSON(res, {
                        status: 'ok',
                        msg: 'Firmware updated successfully. Device will reboot.'
                    }))
                }
                if (method === 'POST' && url === '/forceupdate') {
                    return readBody().then(() => sendJSON(res, {
                        status: 'ok',
                        msg: 'Forced firmware update started.'
                    }))
                }
                if (method === 'POST' && (url === '/reset?lr1121' || url === '/reset')) {
                    return sendText(res, 'LR1121 reset requested. Rebooting...')
                }
                if (method === 'GET' && url === '/lr1121.json') {
                    return sendDelayed(PAGE_LOAD_DELAY_MS, () => sendJSON(res, {
                        manual: false,
                        radio1: {type: 0x07, hardware: 0x11, firmware: 0x1234},
                        radio2: FEATURES.HAS_LR1121 ? {type: 0x07, hardware: 0x12, firmware: 0x1234} : undefined
                    }))
                }
                if (method === 'POST' && url === '/lr1121') {
                    return readBody().then(() => sendJSON(res, {status: 'ok', msg: 'LR1121 firmware updated'}))
                }
                if (method === 'GET' && url === '/hardware.json') {
                    return sendDelayed(PAGE_LOAD_DELAY_MS, () => sendJSON(res, hardwareState))
                }
                if (method === 'POST' && url === '/hardware.json') {
                    return readBody().then((body) => {
                        try {
                            const data = JSON.parse(body || '{}')
                            Object.assign(hardwareState, data)
                            syncVoltageSourcesFromHardware(hardwareState)
                        } catch (e) {
                            // ignore parse errors in mock
                        }
                        return sendText(res, 'Hardware saved')
                    })
                }
                if (method === 'POST' && url === '/reboot') {
                    return sendText(res, 'Rebooting')
                }
                if (method === 'POST' && url === '/binding') {
                    return sendText(res, 'Binding')
                }
                if (method === 'POST' && url === '/sethome') {
                    return sendText(res, 'Home set')
                }
                if (method === 'POST' && url === '/import') {
                    return sendText(res, 'Import complete')
                }
                if (method === 'GET' && url === '/import') {
                    return sendText(res, JSON.stringify(stubState))
                }
                if (method === 'POST' && url === '/voltage-sample') {
                    return readBody().then((body) => {
                        let payload = {}
                        try {
                            payload = JSON.parse(body || '{}')
                        } catch (_e) {
                        }

                        const sampleForRequest = (requestPayload = {}) => {
                            const sourceId = requestPayload.source || 'vbat'
                            const source = voltageSources.find((item) => item.id === sourceId) || voltageSources[0]
                            if (!source) {
                                return {hasReading: false}
                            }

                            const stage = Number(requestPayload.stage ?? 0)
                            const atten = Number(requestPayload.atten ?? source.atten ?? 0)

                            let base = 180
                            let span = 180
                            let readingFloor = 0

                            if (stage === 0) {
                                base = 700
                                span = 320
                            } else if (stage === 1) {
                                base = 320
                                span = 150
                            } else if (stage === 2) {
                                base = Math.max(0, source.noReading - 12)
                                span = 4
                                readingFloor = Math.max(0, source.noReading - 24)
                            } else if (stage === 3) {
                                base = 540
                                span = 210
                            }

                            return mockVoltageSample(atten, base, span, readingFloor)
                        }

                        const requests = Array.isArray(payload.requests) ? payload.requests : []
                        const samples = {}
                        for (const requestPayload of requests) {
                            const sourceId = requestPayload?.source
                            if (!sourceId) continue
                            samples[sourceId] = sampleForRequest(requestPayload)
                        }
                        return sendJSON(res, {samples})
                    })
                }

                return next()
            })
        }
    }
}
