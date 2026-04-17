import {html, LitElement} from "lit"
import {customElement, state} from "lit/decorators.js"
import '../assets/mui.js'
import {errorAlert, postJSON} from '../utils/feedback.js'

@customElement('model-settings-configurator')
class ModelSettingsConfigurator extends LitElement {
    @state() accessor configData = null
    @state() accessor loading = false
    @state() accessor saving = false
    @state() accessor valueDrafts = {}
    @state() accessor modelId = 0

    pendingSave = Promise.resolve()
    readRequestSeq = 0

    createRenderRoot() {
        return this
    }

    firstUpdated() {
        this.readConfig(false)
    }

    async readConfig(applyModelId = false) {
        const requestSeq = ++this.readRequestSeq
        this.loading = true
        this.configData = null
        this.valueDrafts = {}
        try {
            const query = applyModelId ? `?modelId=${this.modelId}` : ''
            const sep = query ? '&' : '?'
            const resp = await fetch(`/lua-parameters${query}${sep}_ts=${Date.now()}`, {cache: 'no-store'})
            if (!resp.ok) throw new Error('Failed to fetch Lua parameters')
            const nextConfigData = await resp.json()

            // Ignore stale responses from older requests when model switches quickly.
            if (requestSeq !== this.readRequestSeq) {
                return
            }

            this.configData = nextConfigData
            if (nextConfigData?.modelId !== undefined) {
                const mid = Number(nextConfigData.modelId)
                if (!Number.isNaN(mid)) {
                    this.modelId = Math.max(0, Math.min(63, Math.trunc(mid)))
                }
            }
            this.valueDrafts = this._buildDrafts(nextConfigData)
        } catch (e) {
            if (requestSeq !== this.readRequestSeq) {
                return
            }
            await errorAlert('Load Failed', `Failed to load configuration: ${e.message}`)
        } finally {
            if (requestSeq === this.readRequestSeq) {
                this.loading = false
            }
        }
    }

    _postJSON(url, data) {
        return new Promise((resolve, reject) => {
            postJSON(url, data, {
                onload: (xhr) => {
                    try {
                        resolve(JSON.parse(xhr.responseText || '{}'))
                    } catch {
                        resolve({})
                    }
                },
                onerror: (xhr) => reject(new Error(xhr.responseText || 'Request failed')),
            })
        })
    }

    _queueSingleSave(id, value) {
        const targetModelId = this.modelId
        this.pendingSave = this.pendingSave.then(async () => {
            this.saving = true
            try {
                await this._postJSON('/lua-parameters/save', {
                    modelId: targetModelId,
                    changes: [{id: Number(id), value}],
                })
                if (this.modelId === targetModelId) {
                    await this.readConfig(true)
                }
            } finally {
                this.saving = false
            }
        }).catch(async (e) => {
            this.saving = false
            await errorAlert('Save Failed', e?.message || 'Failed to save parameter')
        })
    }

    render() {
        return html`
            <div class="mui-panel mui--text-title">Model Settings</div>
            <div class="mui-panel">
                <div style="display: flex; align-items: end; gap: 12px; flex-wrap: wrap;">
                    <div style="display: flex; align-items: center; gap: 8px;">
                        <span class="mui--text-title" style="margin: 0;">Model ID</span>
                        <div class="mui-textfield" style="margin: 0; width: 88px;">
                            <input
                                type="number"
                                .value=${String(this.modelId)}
                                min="0"
                                max="63"
                                step="1"
                                @change=${(e) => this._onModelIdChange(e)}
                            />
                        </div>
                        <button
                            class="mui-btn mui-btn--small mui-btn--primary"
                            ?disabled=${this.loading || this.saving}
                            @click=${() => this.readConfig(true)}
                        >
                            Refresh
                        </button>
                    </div>
                    ${(this.loading || this.saving)
                        ? html`<span>${this.saving ? 'Saving...' : 'Loading...'}</span>`
                        : ''}
                </div>
            </div>
            ${this.configData ? this._renderConfigData() : ''}
        `
    }

