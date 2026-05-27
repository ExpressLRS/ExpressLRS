import {LitElement, html, svg} from 'lit'
import {customElement, query, state} from "lit/decorators.js"
import {initRipple} from './utils/ripple.js'
import {initMuiSelect} from './utils/select.js'
import {elrsState, formatBand} from './utils/state.js'
import {overlay} from './utils/overlay.js'
import './components/elrs-footer.js'

import './pages/info-panel.js'
import {hideLoadingOverlay, loadJSON, showConfirm, showLoadingOverlay} from "./utils/feedback.js"
import {_} from "./utils/libs.js"

@customElement('elrs-app')
export class App extends LitElement {
    static SETTINGS_LOAD_FAILED_MESSAGE = 'Failed to load settings. Retry or power cycle device.'

    @query("#main") accessor mainEl

    // Runtime references and UI constants
    removeRippleListeners = null
    removeSelectListeners = null
    sidedrawerClosePromise = null
    sideDrawer = null

    menu = svg`<svg width="40" height="40" viewBox="0 0 512 512"><path fill="none" stroke="currentColor" stroke-linecap="round" stroke-miterlimit="10" stroke-width="48" d="M88 152h336M88 256h336M88 360h336"/></svg>`

    // Track the currently rendered route so we can restore it when a page blocks navigation
    @state() accessor currentRoute = null

    // Core component setup and rendering
    constructor() {
        super()
        // Bind methods used as callbacks to preserve `this`
        this.renderRoute = this.renderRoute.bind(this)
        this.showSidedrawer = this.showSidedrawer.bind(this)
        this.hideSidedrawer = this.hideSidedrawer.bind(this)
    }

    createRenderRoot() {
        return this
    }

