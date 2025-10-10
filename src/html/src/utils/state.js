import {State} from "@lit-app/state";
import {saveJSONWithReboot} from "./feedback.js";

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

export let elrsState = new ElrsState()
