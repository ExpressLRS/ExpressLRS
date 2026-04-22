const WRAPPER_CLASS = 'mui-select'
const MENU_CLASS = 'mui-select__menu'
const SELECTED_CLASS = 'mui--is-selected'
const DISABLED_CLASS = 'mui--is-disabled'

let activeMenu = null
let selectObserver = null
let cleanupSelect = null

function isTouchDevice() {
    return 'ontouchstart' in document.documentElement
}

function dispatchSimpleEvent(element, type, bubbles = false) {
    element?.dispatchEvent(new Event(type, {bubbles}))
}

function getDirectSelect(wrapper) {
    for (const child of wrapper.children) {
        if (child.tagName === 'SELECT') return child
    }
    return null
}

function syncWrapperState(wrapper) {
    const selectEl = wrapper._selectEl || getDirectSelect(wrapper)
    if (!selectEl) return

    wrapper._selectEl = selectEl
    selectEl.tabIndex = -1

    if (selectEl.disabled) wrapper.removeAttribute('tabindex')
    else wrapper.tabIndex = 0
}

function getMenuPosition(wrapperEl, menuEl, selectedRow) {
    const viewHeight = document.documentElement.clientHeight
    const numRows = menuEl.children.length || 1
    const heightPx = Math.min(menuEl.offsetHeight, viewHeight)
    const padding = Number.parseInt(getComputedStyle(menuEl).paddingTop, 10) || 0
    const rowHeight = (menuEl.offsetHeight - 2 * padding) / numRows

    const initTop = -selectedRow * rowHeight
    const minTop = -wrapperEl.getBoundingClientRect().top
    const maxTop = (viewHeight - heightPx) + minTop
    const top = Math.min(Math.max(initTop, minTop), maxTop)

    let scrollTop = 0
    if (menuEl.offsetHeight > viewHeight) {
        const scrollIdeal = top + padding + selectedRow * rowHeight
        const scrollMax = numRows * rowHeight + 2 * padding - heightPx
        scrollTop = Math.min(scrollIdeal, scrollMax)
    }

    return {
        height: `${heightPx}px`,
        top: `${top}px`,
        scrollTop,
    }
}

function destroyActiveMenu() {
    activeMenu?.destroy()
}

class SelectMenu {
    constructor(wrapperEl, selectEl) {
        this.wrapperEl = wrapperEl
        this.selectEl = selectEl
        this.itemArray = []
        this.origPos = -1
        this.currentPos = 0

        const [menuEl, selectedRow] = this.createMenuEl()
        this.menuEl = menuEl

        wrapperEl.appendChild(menuEl)
        const props = getMenuPosition(wrapperEl, menuEl, selectedRow)
        menuEl.style.height = props.height
        menuEl.style.top = props.top
        menuEl.scrollTop = props.scrollTop

        this.onMenuClick = this.onMenuClick.bind(this)
        this.onWindowResize = this.destroy.bind(this)
        this.onDocumentClick = this.onDocumentClick.bind(this)

        menuEl.addEventListener('click', this.onMenuClick)
        window.addEventListener('resize', this.onWindowResize)
        setTimeout(() => document.addEventListener('click', this.onDocumentClick), 0)

        activeMenu = this
    }