    render() {
        return html`
            <div id="sidedrawer" class="mui--no-user-select">
                <div id="sidedrawer-brand" class="mui--appbar-line-height elrs-brand">
                    <svg viewBox="0 0 512 512" width="64px" >
                        <path fill="#666" d="M237.6 14.8c47.2-5 96.3 10.3 131.9 41.8 5.3 4.9 12.5 9.4 13.4 17.2 1.3 8.8-6.8 17.5-15.8 16.4-8-.4-12.3-8.1-18.1-12.4-21-17.9-47.2-29.6-74.6-33.1-42.8-6-88.1 8.3-119.2 38.5-3.5 3.7-7.8 7.3-13.2 7.1-7.4.4-14.2-5.8-14.9-13.2-.4-4.3 1.1-8.8 4.1-11.9 28.2-28.4 66.5-46.7 106.4-50.4zm0 49c39-5.7 80.5 8.2 107.6 36.9 4.9 5.7 4.3 15.2-1.4 20.1-4.7 4.6-12.7 5.3-18.1 1.5-2.9-2.1-5.3-4.7-8-7-14.9-12.9-34-21-53.6-22.7-26.2-2.7-53.4 6.1-73 23.6-3.6 3.1-6.8 7.2-11.7 8.2-7.7 2.1-16.4-3.6-17.8-11.4-1-4.7.5-9.9 4-13.3 19.3-19.5 45-32.3 72-35.9zm8 48c23.8-3.2 48.8 5.7 65.2 23.1 4.9 5.5 4.5 14.9-1 19.9-5.1 5.5-14.7 5.7-20.1.5-6.3-6.2-14-11-22.6-13.1-16-4.2-33.9.5-45.8 12.1-3.7 4.1-9.7 5.8-15 4.2-4.7-1.4-8.6-5.3-10-9.9-1.3-4.9-.3-10.4 3.3-14.1 12.1-12.6 28.6-20.8 46-22.7zm-2.7 61c6.3-4.1 14.6-4.9 21.6-2.3 8.6 3 15.1 11.2 16.1 20.3 1 7.8-2.1 15.7-7.7 21.1l4.3 28.1h-16.7l-3.3-21.5c-.6 0-1.7.1-2.3.1l-3.3 21.4h-16.7l4.3-28.1c-4.5-4.4-7.6-10.4-7.9-16.8-.6-8.9 4.1-17.7 11.6-22.3m9.4 13.6c-5.6 2.5-6.1 11.1-.9 14.2 5 3.7 13-.5 12.8-6.7.4-5.9-6.7-10.4-11.9-7.5zM86 232c5.5-5.6 13.1-9.2 21-9.6 11.6-.8 23.2 5.5 29 15.6 5.6 9.4 5.8 21.8.3 31.4-2.5 4.6-6.4 8.2-10.1 11.8l-23.5 23.5c-7.6 7.7-12 18.4-11.9 29.2l.1 42.9c.1 13 8 23.8 13.2 35.2l32.5-32.5c10-9.8 26.6-11.4 38.4-3.8 12 7.2 17.9 23 13.5 36.3-1.8 6-5.6 11-10.1 15.2l-46.5 46.5c-5.3 5.4-11 10.5-15.1 16.9-4.3 6.4-7 13.8-8.6 21.3H91.5c2.2-15.7 9.5-30.6 20.7-41.8l54.5-54.5c3.1-2.9 6-6.3 6.5-10.6 1.2-6.4-2.4-13.2-8.3-16-5.7-3-13.2-1.4-17.6 3.2l-24 24c-6.5 6.3-12.5 13.5-21 17.2-11.3 5.4-24.2 3.1-36.3 3.6v-16.5c7.9-.2 15.8.4 23.6-.4L80 400.7c-3.9-8-5.8-16.9-5.7-25.8v-42c.1-14.6 6-29 16.2-39.5l28.9-29.1c4.4-4.4 5.6-11.6 2.8-17.1-2.7-5.7-9.2-9.3-15.4-8.4-3.8.4-7.1 2.4-9.7 5.1l-39.4 39.5c-8.3 8.4-13.9 19.5-15.8 31.2l-7.4 47a109.61 109.61 0 0 0-1.5 18.1v111c0 7.1.6 14.1 2.2 21H18.4c-1.4-6.9-1.9-14-1.9-21V381.8c-.4-16.2 3.5-32 5.7-47.9 2.4-13 3-26.5 8.3-38.8 3.8-9.1 9.5-17.3 16.5-24.3L86 232zm286.5 14.8c2.7-12.2 13.2-22.1 25.6-24 9.9-1.7 20.4 1.6 27.5 8.7l40.5 40.5c11.2 11.3 18.4 26.3 20.6 42.1l7.7 49.1c1.5 9.5 1.1 19.2 1.2 28.9v84c-.1 12 .6 24.1-1.9 36h-16.8c2.8-11.5 2.1-23.3 2.2-35v-84c0-9.6.4-19.3-1.2-28.8l-8-50.3c-2.1-12.5-8.8-24.1-18-32.7l-38-38c-5.5-5.5-15.3-5.8-20.8-.2-5.9 5.2-6.4 15.2-1 20.9 6.4 6.7 13.1 13.1 19.6 19.7 5.9 6.1 12.5 11.8 17 19 6.2 9.5 9.3 21 9.1 32.3l-.1 44.6c-.5 14.9-9.1 27.6-15.2 40.7 7.8.7 15.7.2 23.6.4v16.5c-10-.4-20.2 1.1-30-1.4-7.1-1.8-13.6-5.8-18.8-10.9l-33-33c-5.5-5.5-15.1-5.7-20.7-.2-6.2 5.4-6.4 15.7-.6 21.5l57 57.1c11.2 11.2 18.5 26.2 20.7 41.8H404c-2.2-11.3-7.6-21.9-15.7-30.1l-57.4-57.5c-10-10.1-11.5-27.2-3.5-39 7.6-12 23.7-17.4 36.9-12.5 8.2 2.6 13.8 9.5 19.7 15.3l24 24c5.2-11.4 13.1-22.2 13.2-35.2v-42.9c.1-10.8-4.3-21.6-11.9-29.2l-29-29.1c-7.3-7.7-10.3-18.9-7.8-29.1zm-215.6.4h198.2v16.5H157c-.2-5.5-.1-11-.1-16.5zm-5.4 33.6c11.5-2.8 24.3 4 28.4 15.1 3.3 8 1.8 17.7-3.6 24.5-6.2 8.1-17.7 11.5-27.3 8.1-10.2-3.3-17.5-13.8-16.8-24.5.2-11 8.6-21 19.3-23.2m1.9 16.7c-5.7 2.4-6.3 11.1-1.1 14.3 4.7 3.5 12.2.1 12.8-5.8 1-6.1-6.2-11.4-11.7-8.5zm53-17.3h99.1v66.1h-99.1v-66.1m16.6 16.5v33h66v-33h-66zm127.5-16.1c7.4-1.5 15.5.8 21.1 5.8 7 6.1 10 16.4 7.3 25.3-2.6 9.3-11.1 16.7-20.8 17.7-8.4 1.1-17.2-2.5-22.4-9.2-5.4-6.7-6.9-16.3-3.7-24.3 3-7.7 10.2-13.8 18.5-15.3m1.6 16.7c-5.7 1.9-7.1 10.3-2.3 14 4.3 4 12.1 1.4 13.3-4.3 1.9-6.1-5.1-12.4-11-9.7zm-215.2 48.2c11.4-6.6 25.1-8.9 38.1-6.7 13.6 2.3 26.2 9.7 34.9 20.4 11 13.2 15.5 31.5 11.9 48.3-4.6 23-24.5 42-47.8 45.3l-2.5-16.3c11.2-1.8 21.5-8.2 27.8-17.6 6.6-9.5 8.9-21.9 6-33.1-2.9-11.9-11.5-22.3-22.6-27.5-11.6-5.5-25.8-5.1-37 1.1-11.2 6-19.3 17.4-21.2 30l-16.3-2.5c2.5-17.1 13.4-32.8 28.7-41.4zm175.5 3.9c13.5-10.2 31.6-13.9 48-9.8 22.3 5.2 40.2 24.8 43.5 47.4l-16.3 2.5c-1.9-12.1-9.4-23.1-19.9-29.3-10.3-6.2-23.3-7.5-34.6-3.4-10.6 3.7-19.5 12-24 22.2-5 11.2-4.6 24.7 1.2 35.6 5.9 11.5 17.5 19.9 30.4 21.8l-2.4 16.2c-17-2.4-32.5-13.2-41.2-28-6.6-11-9.2-24.3-7.4-37 1.9-14.9 10.4-29.1 22.7-38.2z"/>
                        <path fill="#666" d="M247.8 395.8h16.5v16.5h-16.5c-.1-5.5-.1-11 0-16.5zm-49.6 66.1h115.6v33h66.1v16.2l-134.9.2c-28.7-.1-57.3 0-86 0-8.9-.2-17.9.4-26.8-.2v-16.2h66.1l-.1-33m16.5 16.5v16.5h82.6v-16.5h-82.6z"/>
                    </svg>
                    <span class="mui--text-headline elrs-brand-title">ExpressLRS&trade;</span>
                </div>
                <div class="mui-divider"></div>
                <ul>
                    <li>
                        <strong>General</strong>
                        <ul>
                            <li><a id="menu-info" href="#info"><span class="mui--align-middle icon--symbols icon--symbols--info"></span>Information</a></li>
                            <li><a id="menu-binding" href="#binding"><span class="mui--align-middle icon--symbols icon--symbols--bind"></span>Binding</a></li>
                            <li><a id="menu-options" href="#options"><span class="mui--align-middle icon--symbols icon--symbols--options"></span>Options</a></li>
                            <!-- FEATURE:IS_TX -->
                            ${elrsState.config['button-actions'] && elrsState.config['button-actions'].length !== 0 ? html`
                                <li><a id="menu-buttons" href="#buttons"><span class="mui--align-middle icon--symbols icon--symbols-buttons"></span>Buttons</a></li>
                            ` : ''}
                            <li><a id="menu-models" href="#models"><span class="mui--align-middle icon--symbols icon--symbols--settings"></span>Import/Export</a></li>
                            <!-- /FEATURE:IS_TX -->
                            <!-- FEATURE:NOT IS_TX -->
                            ${elrsState.config.pwm !== undefined ? html`
                            <li><a id="menu-connections" href="#connections"><span class="mui--align-middle icon--symbols icon--symbols--connections"></span>Connections</a></li>
                            ` : ''}
                            <li><a id="menu-serial" href="#serial"><span class="mui--align-middle icon--symbols icon--symbols--serial"></span>Serial</a></li>
                            <!-- /FEATURE:NOT IS_TX -->
                            <li><a id="menu-wifi" href="#wifi"><span class="mui--align-middle icon--symbols icon--symbols--wifi"></span>WiFi</a></li>
                            <li><a id="menu-update" href="#update"><span class="mui--align-middle icon--symbols icon--symbols--update"></span>Update</a></li>
                        </ul>
                    </li>
                    <li>
                        <strong>Advanced</strong>
                        <ul>
                            <li><a id="menu-hardware" href="#hardware"><span class="mui--align-middle icon--symbols icon--symbols--hardware"></span>Hardware Layout</a></li>
                            <!-- FEATURE:NOT IS_TX -->
                            ${elrsState.settings?.voltage_source_count > 0 ? html`
                                <li><a id="menu-voltage" href="#voltage"><span class="mui--align-middle icon--symbols icon--symbols--voltage"></span>Voltage Calibration</a></li>
                            ` : ''}
                            <!-- /FEATURE:NOT IS_TX -->
                            <li><a id="menu-cw" href="#cw"><span class="mui--align-middle icon--symbols icon--symbols--wave"></span>Continuous Wave</a></li>
                            <!-- FEATURE:HAS_LR1121 -->
                            <li><a id="menu-lr1121" href="#lr1121"><span class="mui--align-middle icon--symbols icon--symbols--lr1121"></span>LR1121 Firmware</a></li>
                            <!-- /FEATURE:HAS_LR1121 -->
                        </ul>
                    </li>
                </ul>
            </div>
            <header id="header" class="mui-appbar elrs-header">
                <div class="mui--appbar-height elrs-header-bar">
                    <div class="elrs-header-menu">
                        <a class="mui--align-middle sidedrawer-toggle mui--visible-xs-inline-block mui--visible-sm-inline-block js-show-sidedrawer"
                           @click="${this.showSidedrawer}">${this.menu}</a>
                        <a class="mui--align-middle sidedrawer-toggle mui--hidden-xs mui--hidden-sm js-hide-sidedrawer"
                           @click="${this.hideSidedrawer}">${this.menu}</a>
                    </div>
                    <div class="elrs-header-meta">
                        <div id="product_name">${elrsState.settings?.product_name}</div>
                        <div>
                            <b>Firmware Rev. </b>${elrsState.settings?.version} ${formatBand()}
                        </div>
                    </div>
                </div>
            </header>
            <div id="content-wrapper">
                <div class="mui--appbar-height"></div>
                <div id="main" class="mui-container-fluid">${this.buildRouteContent(this.currentRoute)}</div>
            </div>
            <elrs-footer></elrs-footer>
        `
    }