    _renderConfigData() {
        const allParams = Array.isArray(this.configData?.parameters) ? this.configData.parameters : []
        const params = this._filterVisibleParams(allParams)
        if (!params.length) {
            return html`
                <div class="mui-panel">
                    <p>No visible Lua parameters returned by device.</p>
                </div>
            `
        }

        const childrenByParent = this._groupByParent(params)
        const rootNodes = this._getChildren(childrenByParent, 0)

        return html`
            <div class="mui-panel">
                ${rootNodes.length
                    ? rootNodes.map((node) => this._renderNode(node, childrenByParent, 0))
                    : html`<p>No root parameters found.</p>`}
            </div>
        `
    }

    _renderNode(node, childrenByParent, depth) {
        const children = this._getChildren(childrenByParent, node.id)
        const indent = `margin-left: ${depth * 14}px;`

        if (node.type === 'folder') {
            return html`
                <div style="${indent} margin: 12px 0 8px 0;">
                    <div class="mui--text-subhead"><b>${node.name || `Folder ${node.id}`}</b></div>
                    <div class="mui-divider" style="margin: 4px 0 10px 0;"></div>
                    ${children.length
                        ? children.map((child) => this._renderNode(child, childrenByParent, depth + 1))
                        : html`<p style="color: #888; margin-left: 8px;">(empty folder)</p>`}
                </div>
            `
        }

        return this._renderParamRow(node, indent)
    }

    _renderParamRow(param, indent) {
        return html`
            <div style="${indent} margin: 0 0 10px 0;">
                <table class="mui-table">
                    <tbody>
                        <tr>
                            <td style="width: 34%; vertical-align: middle;"><b>${param.name || '(unnamed)'}</b></td>
                            <td style="vertical-align: middle;">
                                ${this._renderControlByType(param)}
                            </td>
                        </tr>
                    </tbody>
                </table>
            </div>
        `
    }

    _renderControlByType(param) {
        const draft = this.valueDrafts[param.id]

        if (param.type === 'selection') {
            const options = Array.isArray(param.options) ? param.options : []
            const selectedRaw = draft !== undefined ? Number(draft) : Number(param.value ?? 0)
            const visibleOptions = options
                .map((label, rawIndex) => ({label, rawIndex}))
                .filter((opt) => {
                    const label = String(opt.label ?? '').trim()
                    return label.length > 0 && label !== '(reserved)'
                })

            // Keep the backend raw index semantics while hiding blank UI options.
            const hasSelected = visibleOptions.some((opt) => opt.rawIndex === selectedRaw)
            const effectiveSelectedRaw = hasSelected
                ? selectedRaw
                : (visibleOptions[0]?.rawIndex ?? selectedRaw)

            return html`
                <div style="display: flex; align-items: center; gap: 8px;">
                    <div class="mui-select" style="margin: 0; flex: 1;">
                        <select style="font-size: 14px;" @change=${(e) => this._onParamChange(param, Number(e.target.value))}>
                            ${visibleOptions.map((opt) => html`<option value=${opt.rawIndex} ?selected=${effectiveSelectedRaw === opt.rawIndex}>${opt.label}</option>`) }
                        </select>
                    </div>
                    ${param.units ? html`<span style="font-size: 14px; color: #000;">${param.units}</span>` : ''}
                </div>
            `
        }

        if (param.type === 'uint8' || param.type === 'int8' || param.type === 'uint16' || param.type === 'int16') {
            const value = draft !== undefined ? draft : (param.value ?? 0)
            return html`
                <div style="display: flex; align-items: center; gap: 8px;">
                    <div class="mui-textfield" style="margin: 0; flex: 1;">
                        <input
                            type="number"
                            .value=${String(value)}
                            min=${param.min ?? ''}
                            max=${param.max ?? ''}
                            step="1"
                            @change=${(e) => this._onParamChange(param, Number(e.target.value))}
                        />
                    </div>
                    ${param.units ? html`<span style="font-size: 14px; color: #000;">${param.units}</span>` : ''}
                </div>
            `
        }

        if (param.type === 'float') {
            const step = param.step ?? (param.precision ? (1 / (10 ** param.precision)) : 0.01)
            const value = draft !== undefined ? draft : (param.value ?? 0)
            return html`
                <div style="display: flex; align-items: center; gap: 8px;">
                    <div class="mui-textfield" style="margin: 0; flex: 1;">
                        <input
                            type="number"
                            .value=${String(value)}
                            min=${param.min ?? ''}
                            max=${param.max ?? ''}
                            step=${step}
                            @change=${(e) => this._onParamChange(param, Number(e.target.value))}
                        />
                    </div>
                    ${param.units ? html`<span style="font-size: 14px; color: #000;">${param.units}</span>` : ''}
                </div>
            `
        }

        if (param.type === 'string') {
            const value = draft !== undefined ? draft : (param.value ?? '')
            return html`
                <div style="display: flex; align-items: center; gap: 8px;">
                    <div class="mui-textfield" style="margin: 0; flex: 1;">
                        <input
                            type="text"
                            .value=${String(value)}
                            disabled
                        />
                    </div>
                    ${param.units ? html`<span style="font-size: 14px; color: #000;">${param.units}</span>` : ''}
                </div>
            `
        }

        return html`<code>${this._formatValue(this.valueDrafts[param.id] ?? param.value)}</code>`
    }

