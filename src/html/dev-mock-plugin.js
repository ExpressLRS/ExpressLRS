import FEATURES from "./src/features.js";

export function devMockPlugin() {
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

    // Basic stub data used by multiple endpoints
    const stubState = {
        settings: {
            product_name: 'ELRS Mock Device',
            lua_name: "ELRS+PWM 2400RX",
            uidtype: 'Flashed',
            ssid: 'ExpressLRS TX',
            mode: 'AP',
            custom_hardware: true,
            has_low_band: false,
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
    return {
        name: 'vite-dev-mock',
        apply: 'serve',
        configureServer(server) {
            server.middlewares.use((req, res, next) => {
                const url = req.url || '/'
                const method = (req.method || 'GET').toUpperCase()

                // Utilities to collect request body (json or text)
                const readBody = () => new Promise((resolve) => {
                    let data = ''
                    req.on('data', (chunk) => {
                        data += chunk
                    })
                    req.on('end', () => resolve(data))
                })

                if (method === 'GET' && url === '/config?export') {
                    return sendJSON(res, {})
                }
                if (method === 'GET' && url === '/config') {
                    // Reset the networks scan delay counter whenever config is fetched
                    networkQueryCount = 0
                    return sendJSON(res, stubState)
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
                if (method === 'POST' && url === '/buttons') {
                    return readBody().then(() => sendText(res, 'OK'))
                }
                if (method === 'POST' && url === '/reboot') {
                    return sendText(res, 'Rebooting...')
                }
                if (method === 'POST' && url === '/forceupdate') {
                    return sendText(res, 'Force update scheduled')
                }
                if (method === 'POST' && url === '/import') {
                    return sendText(res, 'Import OK')
                }
                if (method === 'POST' && url === '/update') {
                    // Treat as file upload; we won’t parse multipart in this mock
                    return sendJSON(res, {
                        "status": "ok",
                        "msg": "Update complete. Please wait for the LED to resume blinking before disconnecting power."
                    })
                    // return sendJSON(res, {"status": "mismatch", "msg": "Wrong!"})
                    // return sendJSON(res, {"status": "error", "msg": "Bugger."})
                }
                // WiFi page mock endpoints
                if (method === 'POST' && (url === '/sethome' || url.startsWith('/sethome?'))) {
                    // Accept FormData multipart; no need to parse for mock
                    return readBody().then(() => sendText(res, 'WiFi settings applied'))
                }
                if (method === 'POST' && url === '/access') {
                    return sendText(res, 'Access Point started')
                }
                if (method === 'POST' && url === '/forget') {
                    return sendText(res, 'Home network forgotten')
                }
                if (method === 'POST' && url === '/connect') {
                    return sendText(res, 'Connecting to Home network...')
                }
                // CW page mock endpoints
                if (url === '/cw' && method === 'GET') {
                    return sendJSON(res, {center: 915000000, center2: 2440000000, radios: 2})
                }
                if (url === '/cw' && method === 'POST') {
                    return sendText(res, 'CW started')
                }
                // LR1121 page mock endpoints
                if (method === 'GET' && url === '/lr1121.json') {
                    return sendJSON(res, {
                        manual: false,
                        radio1: {type: 0xA1, hardware: 0x01, firmware: 0x0102},
                        radio2: {type: 0xA1, hardware: 0x02, firmware: 0x0102}
                    })
                }
                if (method === 'POST' && url.startsWith('/lr1121')) {
                    // treat as file upload; ignore body
                    return sendJSON(res, {
                        status: 'ok',
                        msg: 'LR1121 firmware flashed successfully (mock). Reboot device to take effect.'
                    })
                }
                if (method === 'POST' && url.startsWith('/reset')) {
                    // e.g. /reset?lr1121
                    if (url.endsWith('?options')) {
                        return sendText(res, 'Reset complete, rebooting...')
                    }
                    return sendText(res, 'Custom firmware flag cleared, rebooting...')
                }
                // Hardware page mock endpoints
                if (url === '/hardware.json' && method === 'GET') {
                    return sendJSON(res, {
                        "serial_rx": 3,
                        "serial_tx": 1,

                        "radio_miso": 33,
                        "radio_mosi": 32,
                        "radio_sck": 25,

                        "radio_busy": 36,
                        "radio_dio1": 37,
                        "radio_nss": 27,
                        "radio_rst": 15,

                        "radio_busy_2": 39,
                        "radio_dio1_2": 34,
                        "radio_nss_2": 13,
                        "radio_rst_2": 21,

                        "radio_rfo_hf": true,
                        "power_apc2": 26,

                        "power_min": 0,
                        "power_high": 6,
                        "power_max": 6,
                        "power_default": 3,
                        "power_control": 3,
                        "power_values": [120, 120, 120, 120, 120, 120, 100],
                        "power_values2": [-17, -15, -12, -9, -5, 0, 7],
                        "power_values_dual": [-18, -18, -15, -10, -6, -2, 2],
                        "radio_rfsw_ctrl": [31, 0, 20, 24, 24, 2, 0, 1],


                        "led_rgb": 22,
                        "led_rgb_isgrb": true,
                        "ledidx_rgb_status": [0],
                        "ledidx_rgb_boot": [0],

                        "radio_dcdc": true,

                        "use_backpack": true,
                        "debug_backpack_baud": 460800,
                        "debug_backpack_rx": 18,
                        "debug_backpack_tx": 5,
                        "backpack_boot": 23,
                        "backpack_en": 19,

                        "misc_fan_en": 2
                    })
                }
                if (url === '/hardware.json' && method === 'POST') {
                    return readBody().then(() => sendText(res, 'Hardware config saved'))
                }
                next()
            })
        }
    }
}