    // Lifecycle wiring and teardown
    firstUpdated(_changedProperties) {
        this.removeRippleListeners = initRipple()
        this.removeSelectListeners = initMuiSelect()
        this.sideDrawer = _('sidedrawer')
        window.addEventListener('hashchange', this.renderRoute)

        // Initial load sequence
        this.initializeApp()
    }

    disconnectedCallback() {
        this.removeRippleListeners?.()
        this.removeRippleListeners = null
        this.removeSelectListeners?.()
        this.removeSelectListeners = null
        window.removeEventListener('hashchange', this.renderRoute)
        super.disconnectedCallback()
    }

    // Initial data loading and retry flow
    async initializeApp() {
        const loaded = await this.runWithSettingsRetry('Loading settings...', () => this.loadInitialData())
        if (loaded) {
            this.renderRoute()
        }
    }

    async runWithSettingsRetry(loadingMessage, operation) {
        while (true) {
            await showLoadingOverlay(loadingMessage)
            try {
                return await operation()
            } catch (error) {
                hideLoadingOverlay()
                const result = await showConfirm('Settings Load Failed', App.SETTINGS_LOAD_FAILED_MESSAGE, 'Retry', 'Close')
                if (result !== 'confirm') {
                    return false
                }
            } finally {
                hideLoadingOverlay()
            }
        }
    }

