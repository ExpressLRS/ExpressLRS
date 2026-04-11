let cleanupRipple = null

function ensureRipple(buttonEl) {
    if (buttonEl.tagName === 'INPUT') return null

    let rippleEl = buttonEl._rippleEl
    if (!rippleEl) {
        const container = document.createElement('span')
        container.className = 'mui-btn__ripple-container'
        rippleEl = document.createElement('span')
        rippleEl.className = 'mui-ripple'
        container.appendChild(rippleEl)
        buttonEl.appendChild(container)
        buttonEl._rippleEl = rippleEl
    }

    return rippleEl
}

function activateRipple(buttonEl, event) {
    if (buttonEl.disabled) return

    const rippleEl = ensureRipple(buttonEl)
    if (!rippleEl) return

    const rect = buttonEl.getBoundingClientRect()
    const point = event.touches ? event.touches[0] : event
    const radius = Math.sqrt(rect.height * rect.height + rect.width * rect.width)
    const diameter = `${radius * 2}px`

    rippleEl.style.width = diameter
    rippleEl.style.height = diameter
    rippleEl.style.top = `${Math.round(point.clientY - rect.top - radius)}px`
    rippleEl.style.left = `${Math.round(point.clientX - rect.left - radius)}px`
    rippleEl.classList.remove('mui--is-animating')
    rippleEl.classList.add('mui--is-visible')

    requestAnimationFrame(() => {
        rippleEl.classList.add('mui--is-animating')
    })
}

function deactivateRipple(buttonEl) {
    const rippleEl = buttonEl?._rippleEl
    if (!rippleEl) return

    requestAnimationFrame(() => {
        rippleEl.classList.remove('mui--is-visible')
    })
}

export function initRipple() {
    if (cleanupRipple) return cleanupRipple

    const activeButtons = new Set()
    const supportsPointer = 'PointerEvent' in window
    const startEvent = supportsPointer
        ? 'pointerdown'
        : ('ontouchstart' in document.documentElement ? 'touchstart' : 'mousedown')
    const endEvents = supportsPointer
        ? ['pointerup', 'pointerleave', 'pointercancel']
        : (startEvent === 'touchstart' ? ['touchend', 'touchcancel'] : ['mouseup', 'mouseleave'])

    const onStart = (event) => {
        if ((event.type === 'mousedown' || event.type === 'pointerdown') && event.button !== 0) return

        const buttonEl = event.target.closest?.('.mui-btn')
        if (!buttonEl) return

        activeButtons.add(buttonEl)
        activateRipple(buttonEl, event)
    }

    const onEnd = (event) => {
        const buttonEl = event.target.closest?.('.mui-btn')
        if (buttonEl) {
            activeButtons.delete(buttonEl)
            deactivateRipple(buttonEl)
            return
        }

        activeButtons.forEach((activeButton) => deactivateRipple(activeButton))
        activeButtons.clear()
    }

    document.addEventListener(startEvent, onStart, true)
    endEvents.forEach((eventName) => document.addEventListener(eventName, onEnd, true))

    cleanupRipple = () => {
        document.removeEventListener(startEvent, onStart, true)
        endEvents.forEach((eventName) => document.removeEventListener(eventName, onEnd, true))
        cleanupRipple = null
    }

    return cleanupRipple
}
