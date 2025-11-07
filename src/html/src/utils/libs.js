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

export function _uintInput(e) {
    if (e.which !== 8 && e.which !== 0 && e.which < 48 || e.which > 57)
        e.preventDefault();
}

export function _intInput(e) {
    if (e.which !== 8 && e.which !== 0 && e.which !== 45 && e.which < 48 || e.which > 57)
        e.preventDefault();
}

export function _arrayInput(e) {
    if (e.which !== 32 && e.which !== 44 && e.which !== 45)
        _intInput(e)
}