    async loadInitialData() {
        const data = await loadJSON('/config', 'Failed to load config')
        elrsState.settings = data.settings || {}
        elrsState.options = data.options || {}
        elrsState.config = data.config || {}
        document.title = 'ExpressLRS ' + data.settings["module-type"] + ' WebUI'
        this.requestUpdate()
        return true
    }

    // UI utilities and drawer DOM helpers
    scrollMainToTop() {
        const doScroll = (behavior = 'smooth') => {
            try {
                window.scrollTo({top: 0, left: 0, behavior})
            } catch {
                window.scrollTo(0, 0)
            }
        }
        requestAnimationFrame(() => requestAnimationFrame(() => doScroll('smooth')))
    }

    getOverlayElement() {
        return document.getElementById('mui-overlay')
    }

    teardownSidedrawer() {
        if (this.sideDrawer && !this.contains(this.sideDrawer)) {
            this.appendChild(this.sideDrawer)
        }
        this.sideDrawer?.classList.remove('active')
    }

    setActiveMenu(route) {
        if (this.sideDrawer) {
            const links = this.sideDrawer.querySelectorAll('a[href^="#"]')
            links.forEach(a => a.classList.remove('active'))
        }
        const id = 'menu-' +route
        const el = id ? (this.querySelector(`#${id}`) || document.getElementById(id)) : null
        if (el) el.classList.add('active')
    }