    createMenuEl() {
        const menuEl = document.createElement('div')
        menuEl.className = MENU_CLASS

        const fragment = document.createDocumentFragment()
        const childEls = this.selectEl.children
        let itemPos = 0
        let selectedRow = 0
        let numRows = 0

        for (const child of childEls) {
            const optionEls = child.tagName === 'OPTGROUP' ? child.children : [child]

            if (child.tagName === 'OPTGROUP') {
                const labelEl = document.createElement('div')
                labelEl.textContent = child.label
                labelEl.className = 'mui-optgroup__label'
                fragment.appendChild(labelEl)
                numRows += 1
            }

            for (const optionEl of optionEls) {
                if (optionEl.hidden) continue

                const rowEl = document.createElement('div')
                rowEl.textContent = optionEl.textContent

                if (child.tagName === 'OPTGROUP') {
                    rowEl.classList.add('mui-optgroup__option')
                }

                if (optionEl.disabled) {
                    rowEl.classList.add(DISABLED_CLASS)
                } else {
                    rowEl._muiIndex = optionEl.index
                    rowEl._muiPos = itemPos
                    this.itemArray.push(rowEl)

                    if (optionEl.selected) {
                        this.origPos = itemPos
                        this.currentPos = itemPos
                        selectedRow = numRows
                    }
                    itemPos += 1
                }

                fragment.appendChild(rowEl)
                numRows += 1
            }
        }

        if (this.itemArray.length) {
            this.itemArray[this.currentPos]?.classList.add(SELECTED_CLASS)
        }

        menuEl.appendChild(fragment)
        return [menuEl, selectedRow]
    }

    onMenuClick(event) {
        event.stopPropagation()
        const item = event.target.closest('div')
        if (item?._muiIndex === undefined) return

        this.currentPos = item._muiPos
        this.selectCurrent()
        this.destroy()
    }

    onDocumentClick(event) {
        if (!this.wrapperEl.contains(event.target)) {
            this.destroy()
        }
    }

    increment() {
        if (this.currentPos >= this.itemArray.length - 1) return
        this.itemArray[this.currentPos]?.classList.remove(SELECTED_CLASS)
        this.currentPos += 1
        this.itemArray[this.currentPos]?.classList.add(SELECTED_CLASS)
    }

    decrement() {
        if (this.currentPos <= 0) return
        this.itemArray[this.currentPos]?.classList.remove(SELECTED_CLASS)
        this.currentPos -= 1
        this.itemArray[this.currentPos]?.classList.add(SELECTED_CLASS)
    }

    selectCurrent() {
        if (this.currentPos === this.origPos) return
        this.selectEl.selectedIndex = this.itemArray[this.currentPos]._muiIndex
        dispatchSimpleEvent(this.selectEl, 'change', true)
        dispatchSimpleEvent(this.selectEl, 'input', true)
    }

    selectPos(pos) {
        if (pos < 0 || pos >= this.itemArray.length) return
        this.itemArray[this.currentPos]?.classList.remove(SELECTED_CLASS)
        this.currentPos = pos
        const itemEl = this.itemArray[pos]
        itemEl?.classList.add(SELECTED_CLASS)

        if (!itemEl) return
        const itemRect = itemEl.getBoundingClientRect()
        if (itemRect.top < 0) {
            this.menuEl.scrollTop = this.menuEl.scrollTop + itemRect.top - 5
        } else if (itemRect.bottom > window.innerHeight) {
            this.menuEl.scrollTop = this.menuEl.scrollTop + (itemRect.bottom - window.innerHeight) + 5
        }
    }

    destroy() {
        this.menuEl.removeEventListener('click', this.onMenuClick)
        window.removeEventListener('resize', this.onWindowResize)
        document.removeEventListener('click', this.onDocumentClick)

        this.menuEl.parentNode?.removeChild(this.menuEl)
        this.wrapperEl._menu = null
        this.wrapperEl.focus()

        if (activeMenu === this) {
            activeMenu = null
        }
    }
}

function renderMenu(wrapperEl) {
    if (wrapperEl._menu || wrapperEl._selectEl?.disabled) return
    if (activeMenu && activeMenu.wrapperEl !== wrapperEl) {
        activeMenu.destroy()
    }
    wrapperEl._menu = new SelectMenu(wrapperEl, wrapperEl._selectEl)
}

function onInnerMouseDown(event) {
    if (event.button !== 0) return
    event.preventDefault()
}

function onWrapperFocus(event) {
    dispatchSimpleEvent(event.currentTarget._selectEl, 'focus')
}

