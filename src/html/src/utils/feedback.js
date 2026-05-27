import {overlay} from './overlay.js'

function scopeLoadingOverlay(overlayEl) {
  const contentWrapper = document.getElementById('content-wrapper')
  if (!overlayEl || !contentWrapper) return
  const rect = contentWrapper.getBoundingClientRect()
  const footer = document.getElementById('footer')
  const footerTop = footer ? footer.getBoundingClientRect().top : window.innerHeight
  const top = Math.max(rect.top, 0)
  const left = Math.max(rect.left, 0)
  const right = Math.min(rect.right, window.innerWidth)
  const bottom = Math.min(rect.bottom, footerTop)
  overlayEl.style.top = `${top}px`
  overlayEl.style.left = `${left}px`
  overlayEl.style.width = `${Math.max(0, right - left)}px`
  overlayEl.style.height = `${Math.max(0, bottom - top)}px`
  overlayEl.style.right = 'auto'
  overlayEl.style.bottom = 'auto'
}

export async function showLoadingOverlay(message) {
  const wrapper = document.createElement('div')
  wrapper.className = 'elrs-loading-overlay'
  wrapper.innerHTML = `
    <div class="elrs-loading-panel">
      <div class="loader elrs-loading-spinner"></div>
      <span class="elrs-loading-message">${message}</span>
    </div>
  `
  const overlayEl = overlay('on', {keyboard: false, static: true}, wrapper)
  overlayEl?.classList.add('elrs-loading-active')
  // scopeLoadingOverlay(overlayEl)
  await new Promise(requestAnimationFrame)
}

export function hideLoadingOverlay() {
  overlay('off')
}

export function showAlert(type, title, message) {
  return cuteAlert({type, title, message})
}

export function showConfirm(title, message, confirmText = 'OK', cancelText = 'Cancel') {
  return cuteAlert({type: 'question', title, message, confirmText, cancelText})
}

export async function loadJSON(url, errorMessage = 'Failed to load data', headers = {}) {
  const response = await fetch(url, {headers})
  if (!response.ok) {
    const text = await response.text()
    throw new Error(text || errorMessage)
  }
  return await response.json()
}

// Generic POST helper that can send JSON, FormData, or file uploads (no UI side-effects)
export function post(url, data, { headers = {}, timeoutMs = 0, onprogress, onload, onerror, onabort, ontimeout } = {}) {
  const xhr = new XMLHttpRequest()
  if (timeoutMs > 0) xhr.timeout = timeoutMs
  if (onprogress) xhr.upload.addEventListener('progress', onprogress, false)
  if (onerror) xhr.addEventListener('error', () => onerror(xhr), false)
  if (onabort) xhr.addEventListener('abort', () => onabort(xhr), false)
  if (ontimeout) xhr.addEventListener('timeout', () => ontimeout(xhr), false)
  xhr.onreadystatechange = function () {
    if (xhr.readyState === 4) {
      if (xhr.status === 200) {
        if (onload) onload(xhr)
      } else {
        if (onerror) onerror(xhr)
      }
    }
  }
  xhr.open('POST', url, true)
  if ((typeof File !== 'undefined') && (data instanceof File)) {
    xhr.setRequestHeader('X-FileSize', data.size)
    for (const [k, v] of Object.entries(headers)) {
      xhr.setRequestHeader(k, v)
    }
    const formdata = new FormData()
    formdata.append('upload', data, data.name)
    xhr.send(formdata)
    return xhr
  }
  const isFormData = (typeof FormData !== 'undefined') && (data instanceof FormData)
  if (!isFormData) {
    // default to JSON unless caller supplied explicit Content-Type
    xhr.setRequestHeader('Content-Type', headers['Content-Type'] || 'application/json')
  }
  for (const [k, v] of Object.entries(headers)) {
    if (k.toLowerCase() !== 'content-type') xhr.setRequestHeader(k, v)
  }
  const payload = isFormData ? data : (typeof data === 'string' ? data : JSON.stringify(data))
  xhr.send(payload)
  return xhr
}

// Helper to post JSON and then show reboot prompt on success
export function saveJSONWithReboot(title, errorTitle, url, changes, successCB) {
  post(url, changes, {
    onload: async () => {
      let message
      if (successCB) message = successCB()
      const res = await showConfirm(title, message || 'Reboot to take effect', 'Reboot', 'Close')
      if (res === 'confirm') {
        // fire-and-forget reboot
        const r = new XMLHttpRequest()
        r.open('POST', '/reboot')
        r.setRequestHeader('Content-Type', 'application/json')
        r.send()
      }
    },
    onerror: async (xhr) => {
      await showAlert('error', errorTitle, xhr.responseText || 'Request failed')
    }
  })
}

export function postWithFeedback(title, errorMsg, url, getdata, success) {
  return function (e) {
    e.stopPropagation()
    e.preventDefault()
    post(url, getdata ? getdata() : null, {
      onload: async (xhr) => {
        if (success) success()
        await showAlert('info', title, xhr.responseText)
      },
      onerror: async () => {
        await showAlert('error', title, errorMsg)
      }
    })
  }
}

