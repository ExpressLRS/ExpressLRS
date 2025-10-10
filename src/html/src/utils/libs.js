/* eslint-disable max-len */
/* eslint-disable comma-dangle */
/* eslint-disable require-jsdoc */

// =========================================================

import {html} from "lit";

export function _(el) {
    return document.getElementById(el);
}

export function _renderOptions(options, selected) {
    return options.map(
        (label, index) => html`
                <option .value="${index.toString()}" ?selected="${index === selected}">${label}</option>
            `
    );
}