function onWrapperBlur(event) {
    dispatchSimpleEvent(event.currentTarget._selectEl, 'blur')
}

function onWrapperClick(event) {
    const wrapperEl = event.currentTarget
    if (wrapperEl._selectEl?.disabled) return
    wrapperEl.focus()
    renderMenu(wrapperEl)
}

function onWrapperKeyDown(event) {
    if (event.defaultPrevented) return

    const wrapperEl = event.currentTarget
    const menu = wrapperEl._menu
    const keyCode = event.keyCode

    if (!menu) {
        if (keyCode === 32 || keyCode === 38 || keyCode === 40) {
            event.preventDefault()
            renderMenu(wrapperEl)
        }
        return
    }

    if (keyCode === 9) {
        menu.destroy()
        return
    }

    if (keyCode === 27 || keyCode === 38 || keyCode === 40 || keyCode === 13) {
        event.preventDefault()
    }

    if (keyCode === 27) menu.destroy()
    else if (keyCode === 40) menu.increment()
    else if (keyCode === 38) menu.decrement()
    else if (keyCode === 13) {
        menu.selectCurrent()
        menu.destroy()
    }
}

function onWrapperKeyPress(event) {
    const wrapperEl = event.currentTarget
    const menu = wrapperEl._menu
    if (event.defaultPrevented || !menu) return

    clearTimeout(wrapperEl._qTimeout)
    wrapperEl._q += event.key
    wrapperEl._qTimeout = setTimeout(() => {
        wrapperEl._q = ''
    }, 300)

    const prefixRegex = new RegExp(`^${wrapperEl._q}`, 'i')
    for (const item of menu.itemArray) {
        if (prefixRegex.test(item.innerText)) {
            menu.selectPos(item._muiPos)
            break
        }
    }
}

function setupWrapper(wrapperEl) {
    if (wrapperEl._muiSelectSetup || wrapperEl.classList.contains('mui-select--use-default')) return

    const selectEl = getDirectSelect(wrapperEl)
    if (!selectEl) return

    wrapperEl._muiSelectSetup = true
    wrapperEl._selectEl = selectEl
    wrapperEl._menu = null
    wrapperEl._q = ''
    wrapperEl._qTimeout = null

    syncWrapperState(wrapperEl)
    selectEl.addEventListener('mousedown', onInnerMouseDown)
    wrapperEl.addEventListener('focus', onWrapperFocus)
    wrapperEl.addEventListener('blur', onWrapperBlur)
    wrapperEl.addEventListener('click', onWrapperClick)
    wrapperEl.addEventListener('keydown', onWrapperKeyDown)
    wrapperEl.addEventListener('keypress', onWrapperKeyPress)
}

function scanForSelects(root) {
    if (!root || root.nodeType !== Node.ELEMENT_NODE) return

    if (root.matches?.('.mui-select')) {
        setupWrapper(root)
    }

    root.querySelectorAll?.('.mui-select').forEach(setupWrapper)
}

export function initMuiSelect() {
    if (cleanupSelect || isTouchDevice()) {
        return cleanupSelect || (() => {})
    }

    scanForSelects(document.body)

    selectObserver = new MutationObserver((mutations) => {
        for (const mutation of mutations) {
            if (mutation.type === 'childList') {
                mutation.addedNodes.forEach(scanForSelects)
            } else if (mutation.type === 'attributes' && mutation.target.tagName === 'SELECT') {
                const wrapperEl = mutation.target.parentElement
                if (wrapperEl?.classList.contains(WRAPPER_CLASS)) {
                    syncWrapperState(wrapperEl)
                }
            }
        }
    })

    selectObserver.observe(document.body, {
        subtree: true,
        childList: true,
        attributes: true,
        attributeFilter: ['disabled'],
    })

    cleanupSelect = () => {
        destroyActiveMenu()
        selectObserver?.disconnect()
        selectObserver = null
        cleanupSelect = null
    }

    return cleanupSelect
}
