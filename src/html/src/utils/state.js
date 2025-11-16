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
    saveJSONWithReboot('Configuration Update Succeeded', 'Configuration Update Failed', '/config', changes, successCB)
}

export function saveOptions(changes, successCB) {
    saveJSONWithReboot('Configuration Update Succeeded', 'Configuration Update Failed', '/options.json', changes, successCB)
}

export function saveOptionsAndConfig(changes, successCB) {
    postJSON('/options.json', changes.options, {
        onload: async () => {
            elrsState.options = changes.options
            saveConfig(changes.config, () => {
                elrsState.config = changes.config
                return successCB()
            })
        },
        onerror: async (xhr) => {
            await errorAlert('Configuration Update Failed', xhr.responseText || 'Request failed')
        }
    })
}

export let elrsState = new ElrsState()
