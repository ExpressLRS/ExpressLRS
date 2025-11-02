import {State} from "@lit-app/state";
import {errorAlert, postJSON, saveJSONWithReboot} from "./feedback.js";

class ElrsState extends State {
    config = {}
    options = {}
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