    // Route content rendering and lazy-loading
    buildRouteContent(route) {
        switch (route) {
            case 'info':
                return html`<info-panel></info-panel>`
            case 'binding':
                return html`<binding-panel></binding-panel>`
            case 'options':
            // FEATURE:IS_TX
                return html`<tx-options-panel></tx-options-panel>`
            // /FEATURE:IS_TX
            // FEATURE:NOT IS_TX
                return html`<rx-options-panel></rx-options-panel>`
            // /FEATURE:NOT IS_TX
            case 'wifi':
                return html`<wifi-panel></wifi-panel>`
            case 'update':
                return html`<update-panel></update-panel>`
            // FEATURE:NOT IS_TX
            case 'connections':
                return elrsState.config.pwm !== undefined ? html`<connections-panel></connections-panel>` : null
            case 'serial':
                return html`<serial-panel></serial-panel>`
            case 'voltage':
                return elrsState.settings?.voltage_source_count > 0 ? html`<voltage-calibration-panel></voltage-calibration-panel>` : null
            // /FEATURE:NOT IS_TX
            // FEATURE:IS_TX
            case 'buttons':
                return html`<buttons-panel></buttons-panel>`
            // /FEATURE:IS_TX
            case 'hardware':
                return html`<hardware-layout></hardware-layout>`
            case 'cw':
                return html`<continuous-wave></continuous-wave>`
            case 'models':
                return html`<models-panel></models-panel>`
            // FEATURE:HAS_LR1121
            case 'lr1121':
                return html`<lr1121-updater></lr1121-updater>`
            // /FEATURE:HAS_LR1121
            default:
                return null
        }
    }

    generalGroupLoaded = false
    advancedGroupLoaded = false

    loadGeneralGroup() {
        if (this.generalGroupLoaded) return Promise.resolve()
        return showLoadingOverlay('Loading...')
            .then(() => import('./page-groups/general-group.js'))
            .finally(() => {
                hideLoadingOverlay()
                this.generalGroupLoaded = true
            })
    }

    loadAdvancedGroup() {
        if (this.advancedGroupLoaded) return Promise.resolve()
        return showLoadingOverlay('Loading...')
            .then(() => import('./page-groups/advanced-group.js'))
            .finally(() => {
                hideLoadingOverlay()
                this.advancedGroupLoaded = true
            })
    }

    ensureLoadedForRoute(route) {
        if (['binding', 'options', 'wifi', 'update', 'connections', 'serial', 'buttons', 'models'].includes(route)) {
            return this.loadGeneralGroup()
        }
        if (['hardware', 'voltage', 'cw', 'lr1121'].includes(route)) {
            return this.loadAdvancedGroup()
        }
        return Promise.resolve()
    }