// Alert box design by Igor Ferrão de Souza
export function cuteAlert({
  type,
  title,
  message,
  buttonText = 'OK',
  confirmText = 'OK',
  cancelText = 'Cancel',
  closeStyle,
}) {
  return new Promise((resolve) => {
    setInterval(() => {}, 5000)
    const body = document.querySelector('body')

    let closeStyleTemplate = 'alert-close'
    if (closeStyle === 'circle') {
      closeStyleTemplate = 'alert-close-circle'
    }

    let btnTemplate = `<button class="alert-button ${type}-bg ${type}-btn mui-btn mui-btn--primary">${buttonText}</button>`
    if (type === 'question') {
      btnTemplate = `
<div class="question-buttons">
  <button class="confirm-button error-bg error-btn mui-btn mui-btn--danger">${confirmText}</button>
  <button class="cancel-button question-bg question-btn mui-btn">${cancelText}</button>
</div>
`
    }

    let svgTemplate = `
<svg class="alert-img" xmlns="http://www.w3.org/2000/svg" fill="#fff" viewBox="0 0 52 52" xmlns:v="https://vecta.io/nano">
<path d="M26 0C11.664 0 0 11.663 0 26s11.664 26 26 26 26-11.663 26-26S40.336 0 26 0zm0 50C12.767 50 2 39.233 2 26S12.767 2 26 2s24 10.767 24 24-10.767 24-24
24zm9.707-33.707a1 1 0 0 0-1.414 0L26 24.586l-8.293-8.293a1 1 0 0 0-1.414 1.414L24.586 26l-8.293 8.293a1 1 0 0 0 0 1.414c.195.195.451.293.707.293s.512-.098.707
-.293L26 27.414l8.293 8.293c.195.195.451.293.707.293s.512-.098.707-.293a1 1 0 0 0 0-1.414L27.414 26l8.293-8.293a1 1 0 0 0 0-1.414z"/>
</svg>
`
    if (type === 'success') {
      svgTemplate = `
<svg class="alert-img" xmlns="http://www.w3.org/2000/svg" fill="#fff" viewBox="0 0 52 52" xmlns:v="https://vecta.io/nano">
<path d="M26 0C11.664 0 0 11.663 0 26s11.664 26 26 26 26-11.663 26-26S40.336 0 26 0zm0 50C12.767 50 2 39.233 2 26S12.767 2 26 2s24 10.767 24 24-10.767 24-24
24zm12.252-34.664l-15.369 17.29-9.259-7.407a1 1 0 0 0-1.249 1.562l10 8a1 1 0 0 0 1.373-.117l16-18a1 1 0 1 0-1.496-1.328z"/>
</svg>
`
    }
    if (type === 'info') {
      svgTemplate = `
<svg class="alert-img" xmlns="http://www.w3.org/2000/svg" fill="#fff" viewBox="0 0 64 64" xmlns:v="https://vecta.io/nano">
<path d="M38.535 47.606h-4.08V28.447a1 1 0 0 0-1-1h-4.52a1 1 0 1 0 0 2h3.52v18.159h-5.122a1 1 0 1 0 0 2h11.202a1 1 0 1 0 0-2z"/>
<circle cx="32" cy="18" r="3"/><path d="M32 0C14.327 0 0 14.327 0 32s14.327 32 32 32 32-14.327 32-32S49.673 0 32 0zm0 62C15.458 62 2 48.542 2 32S15.458 2 32 2s30 13.458 30 30-13.458 30-30 30z"/>
</svg>
`
    }

    const template = `
<div class="alert-wrapper">
  <div class="alert-frame">
    <div class="alert-header ${type}-bg">
      <span class="${closeStyleTemplate}">X</span>
      ${svgTemplate}
    </div>
    <div class="alert-body">
      <span class="alert-title">${title}</span>
      <span class="alert-message">${message}</span>
      ${btnTemplate}
    </div>
  </div>
</div>
`

    const host = document.createElement('div')
    host.innerHTML = template.trim()
    const alertWrapper = host.firstElementChild
    body.appendChild(alertWrapper)

    const alertFrame = alertWrapper.querySelector('.alert-frame')
    const alertClose = alertWrapper.querySelector(`.${closeStyleTemplate}`)

    function resolveIt() {
      alertWrapper.remove()
      resolve()
    }
    function confirmIt() {
      alertWrapper.remove()
      resolve('confirm')
    }
    function stopProp(e) {
      e.stopPropagation()
    }

    if (type === 'question') {
      const confirmButton = alertWrapper.querySelector('.confirm-button')
      const cancelButton = alertWrapper.querySelector('.cancel-button')

      confirmButton.addEventListener('click', confirmIt)
      cancelButton.addEventListener('click', resolveIt)
    } else {
      const alertButton = alertWrapper.querySelector('.alert-button')

      alertButton.addEventListener('click', resolveIt)
    }

    alertClose.addEventListener('click', resolveIt)
    alertWrapper.addEventListener('click', resolveIt)
    alertFrame.addEventListener('click', stopProp)
  })
}