    _groupByParent(params) {
        const map = {}
        params.forEach((param) => {
            const parent = Number(param.parent ?? 0)
            if (!map[parent]) {
                map[parent] = []
            }
            map[parent].push(param)
        })

        Object.keys(map).forEach((key) => {
            map[key].sort((a, b) => Number(a.id || 0) - Number(b.id || 0))
        })

        return map
    }

    _filterVisibleParams(allParams) {
        const base = allParams.filter((p) => p?.type !== 'info' && p?.type !== 'command')
        const hiddenFolderIds = new Set(
            base
                .filter((p) => p?.type === 'folder' && typeof p?.name === 'string' && /wifi\s*connectivity/i.test(p.name))
                .map((p) => Number(p.id))
        )

        if (hiddenFolderIds.size === 0) {
            return base
        }

        let expanded = true
        while (expanded) {
            expanded = false
            for (const p of base) {
                const pid = Number(p?.id)
                const parent = Number(p?.parent)
                if (!hiddenFolderIds.has(pid) && hiddenFolderIds.has(parent)) {
                    hiddenFolderIds.add(pid)
                    expanded = true
                }
            }
        }

        return base.filter((p) => !hiddenFolderIds.has(Number(p?.id)))
    }

    _getChildren(childrenByParent, parentId) {
        return childrenByParent[parentId] || []
    }

    _buildDrafts(payload) {
        const drafts = {}
        const params = Array.isArray(payload?.parameters) ? payload.parameters : []
        params.forEach((param) => {
            if (param.value !== undefined) {
                drafts[param.id] = param.value
            }
        })
        return drafts
    }

    _onDraftChange(id, value) {
        this.valueDrafts = {
            ...this.valueDrafts,
            [id]: value
        }
    }

    _onParamChange(param, value) {
        this._onDraftChange(param.id, value)
        this._queueSingleSave(param.id, value)
    }

    async _onModelIdChange(e) {
        const raw = Number(e.target.value)
        let next = 0
        if (Number.isNaN(raw)) {
            next = 0
        } else {
            next = Math.max(0, Math.min(63, Math.trunc(raw)))
        }

        if (next === this.modelId) {
            return
        }

        this.modelId = next
        await this.readConfig(true)
    }

    _formatValue(value) {
        if (value === null || value === undefined) {
            return html`<span style="color: #999;">null</span>`
        }
        if (typeof value === 'boolean') {
            return value ? html`<span style="color: green;">true</span>` : html`<span style="color: red;">false</span>`
        }
        if (typeof value === 'number') {
            return html`<span style="color: #0066cc;">${value}</span>`
        }
        return String(value)
    }
}
