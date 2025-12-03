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
    const newOptions = {...elrsState.options, ...changes}
    saveJSONWithReboot('Configuration Update Succeeded', 'Configuration Update Failed', '/options.json', newOptions, () => {
        elrsState.options = newOptions
        if (successCB) successCB()
    })
}

export function saveOptionsAndConfig(changes, successCB) {
    const newOptions = {...elrsState.options, ...changes.options}
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