    // Transition/animation timing helpers
    waitForElementTransition(element, mutate, fallbackMs = 220) {
        return new Promise(resolve => {
            if (!element) {
                resolve()
                return
            }

            let settled = false
            let fallbackTimer = null
            const finish = () => {
                if (settled) return
                settled = true
                element.removeEventListener('transitionend', onEnd)
                if (fallbackTimer !== null) {
                    clearTimeout(fallbackTimer)
                    fallbackTimer = null
                }
                resolve()
            }

            const onEnd = (event) => {
                if (event?.target !== element) return
                finish()
            }

            element.addEventListener('transitionend', onEnd)
            try {
                mutate?.()
            } catch {
                finish()
                return
            }
            fallbackTimer = setTimeout(finish, fallbackMs)
        })
    }

    animateMainIn() {
        return new Promise(resolve => {
            if (!this.mainEl) {
                resolve()
                return
            }

            this.mainEl.classList.add('route-fade-in')
            requestAnimationFrame(() => {
                this.mainEl.classList.remove('route-fade-out')
                requestAnimationFrame(() => {
                    this.mainEl.classList.remove('route-fade-in')
                    resolve()
                })
            })
        })
    }

    // Route navigation orchestration
    renderRoute() {
        const route = (location.hash || '#info').replace('#', '')
        if (this.currentRoute && route === this.currentRoute) {
            this.setActiveMenu(route)
            return Promise.resolve()
        }

        const checkNavGuard = () => {
            const currentEl = this.mainEl?.firstElementChild
            if (!currentEl?.checkChanged?.()) return Promise.resolve(true)
            return showConfirm('Configuration Changed', 'Do you wish to navigate away and discard changes to this page?', 'Discard', 'Cancel')
                .then(r => r === 'confirm')
                .catch(() => true)
        }

        return checkNavGuard().then(canNavigate => {
            if (!canNavigate) {
                const hash = '#' + this.currentRoute
                if (this.currentRoute && this.currentRoute !== route && hash !== location.hash) {
                    location.hash = hash
                }
                this.setActiveMenu(this.currentRoute || route)
                return
            }

            this.setActiveMenu(route)
            return this.closeSidedrawer().then(() => this.ensureLoadedForRoute(route)).then(() => {
                let rendered = false
                return this.runWithSettingsRetry('Loading panel data...', () => {
                    return (rendered || !this.currentRoute
                        ? Promise.resolve()
                        : this.waitForElementTransition(this.mainEl, () => this.mainEl.classList.add('route-fade-out')))
                        .then(() => rendered || (this.currentRoute = route) && this.updateComplete)
                        .then(() => rendered || (rendered = true) && this.animateMainIn())
                        .then(() => this.mainEl?.firstElementChild?.pageReady?.())
                        .then(() => {
                            this.scrollMainToTop()
                            return true
                        })
                })
            })
        })
    }

    // Sidedrawer interactions (mobile and desktop toggle)
    showSidedrawer() {
        if (this.sidedrawerClosePromise) {
            this.sidedrawerClosePromise.then(() => this.showSidedrawer())
            return
        }

        if (this.getOverlayElement()) return

        if (!this.sideDrawer) return

        // Ensure known closed baseline before animating in
        this.sideDrawer.classList.remove('active')

        const options = {
            static: true,
            keyboard: false,
            onclose: () => {
                this.teardownSidedrawer()
            }
        }

        const overlayEl = overlay('on', options)
        overlayEl.appendChild(this.sideDrawer)

        overlayEl.addEventListener('click', (event) => {
            if (event.target === overlayEl) {
                this.closeSidedrawer()
            }
        })

        requestAnimationFrame(() => requestAnimationFrame(() => this.sideDrawer.classList.add('active')))
    }

    closeSidedrawer() {
        if (this.sidedrawerClosePromise) {
            return this.sidedrawerClosePromise
        }

        const overlayEl = this.getOverlayElement()
        document.body.classList.remove('hide-sidedrawer')

        // If no overlay, ensure drawer is reset and return
        if (!overlayEl) {
            this.teardownSidedrawer()
            return Promise.resolve()
        }

        this.sidedrawerClosePromise = this.waitForElementTransition(this.sideDrawer, () => {
            this.sideDrawer?.classList.remove('active')
        }).then(() => {
            overlay('off')
        }).finally(() => {
            this.sidedrawerClosePromise = null
        })

        return this.sidedrawerClosePromise
    }

    hideSidedrawer() {
        document.body.classList.toggle('hide-sidedrawer')
    }
}
