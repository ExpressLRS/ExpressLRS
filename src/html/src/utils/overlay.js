const OVERLAY_ID = 'mui-overlay'
const BODY_CLASS = 'mui--overflow-hidden'

let activeElement = null
let keyupHandler = null
let clickHandler = null

function getOverlay() {
    return document.getElementById(OVERLAY_ID)
}

function clearChildren(element) {
    while (element.firstChild) {
        element.removeChild(element.firstChild)
    }
}

function clearKeyupHandler() {
    if (!keyupHandler) return
    document.removeEventListener('keyup', keyupHandler)
    keyupHandler = null
}

function clearClickHandler(overlayEl) {
    if (!overlayEl || !clickHandler) return
    overlayEl.removeEventListener('click', clickHandler)
    clickHandler = null
}

export function overlay(action, options = {}, childElement) {
    if (action === 'on') return overlayOn(options, childElement)
    if (action === 'off') return overlayOff()
    throw new Error("Expecting 'on' or 'off'")
}

export function overlayOn(options = {}, childElement) {
    let overlayEl = getOverlay()
    activeElement = document.activeElement instanceof HTMLElement ? document.activeElement : null
    document.body.classList.add(BODY_CLASS)

    if (!overlayEl) {
        overlayEl = document.createElement('div')
        overlayEl.id = OVERLAY_ID
        overlayEl.tabIndex = -1
        document.body.appendChild(overlayEl)
    } else {
        clearChildren(overlayEl)
        clearClickHandler(overlayEl)
    }

    if (childElement) {
        overlayEl.appendChild(childElement)
    }

    const resolvedOptions = {
        keyboard: options.keyboard !== false,
        static: options.static === true,
        onclose: options.onclose,
    }
    overlayEl.overlayOptions = resolvedOptions

    clearKeyupHandler()
    if (resolvedOptions.keyboard) {
        keyupHandler = (event) => {
            if (event.key === 'Escape' || event.keyCode === 27) {
                overlayOff()
            }
        }
        document.addEventListener('keyup', keyupHandler)
    }

    if (!resolvedOptions.static) {
        clickHandler = (event) => {
            if (event.target === overlayEl) {
                overlayOff()
            }
        }
        overlayEl.addEventListener('click', clickHandler)
    }

    overlayEl.focus()
    return overlayEl
}

export function overlayOff() {
    const overlayEl = getOverlay()
    let onclose

    if (overlayEl) {
        clearChildren(overlayEl)
        onclose = overlayEl.overlayOptions?.onclose
        clearClickHandler(overlayEl)
        overlayEl.parentNode?.removeChild(overlayEl)
    }

    document.body.classList.remove(BODY_CLASS)
    clearKeyupHandler()

    if (activeElement) {
        activeElement.focus()
        activeElement = null
    }

    if (onclose) {
        onclose()
    }

    return overlayEl
}
