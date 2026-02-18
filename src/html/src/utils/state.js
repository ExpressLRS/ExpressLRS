import {State} from "@lit-app/state";
import {errorAlert, postJSON, saveJSONWithReboot} from "./feedback.js";

class ElrsState extends State {
    config = {}
    options = {}
    settings = {}
}

export function formatBand() {
    if (elrsState.settings) {
        if (elrsState.settings.reg_domain_low && elrsState.settings.reg_domain_high) {
            return elrsState.settings.reg_domain_low + '/' + elrsState.settings.reg_domain_high
        }
        if (elrsState.settings.reg_domain_low)
            return elrsState.settings.reg_domain_low
        return elrsState.settings.reg_domain_high
    }
}

export function formatWifiRssi() {
    let wifiDesc = "";
    if (elrsState.settings) {
        if (elrsState.settings.mode === "STA")
            wifiDesc = `Client [${elrsState.settings.ssid}]`;
        else
            wifiDesc = "AP mode";

        // If we have dbm and it isn't 0, add it
        if (elrsState.settings?.wifi_dbm)
        {
            let dbm = elrsState.settings.wifi_dbm;
            wifiDesc += ` ${dbm}dBm `;
            if (dbm > -30)
                wifiDesc += "**GODLIKE**";
            else if (dbm > -60)
                wifiDesc += "excellent signal";
            else if (dbm == -69)
                wifiDesc += "nice signal";
            else if (dbm > -80)
                wifiDesc += "good signal";
            else if (dbm > -90)
                wifiDesc += "poor signal";
            else
                wifiDesc += "Unusable";
        }
    }
    return wifiDesc;
}

export function saveConfig(changes, successCB) {
    const currentPWM = elrsState.config.pwm
    if (changes.pwm) {
        // update pwm settings
        for (let i = 0; i < currentPWM.length; i++) {
            currentPWM[i].config = changes.pwm[i]
        }
    } else if (currentPWM) {
        // preserve original pwm settings
        changes.pwm = []
        for (let i = 0; i < currentPWM.length; i++) {
            changes.pwm.push(currentPWM[i].config)
        }
    }
    const newConfig = {...elrsState.config, ...changes}
    saveJSONWithReboot('Configuration Update Succeeded', 'Configuration Update Failed', '/config', newConfig, () => {
        elrsState.config = {...newConfig, pwm: currentPWM}
        if (successCB) successCB()
    })
}

export function saveOptions(changes, successCB) {
    const newOptions = {...elrsState.options, ...changes, customised: true}
    saveJSONWithReboot('Configuration Update Succeeded', 'Configuration Update Failed', '/options.json', newOptions, () => {
        elrsState.options = newOptions
        if (successCB) successCB()
    })
}

export function saveOptionsAndConfig(changes, successCB) {
    const newOptions = {...elrsState.options, ...changes.options, customised: true}
    postJSON('/options.json', newOptions, {
        onload: async () => {
            saveConfig(changes.config, () => {
                elrsState.options = newOptions
                if (successCB) successCB()
            })
        },
        onerror: async (xhr) => {
            await errorAlert('Configuration Update Failed', xhr.responseText || 'Request failed')
        }
    })
}

export let elrsState = new ElrsState()
