-- TNS|ExpressLRS LVGL|TNE
---- #########################################################################
---- #                                                                       #
---- # Copyright (C) OpenTX, adapted for ExpressLRS                          #
-----#                                                                       #
---- # License GPLv2: http://www.gnu.org/licenses/gpl-2.0.html               #
---- #                                                                       #
---- # LVGL version for EdgeTX 2.11.4+                                       #
---- #########################################################################
local VERSION = "r1 LVGL"

-- ============================================================================
-- App Module: Application state and coordinators
-- ============================================================================

local App = {
  -- Module check
  crsfModuleChecked = false,
  crsfModuleFound = false,

  -- User acknowledgment
  warningDismissed = false,

  -- Active dialogs
  warningDialog = nil,
  commandDialog = nil,

  -- Exit
  shouldExit = false,
}

-- Forward declarations for modules (needed for cross-references)
local Navigation
local Protocol
local UI
local Dialogs

-- Reset App to initial values
function App.reset()
  App.crsfModuleChecked = false
  App.crsfModuleFound = false
  App.warningDismissed = false
  App.shouldExit = false
  Protocol.reset()
end

-- Module check (caches result to avoid repeated checks)
function App.checkCrsfModule()
  if App.crsfModuleChecked then
    return App.crsfModuleFound
  end

  App.crsfModuleChecked = true
  App.crsfModuleFound = Protocol.hasCrsfModule()
  return App.crsfModuleFound
end

-- Coordinator: changes device and triggers UI update
-- When switching to a DIFFERENT device (user action), pushes a device entry
-- onto the navigation stack so the user can navigate back.
-- When the SAME device is updated (initial setup), just resets navigation.
function App.changeDevice(devId)
  local device = Protocol.getDevice(devId)
  if not device then return end
  local prevDeviceId = Protocol.deviceId
  local isSwitching = (prevDeviceId ~= devId)
  if Protocol.setDevice(device) then
    if isSwitching then
      Navigation.openDevice(device.name, prevDeviceId)
    else
      Navigation.reset()
    end
    return UI.invalidate()
  end
end

-- Coordinator: opens a folder, loads its children, and refreshes UI
function App.openFolder(folderId, folderName)
  Protocol.flushPendingSaves()
  Navigation.openFolder(folderId, folderName)
  Protocol.loadFolderChildren(folderId)
  return UI.invalidate()
end

-- Back button handler: navigate back or show exit confirmation
function App.handleBack()
  Protocol.flushPendingSaves()
  if Navigation.isAtRoot() then
    Dialogs.showConfirm({
      title = "Exit",
      message = "Exit ExpressLRS Lua script?",
      onConfirm = function() App.shouldExit = true end
    })
  else
    local entry = Navigation.goBack()
    if entry and entry.type == Navigation.TYPE_DEVICE and entry.prevDeviceId then
      local prevDevice = Protocol.getDevice(entry.prevDeviceId)
      if prevDevice then Protocol.setDevice(prevDevice) end
    end
    UI.invalidate()
  end
end

-- ============================================================================
-- Navigation Module: Folder navigation stack and methods
-- ============================================================================

Navigation = {
  stack = {},
  -- Navigation entry type constants (integers to save RAM vs strings)
  TYPE_FOLDER = 0,
  TYPE_DEVICE = 1,
  -- Synthetic folder IDs
  FOLDER_OTHER_DEVICES = -1,
}

function Navigation.getCurrent()
  local top = Navigation.stack[#Navigation.stack]
  return top and top.id or nil  -- nil if at root (or device root)
end

function Navigation.isAtRoot()
  return #Navigation.stack == 0
end

-- Check if the user has navigated into a device (for hiding "Other Devices")
function Navigation.hasDeviceEntry()
  for _, entry in ipairs(Navigation.stack) do
    if entry.type == Navigation.TYPE_DEVICE then return true end
  end
  return false
end

function Navigation.openFolder(folderId, folderName)
  local baseName = folderName
  if folderName then
    baseName = string.match(folderName, "^(.-)%s*%(.*%)$") or folderName
  end
  table.insert(Navigation.stack, { type = Navigation.TYPE_FOLDER, id = folderId, name = baseName })
end

function Navigation.openDevice(deviceName, prevDeviceId)
  -- id=nil means device root; getCurrent() returns nil which shows root fields
  table.insert(Navigation.stack, { type = Navigation.TYPE_DEVICE, id = nil, name = deviceName, prevDeviceId = prevDeviceId })
end

function Navigation.goBack()
  if #Navigation.stack > 0 then
    return table.remove(Navigation.stack)
  end
  return nil
end

function Navigation.reset()
  Navigation.stack = {}
end

-- ============================================================================
-- Protocol Module: CRSF constants, parsing, and field operations
-- ============================================================================

Protocol = {
  -- EdgeTX module type for CRSF/ELRS
  MODULE_TYPE_CROSSFIRE = 5,

  -- CRSF Field Type Constants
  CRSF = {
    UINT8          = 0,
    INT8           = 1,
    UINT16         = 2,
    INT16          = 3,
    UINT32         = 4,
    INT32          = 5,
    UINT64         = 6,
    INT64          = 7,
    FLOAT          = 8,
    TEXT_SELECTION = 9,
    STRING         = 10,
    FOLDER         = 11,
    INFO           = 12,
    COMMAND        = 13,
    -- Internal/extended types (not in official CRSF protocol)
    BACK_EXIT      = 14,
    DEVICE         = 15,
    DEVICE_FOLDER  = 16,

    -- Frame types
    FRAMETYPE_DEVICE_PING              = 0x28,
    FRAMETYPE_DEVICE_INFO              = 0x29,
    FRAMETYPE_PARAMETER_SETTINGS_ENTRY = 0x2B,
    FRAMETYPE_PARAMETER_READ           = 0x2C,
    FRAMETYPE_PARAMETER_WRITE          = 0x2D,
    FRAMETYPE_ELRS_STATUS              = 0x2E,

    -- Addresses
    ADDRESS_BROADCAST          = 0x00,
    ADDRESS_RADIO_TRANSMITTER  = 0xEA,
    ADDRESS_CRSF_RECEIVER      = 0xEC,
    ADDRESS_CRSF_TRANSMITTER   = 0xEE,
    ADDRESS_ELRS_LUA           = 0xEF,

    -- ELRS identification
    ELRS_SERIAL_ID             = 0x454C5253,

    -- ELRS flags: bits 0-1 are status (connected, status1),
    -- bits 2-4 are warnings (model match, armed, warning1),
    -- bits 5-7 are critical errors (error connected, error baudrate, critical2)
    ELRS_FLAGS_STATUS_MASK = 0x03,       -- bits 0-1: status flags only
    ELRS_FLAGS_WARNING_THRESHOLD = 0x1F, -- bits 5+: critical error flags

    -- Command steps (sent as last byte in PARAMETER_WRITE for COMMAND fields)
    CMD_IDLE       = 0,
    CMD_CLICK      = 1,
    CMD_EXECUTING  = 2,
    CMD_ASKCONFIRM = 3,
    CMD_CONFIRMED  = 4,
    CMD_CANCEL     = 5,
    CMD_QUERY      = 6,
  },

  -- Handlers dispatch table (populated after function definitions)
  handlers = {},

  -- Device identity (used in every CRSF frame) -- defaults to TX module + ELRS Lua
  deviceId = 0xEE,   -- ADDRESS_CRSF_TRANSMITTER (can't self-ref before table is created)
  handsetId = 0xEF,  -- ADDRESS_ELRS_LUA
  deviceName = nil,
  deviceIsELRS_TX = nil,

  -- Fields collection
  fields = {},
  fieldsCount = 0,
  fieldPopup = nil,

  -- Devices collection
  devices = {},
  devicesRefreshTimeout = 50,

  -- Status/flags (parsed from ELRS info messages)
  elrsFlags = 0,
  elrsFlagsInfo = "",
  receivedPackets = nil,
  lostPackets = nil,

  -- Protocol timing
  linkstatTimeout = 100,

  -- Communication state
  fieldTimeout = 0,
  fieldChunk = 0,
  fieldData = nil,
  loadQueue = {},
  expectChunksRemain = -1,
  backgroundLoading = false,

  -- Debounce: deferred saves for continuous controls (numberEdit)
  DEBOUNCE_SAVE_DELAY = 30,  -- 300ms in getTime() ticks (10ms each)
  pendingSaves = {},  -- keyed by field.id: { field, timeout }

  -- Connection transition tracking (for auto-discovery on reconnect)
  wasConnected = false,
}

-- Reset Protocol state
function Protocol.reset()
  -- Device identity
  Protocol.deviceId = Protocol.CRSF.ADDRESS_CRSF_TRANSMITTER
  Protocol.handsetId = Protocol.CRSF.ADDRESS_ELRS_LUA
  Protocol.deviceName = nil
  Protocol.deviceIsELRS_TX = nil

  -- Fields collection
  Protocol.fields = {}
  Protocol.fieldsCount = 0
  Protocol.fieldPopup = nil

  -- Devices collection
  Protocol.devices = {}
  Protocol.devicesRefreshTimeout = 50

  -- Status/flags
  Protocol.elrsFlags = 0
  Protocol.elrsFlagsInfo = ""
  Protocol.receivedPackets = nil
  Protocol.lostPackets = nil

  -- Protocol timing
  Protocol.linkstatTimeout = 100

  -- Communication state
  Protocol.fieldTimeout = 0
  Protocol.fieldChunk = 0
  Protocol.fieldData = nil
  Protocol.loadQueue = {}
  Protocol.expectChunksRemain = -1
  Protocol.backgroundLoading = false
  Protocol.pendingSaves = {}
  Protocol.wasConnected = false
end

-- Default telemetry wrappers: delegate to real EdgeTX functions
-- When mocking is active, setMock() replaces these with mock implementations
function Protocol.pop()
  return crossfireTelemetryPop()
end

function Protocol.push(command, data)
  return crossfireTelemetryPush(command, data)
end

-- Check connection state from elrsFlags
function Protocol.isConnected()
  return bit32.btest(Protocol.elrsFlags, 1)
end

-- Check if a CRSF-compatible module is available
function Protocol.hasCrsfModule()
  for modIdx = 0, 1 do
    local mod = model.getModule(modIdx)
    if mod and (mod.Type == nil or mod.Type == Protocol.MODULE_TYPE_CROSSFIRE) then
      return true
    end
  end
  return false
end

-- Set active device and prepare fields
-- Returns true if device changed, false if no change needed
function Protocol.setDevice(device)
  if not device then return false end
  if Protocol.deviceId == device.id and Protocol.fieldsCount == device.fldcnt then
    return false
  end

  Protocol.deviceId = device.id
  Protocol.elrsFlags = 0
  Protocol.deviceName = device.name
  Protocol.fieldsCount = device.fldcnt
  Protocol.deviceIsELRS_TX = device.isElrs and device.id == Protocol.CRSF.ADDRESS_CRSF_TRANSMITTER or nil
  Protocol.handsetId = Protocol.deviceIsELRS_TX and Protocol.CRSF.ADDRESS_ELRS_LUA or Protocol.CRSF.ADDRESS_RADIO_TRANSMITTER

  Protocol.allocateFields()
  Protocol.reloadAllFields()
  return true
end

-- ============================================================================
-- Protocol: Field management functions
-- ============================================================================

function Protocol.allocateFields()
  Protocol.fields = {}
  Protocol.fields[0] = {}  -- root folder (field 0)
  for i = 1, Protocol.fieldsCount do
    Protocol.fields[i] = {}
  end
end

-- Check if all children of a folder have been loaded (have names).
-- folderId: the folder's field ID, or nil for root (uses field 0).
function Protocol.isFolderLoaded(folderId)
  local folder = Protocol.fields[folderId or 0]
  if not folder or not folder.children then return false end
  for _, childId in ipairs(folder.children) do
    local child = Protocol.fields[childId]
    if not child or not child.name then return false end
  end
  return true
end

-- Return load progress for a folder's children as (loaded, total).
-- Returns nil if the folder or its children list is unknown yet.
function Protocol.getFolderLoadProgress(folderId)
  local folder = Protocol.fields[folderId or 0]
  if not folder or not folder.children then return nil end
  local total = #folder.children
  local loaded = 0
  for _, childId in ipairs(folder.children) do
    local child = Protocol.fields[childId]
    if child and child.name then loaded = loaded + 1 end
  end
  return loaded, total
end

function Protocol.reloadAllFields()
  Protocol.fieldTimeout = 0
  Protocol.fieldChunk = 0
  Protocol.fieldData = nil
  Protocol.loadQueue = {}
  -- Start by loading only field 0 (root folder).
  -- Its response contains child IDs; only root children are auto-queued.
  -- Subfolder children are loaded on-demand via loadFolderChildren().
  Protocol.loadQueue[1] = 0
end

-- Parameterized: takes folderId instead of accessing Navigation
function Protocol.getFieldsInFolder(folderId)
  local folder = Protocol.fields[folderId or 0]
  if not folder or not folder.children then return {} end
  local result = {}
  for _, childId in ipairs(folder.children) do
    local child = Protocol.fields[childId]
    if child and child.name and not child.hidden then
      result[#result + 1] = child
    end
  end
  return result
end

function Protocol.getDevice(id)
  for _, device in ipairs(Protocol.devices) do
    if device.id == id then
      return device
    end
  end
end

function Protocol.reloadCurField(field)
  Protocol.fieldTimeout = 0
  Protocol.fieldChunk = 0
  Protocol.fieldData = nil
  Protocol.loadQueue[#Protocol.loadQueue + 1] = field.id
end

-- Queue unloaded children of a folder for on-demand loading.
-- Called when the user navigates into a subfolder whose contents
-- haven't been fetched yet.
function Protocol.loadFolderChildren(folderId)
  local folder = Protocol.fields[folderId]
  if not folder or not folder.children then return end
  for i = #folder.children, 1, -1 do
    local childId = folder.children[i]
    local child = Protocol.fields[childId]
    if child and not child.name then
      Protocol.loadQueue[#Protocol.loadQueue + 1] = childId
    end
  end
  if #Protocol.loadQueue > 0 then
    Protocol.fieldTimeout = 0
  end
end

-- Queue all unloaded subfolder children for background preloading.
-- Called once after the root screen finishes loading so that navigating
-- into subfolders is instant (or near-instant).
function Protocol.startBackgroundLoad()
  Protocol.backgroundLoading = true
  for i = 1, #Protocol.fields do
    local field = Protocol.fields[i]
    if field.type == Protocol.CRSF.FOLDER and field.children then
      for j = #field.children, 1, -1 do
        local childId = field.children[j]
        local child = Protocol.fields[childId]
        if child and not child.name then
          Protocol.loadQueue[#Protocol.loadQueue + 1] = childId
        end
      end
    end
  end
  if #Protocol.loadQueue > 0 then
    Protocol.fieldTimeout = 0
  end
end

-- ============================================================================
-- Protocol: Field data helpers
-- ============================================================================

function Protocol.fieldGetStrOrOpts(data, offset, last, isOpts)
  local r = last or (isOpts and {})
  local optParts = {}
  local vcnt = 0
  repeat
    local b = data[offset]
    offset = offset + 1

    if not last then
      if r and (b == 59 or b == 0) then
        r[#r + 1] = table.concat(optParts)
        if #optParts > 0 then
          vcnt = vcnt + 1
          optParts = {}
        end
      elseif b ~= 0 then
        -- Translate legacy OpenTX arrow bytes (0xC0/0xC1) from ELRS firmware
        -- to EdgeTX CHAR_UP/CHAR_DOWN glyphs
        optParts[#optParts + 1] = ({
          [192] = CHAR_UP or (__opentx and __opentx.CHAR_UP),
          [193] = CHAR_DOWN or (__opentx and __opentx.CHAR_DOWN)
        })[b] or string.char(b)
      end
    end
  until b == 0

  return (r or table.concat(optParts)), offset, vcnt
end

function Protocol.fieldGetValue(data, offset, size)
  local result = 0
  for i = 0, size - 1 do
    result = bit32.lshift(result, 8) + data[offset + i]
  end
  return result
end

-- ============================================================================
-- Protocol: Field load functions
-- ============================================================================

local function fieldUnsignedLoad(field, data, offset, size, unitoffset)
  field.value = Protocol.fieldGetValue(data, offset, size)
  field.min = Protocol.fieldGetValue(data, offset + size, size)
  field.max = Protocol.fieldGetValue(data, offset + 2 * size, size)
  field.unit = Protocol.fieldGetStrOrOpts(data, offset + (unitoffset or (4 * size)), field.unit)
  if size ~= 1 then
    field.size = size
  end
end

local function fieldUnsignedToSigned(field, size)
  local bandval = bit32.lshift(0x80, (size - 1) * 8)
  field.value = field.value - bit32.band(field.value, bandval) * 2
  field.min = field.min - bit32.band(field.min, bandval) * 2
  field.max = field.max - bit32.band(field.max, bandval) * 2
end

local function fieldSignedLoad(field, data, offset, size, unitoffset)
  fieldUnsignedLoad(field, data, offset, size, unitoffset)
  fieldUnsignedToSigned(field, size)
  field.size = -size
end

function Protocol.fieldIntLoad(field, data, offset)
  local loadFn = (field.type % 2 == 0) and fieldUnsignedLoad or fieldSignedLoad
  return loadFn(field, data, offset, math.floor(field.type / 2) + 1)
end

function Protocol.fieldFloatLoad(field, data, offset)
  fieldSignedLoad(field, data, offset, 4, 21)
  field.prec = data[offset + 16]
  if field.prec > 3 then
    field.prec = 3
  end
  field.step = Protocol.fieldGetValue(data, offset + 17, 4)
  field.fmt = "%." .. tostring(field.prec) .. "f" .. field.unit
  field.prec = 10 ^ field.prec
end

function Protocol.fieldTextSelLoad(field, data, offset)
  local vcnt
  local cached = field.dirty == nil and field.values
  field.values, offset, vcnt = Protocol.fieldGetStrOrOpts(data, offset, cached, true)
  if not cached then
    field.disabled = vcnt <= 1
  end
  field.value = data[offset]
  field.unit = Protocol.fieldGetStrOrOpts(data, offset + 4)
  field.dirty = nil
end

function Protocol.fieldStringLoad(field, data, offset)
  field.value, offset = Protocol.fieldGetStrOrOpts(data, offset)
  if #data >= offset then
    field.maxlen = data[offset]
  end
end

function Protocol.fieldCommandLoad(field, data, offset)
  field.status = data[offset]
  field.timeout = data[offset + 1]
  field.info = Protocol.fieldGetStrOrOpts(data, offset + 2)
  if field.status == Protocol.CRSF.CMD_IDLE then
    Protocol.fieldPopup = nil
  end
end

function Protocol.fieldFolderLoad(field, data, offset)
  -- Parse child ID list (terminated by 0xFF)
  field.children = {}
  while data[offset] and data[offset] ~= 0xFF do
    field.children[#field.children + 1] = data[offset]
    offset = offset + 1
  end
end

-- ============================================================================
-- Protocol: Field save functions
-- ============================================================================

function Protocol.fieldIntSave(field)
  local value = field.value
  local size = field.size or 1
  if size < 0 then
    size = -size
    if value < 0 then
      value = bit32.lshift(0x100, (size - 1) * 8) + value
    end
  end

  local frame = { Protocol.deviceId, Protocol.handsetId, field.id }
  for i = size - 1, 0, -1 do
    frame[#frame + 1] = bit32.rshift(value, 8 * i) % 256
  end
  Protocol.push(Protocol.CRSF.FRAMETYPE_PARAMETER_WRITE, frame)
end

-- ============================================================================
-- Protocol: Related fields reload (for value changes)
-- ============================================================================

function Protocol.reloadParentFolder(field)
  if field.parent and Protocol.fields[field.parent] then
    Protocol.fields[field.parent].name = nil
    Protocol.loadQueue[#Protocol.loadQueue + 1] = field.parent
  end
end

function Protocol.debounceSave(field)
  Protocol.pendingSaves[field.id] = { field = field, timeout = getTime() + Protocol.DEBOUNCE_SAVE_DELAY }
end

function Protocol.flushPendingSaves()
  for id, ps in pairs(Protocol.pendingSaves) do
    Protocol.pendingSaves[id] = nil
    Protocol.fieldIntSave(ps.field)
    Protocol.reloadParentFolder(ps.field)
  end
end

function Protocol.reloadRelatedFields(field)
  Protocol.reloadParentFolder(field)

  for fieldId = Protocol.fieldsCount, 1, -1 do
    local sibling = Protocol.fields[fieldId]
    local siblingType = sibling.type or 99
    if fieldId ~= field.id
      and sibling.parent == field.parent
      and (siblingType < Protocol.CRSF.FOLDER or siblingType == Protocol.CRSF.INFO) then
      sibling.dirty = true
      sibling.name = nil
      Protocol.loadQueue[#Protocol.loadQueue + 1] = fieldId
    end
  end

  field.dirty = true
  field.name = nil
  Protocol.loadQueue[#Protocol.loadQueue + 1] = field.id
  Protocol.fieldTimeout = getTime() + 20
  Protocol.linkstatTimeout = Protocol.fieldTimeout + 100
end

function Protocol.handleCommandSave(field)
  Protocol.reloadCurField(field)

  if field.status ~= nil then
    if field.status < Protocol.CRSF.CMD_CONFIRMED then
      field.status = Protocol.CRSF.CMD_CLICK
      Protocol.push(Protocol.CRSF.FRAMETYPE_PARAMETER_WRITE, { Protocol.deviceId, Protocol.handsetId, field.id, field.status })
      Protocol.fieldPopup = field
      Protocol.fieldPopup.lastStatus = Protocol.CRSF.CMD_IDLE
      Protocol.fieldTimeout = getTime() + field.timeout
    end
  end
end

function Protocol.commandConfirm()
  if Protocol.fieldPopup then
    Protocol.push(Protocol.CRSF.FRAMETYPE_PARAMETER_WRITE, { Protocol.deviceId, Protocol.handsetId, Protocol.fieldPopup.id, Protocol.CRSF.CMD_CONFIRMED })
    Protocol.fieldTimeout = getTime() + Protocol.fieldPopup.timeout
    Protocol.fieldPopup.status = Protocol.CRSF.CMD_CONFIRMED
  end
end

function Protocol.commandCancel()
  if Protocol.fieldPopup then
    Protocol.push(Protocol.CRSF.FRAMETYPE_PARAMETER_WRITE, { Protocol.deviceId, Protocol.handsetId, Protocol.fieldPopup.id, Protocol.CRSF.CMD_CANCEL })
    Protocol.fieldPopup = nil
  end
end

-- ============================================================================
-- Protocol: Handlers dispatch table (no forward declarations needed!)
-- ============================================================================

Protocol.handlers = {
  [Protocol.CRSF.UINT8 + 1]          = { load = Protocol.fieldIntLoad, save = Protocol.fieldIntSave },
  [Protocol.CRSF.INT8 + 1]           = { load = Protocol.fieldIntLoad, save = Protocol.fieldIntSave },
  [Protocol.CRSF.UINT16 + 1]         = { load = Protocol.fieldIntLoad, save = Protocol.fieldIntSave },
  [Protocol.CRSF.INT16 + 1]          = { load = Protocol.fieldIntLoad, save = Protocol.fieldIntSave },
  [Protocol.CRSF.UINT32 + 1]         = nil,
  [Protocol.CRSF.INT32 + 1]          = nil,
  [Protocol.CRSF.UINT64 + 1]         = nil,
  [Protocol.CRSF.INT64 + 1]          = nil,
  [Protocol.CRSF.FLOAT + 1]          = { load = Protocol.fieldFloatLoad, save = Protocol.fieldIntSave },
  [Protocol.CRSF.TEXT_SELECTION + 1] = { load = Protocol.fieldTextSelLoad, save = Protocol.fieldIntSave },
  [Protocol.CRSF.STRING + 1]         = { load = Protocol.fieldStringLoad, save = nil },
  [Protocol.CRSF.FOLDER + 1]         = { load = Protocol.fieldFolderLoad, save = nil },
  [Protocol.CRSF.INFO + 1]           = { load = Protocol.fieldStringLoad, save = nil },
  [Protocol.CRSF.COMMAND + 1]        = { load = Protocol.fieldCommandLoad, save = Protocol.handleCommandSave },
  -- DEVICE and DEVICE_FOLDER are synthetic types, handled by UI directly
}

-- ============================================================================
-- Protocol: CRSF message parsing (return signals, no Nav/UI knowledge)
-- ============================================================================

function Protocol.parseDeviceInfoMessage(data)
  local id = data[2]
  local newName, offset = Protocol.fieldGetStrOrOpts(data, 3)
  local device = Protocol.getDevice(id)
  local isNew = (device == nil)
  if isNew then
    device = { id = id }
    Protocol.devices[#Protocol.devices + 1] = device
  end
  device.name = newName
  device.fldcnt = data[offset + 12]
  device.isElrs = Protocol.fieldGetValue(data, offset, 4) == Protocol.CRSF.ELRS_SERIAL_ID

  -- Return signal - caller handles device change and navigation
  -- shouldChangeDevice: true if this is info about the currently selected device
  -- isNewDevice: true if this device was not previously known
  local shouldChangeDevice = (Protocol.deviceId == id)
  return { shouldChangeDevice = shouldChangeDevice, deviceId = id, isNewDevice = isNew }
end

function Protocol.parseParameterInfoMessage(data)
  local fieldId = (Protocol.fieldPopup and Protocol.fieldPopup.id) or Protocol.loadQueue[#Protocol.loadQueue]
  if data[2] ~= Protocol.deviceId or data[3] ~= fieldId then
    Protocol.fieldData = nil
    Protocol.fieldChunk = 0
    return false
  end
  local field = Protocol.fields[fieldId]
  local chunksRemain = data[4]
  if not field or (Protocol.fieldData and chunksRemain ~= Protocol.expectChunksRemain) then
    return false
  end

  local offset
  if chunksRemain > 0 or Protocol.fieldChunk > 0 then
    Protocol.fieldData = Protocol.fieldData or {}
    for i = 5, #data do
      Protocol.fieldData[#Protocol.fieldData + 1] = data[i]
      data[i] = nil
    end
    offset = 1
  else
    Protocol.fieldData = data
    offset = 5
  end

  if chunksRemain > 0 then
    Protocol.fieldChunk = Protocol.fieldChunk + 1
    Protocol.expectChunksRemain = chunksRemain - 1
    return false
  else
    Protocol.loadQueue[#Protocol.loadQueue] = nil

    if #Protocol.fieldData > (offset + 2) then
      field.id = fieldId
      field.parent = (Protocol.fieldData[offset] ~= 0) and Protocol.fieldData[offset] or nil
      field.type = bit32.band(Protocol.fieldData[offset + 1], 0x7f)
      field.hidden = bit32.btest(Protocol.fieldData[offset + 1], 0x80) or nil
      field.name, offset = Protocol.fieldGetStrOrOpts(Protocol.fieldData, offset + 2, field.name)
      local handler = Protocol.handlers[field.type + 1]
      if handler and handler.load then
        handler.load(field, Protocol.fieldData, offset)
      end
      if field.min == 0 then field.min = nil end
      if field.max == 0 then field.max = nil end

      -- Auto-queue children for root folder (field 0) and during background preloading.
      -- Subfolder children are otherwise loaded on-demand when user navigates into them.
      if field.type == Protocol.CRSF.FOLDER and field.children
          and (fieldId == 0 or Protocol.backgroundLoading) then
        for i = #field.children, 1, -1 do
          Protocol.loadQueue[#Protocol.loadQueue + 1] = field.children[i]
        end
      end
    end

    Protocol.fieldChunk = 0
    Protocol.fieldData = nil

    return Protocol.deviceId ~= Protocol.CRSF.ADDRESS_CRSF_TRANSMITTER or #Protocol.loadQueue == 0
  end
end

function Protocol.parseElrsInfoMessage(data)
  if data[2] ~= Protocol.deviceId then
    Protocol.fieldData = nil
    Protocol.fieldChunk = 0
    return
  end

  Protocol.lostPackets = data[3]
  Protocol.receivedPackets = (data[4] * 256) + data[5]
  local newFlags = data[6]
  Protocol.elrsFlags = newFlags
  Protocol.elrsFlagsInfo = Protocol.fieldGetStrOrOpts(data, 7)
end

function Protocol.parseElrsV1Message(data)
  if (data[1] ~= Protocol.CRSF.ADDRESS_RADIO_TRANSMITTER) or (data[2] ~= Protocol.CRSF.ADDRESS_CRSF_TRANSMITTER) then
    return
  end
  Protocol.fieldPopup = { id = 0, status = Protocol.CRSF.CMD_EXECUTING, timeout = 0xFF, info = "ERROR: 1.x firmware" }
  Protocol.fieldTimeout = getTime() + 0xFFFF
end

-- ============================================================================
-- Protocol: Main CRSF communication loop (renamed from refreshNext)
-- ============================================================================

function Protocol.poll()
  local command, data
  local deviceInfoResult = nil

  repeat
    command, data = Protocol.pop()
    if command == Protocol.CRSF.FRAMETYPE_DEVICE_INFO then
      deviceInfoResult = Protocol.parseDeviceInfoMessage(data)
    elseif command == Protocol.CRSF.FRAMETYPE_PARAMETER_SETTINGS_ENTRY then
      Protocol.parseParameterInfoMessage(data)
      if #Protocol.loadQueue > 0 then
        Protocol.fieldTimeout = 0
      elseif Protocol.fieldPopup then
        Protocol.fieldTimeout = getTime() + Protocol.fieldPopup.timeout
      end
    elseif command == Protocol.CRSF.FRAMETYPE_PARAMETER_WRITE then
      Protocol.parseElrsV1Message(data)
    elseif command == Protocol.CRSF.FRAMETYPE_ELRS_STATUS then
      Protocol.parseElrsInfoMessage(data)
    end
  until command == nil

  -- Auto-discover other devices when link transitions to connected
  local connected = Protocol.isConnected()
  if connected and not Protocol.wasConnected and #Protocol.devices <= 1 then
    Protocol.push(Protocol.CRSF.FRAMETYPE_DEVICE_PING, { Protocol.CRSF.ADDRESS_BROADCAST, Protocol.CRSF.ADDRESS_RADIO_TRANSMITTER })
    Protocol.devicesRefreshTimeout = getTime() + 100
  end
  Protocol.wasConnected = connected

  local time = getTime()
  -- Flush any debounced saves whose timer has expired
  for id, ps in pairs(Protocol.pendingSaves) do
    if time > ps.timeout then
      Protocol.pendingSaves[id] = nil
      Protocol.fieldIntSave(ps.field)
      Protocol.reloadParentFolder(ps.field)
    end
  end

  if Protocol.fieldPopup then
    if time > Protocol.fieldTimeout and Protocol.fieldPopup.status ~= Protocol.CRSF.CMD_ASKCONFIRM then
      Protocol.push(Protocol.CRSF.FRAMETYPE_PARAMETER_WRITE, { Protocol.deviceId, Protocol.handsetId, Protocol.fieldPopup.id, Protocol.CRSF.CMD_QUERY })
      Protocol.fieldTimeout = time + Protocol.fieldPopup.timeout
    end
  elseif time > Protocol.devicesRefreshTimeout and #Protocol.devices == 0 then
    Protocol.devicesRefreshTimeout = time + 100
    Protocol.push(Protocol.CRSF.FRAMETYPE_DEVICE_PING, { Protocol.CRSF.ADDRESS_BROADCAST, Protocol.CRSF.ADDRESS_RADIO_TRANSMITTER })
  elseif time > Protocol.linkstatTimeout then
    if Protocol.deviceIsELRS_TX then
      Protocol.push(Protocol.CRSF.FRAMETYPE_PARAMETER_WRITE, { Protocol.deviceId, Protocol.handsetId, 0x0, 0x0 })
    else
      Protocol.receivedPackets = nil
      Protocol.lostPackets = nil
    end
    Protocol.linkstatTimeout = time + 100
  elseif time > Protocol.fieldTimeout and Protocol.fieldsCount ~= 0 then
    if #Protocol.loadQueue > 0 then
      Protocol.push(Protocol.CRSF.FRAMETYPE_PARAMETER_READ, { Protocol.deviceId, Protocol.handsetId, Protocol.loadQueue[#Protocol.loadQueue], Protocol.fieldChunk })
      Protocol.fieldTimeout = time + (Protocol.deviceIsELRS_TX and 50 or 500)
    else
      Protocol.backgroundLoading = false
    end
  end

  return { deviceInfo = deviceInfoResult }
end

-- ============================================================================
-- UI Module: LVGL page/widget building
-- ============================================================================

UI = {
  currentPage = nil,
  uiBuilt = false,
  folderWasReady = false,
}

-- Called by Navigation/run loop to trigger UI rebuild
function UI.invalidate()
  UI.uiBuilt = false
end

function UI.getSubtitle()
  -- When inside a folder, show current menu item name
  if not Navigation.isAtRoot() then
    local top = Navigation.stack[#Navigation.stack]
    local path = top.name or ""

    -- Append loading indicator only when current folder's children aren't ready
    local loaded, total = Protocol.getFolderLoadProgress(Navigation.getCurrent())
    if loaded and loaded < total then
      path = path .. string.format(" • Loading %d%%", math.floor(loaded / total * 100))
    end

    return path
  end

  -- At root level, show loading status only during initial load (not background)
  local loaded, total = Protocol.getFolderLoadProgress(nil)
  if loaded and loaded < total and Protocol.fieldsCount > 0 then
    return string.format("Loading %d%%", math.floor(loaded / total * 100))
  end

  -- At root level, show link stats (and warning if present)
  local subtitle = ""
  if Protocol.receivedPackets then
    local state = Protocol.isConnected() and "Connected" or "No link"
    subtitle = string.format("%u/%u • %s", Protocol.lostPackets, Protocol.receivedPackets, state)
  end

  -- Add warning indicator if warning/error flags are set (bits 2+, above status bits 0-1)
  if Protocol.elrsFlags > Protocol.CRSF.ELRS_FLAGS_STATUS_MASK and Protocol.elrsFlagsInfo and Protocol.elrsFlagsInfo ~= "" then
    if subtitle ~= "" then
      subtitle = subtitle .. " • " .. Protocol.elrsFlagsInfo
    else
      subtitle = Protocol.elrsFlagsInfo
    end
  end

  return subtitle
end

function UI.incrField(field, step)
  local min, max = 0, 0
  if field.type <= Protocol.CRSF.FLOAT then
    min = field.min or 0
    max = field.max or 0
    step = (field.step or 1) * step
  elseif field.type == Protocol.CRSF.TEXT_SELECTION then
    min = 0
    max = #field.values - 1
  end

  local newval = field.value
  repeat
    newval = newval + step
    if newval < min then
      newval = min
    elseif newval > max then
      newval = max
    end

    if field.values == nil or #field.values[newval + 1] ~= 0 then
      field.value = newval
      return
    end
  until (newval == min or newval == max)
end

-- Check if field has only Off/On values
function UI.isBooleanField(field)
  if not field.values or #field.values ~= 2 then
    return false
  end
  return field.values[1] == "Off" and field.values[2] == "On"
end

-- ============================================================================
-- UI: Widget creators
-- ============================================================================

function UI.createChoiceRow(pg, field)
  local row = pg:rectangle({
    w = lvgl.PERCENT_SIZE + 100,
    thickness = 0,
    flexFlow = lvgl.FLOW_ROW,
    flexPad = 0
  })

  local labelRect = row:rectangle({
    w = lvgl.PERCENT_SIZE + 50,
    h = lvgl.UI_ELEMENT_HEIGHT,
    thickness = 0,
    children = {
        {
            type = lvgl.LABEL,
            y = lvgl.PAD_SMALL,
            text = field.name or "",
            color = COLOR_THEME_PRIMARY1
        }
    }
  })

  local ctrlRect = row:rectangle({
    w = lvgl.PERCENT_SIZE + 50,
    thickness = 0,
    flexFlow = lvgl.FLOW_ROW,
    align = LEFT + VCENTER
  })

  if UI.isBooleanField(field) then
    ctrlRect:toggle({
      get = function() return field.value or 0 end,
      set = function(val)
        field.value = val
        Protocol.fieldIntSave(field)
        Protocol.reloadRelatedFields(field)
      end,
      active = function() return not field.disabled end
    })
  else
    local filteredValues = {}
    local origToFiltered = {}
    local filteredToOrig = {}
    for i, v in ipairs(field.values or {}) do
      if v ~= "" then
        filteredValues[#filteredValues + 1] = v
        origToFiltered[i - 1] = #filteredValues
        filteredToOrig[#filteredValues] = i - 1
      end
    end
    ctrlRect:choice({
      values = filteredValues,
      get = function() return origToFiltered[field.value or 0] or 1 end,
      set = function(val)
        field.value = filteredToOrig[val] or 0
        Protocol.fieldIntSave(field)
        Protocol.reloadRelatedFields(field)
      end,
      active = function() return not field.disabled end
    })
  end

  if field.unit and field.unit ~= "" then
    ctrlRect:box({
      h = lvgl.UI_ELEMENT_HEIGHT,
      children = {
          {
              type = lvgl.LABEL,
              y = lvgl.PAD_SMALL,
              text = " " .. field.unit,
          }
      }
    })
  end
end

function UI.createNumberRow(pg, field)
  local displayFn
  if field.type == Protocol.CRSF.FLOAT then
    displayFn = function(val)
      return string.format(field.fmt or "%.0f", val / (field.prec or 1))
    end
  else
    displayFn = function(val)
      return tostring(val) .. (field.unit or "")
    end
  end

  pg:build({
    {type="rectangle", w=lvgl.PERCENT_SIZE+100, thickness=0, flexFlow=lvgl.FLOW_ROW, flexPad=0, children={
      {type="rectangle", w=lvgl.PERCENT_SIZE+50, thickness=0, children={
        {type="label", text=field.name or "", color=COLOR_THEME_PRIMARY1},
      }},
      {type="rectangle", w=lvgl.PERCENT_SIZE+50, thickness=0, flexFlow=lvgl.FLOW_ROW, align=LEFT, children={
        {type="numberEdit", min=field.min or 0, max=field.max or 255,
          get=function() return field.value or 0 end,
          set=function(val)
            field.value = val
          end,
          edited=function(val)
            field.value = val
            Protocol.fieldIntSave(field)
            Protocol.reloadParentFolder(field)
          end,
          display=displayFn,
          active=function() return not field.disabled end},
      }},
    }},
  })
end

function UI.createInfoRow(pg, field)
  pg:build({
    {type="rectangle", w=lvgl.PERCENT_SIZE+100, thickness=0, flexFlow=lvgl.FLOW_ROW, flexPad=0, children={
      {type="rectangle", w=lvgl.PERCENT_SIZE+50, thickness=0, children={
        {type="label", text=field.name or "", color=COLOR_THEME_PRIMARY1},
      }},
      {type="rectangle", w=lvgl.PERCENT_SIZE+50, thickness=0, flexFlow=lvgl.FLOW_ROW, align=LEFT, children={
        {type="label", text=field.value or ""},
      }},
    }},
  })
end

function UI.createFolderWidget(pg, field, width)
  pg:button({
    text = field.name or "",
    w = width or (lvgl.PERCENT_SIZE + 100),
    h = lvgl.UI_ELEMENT_HEIGHT * 2,
    press = function()
      App.openFolder(field.id, field.name)
    end
  })
end

function UI.createCommandWidget(pg, field)
  pg:button({
    text = field.name or "",
    w = lvgl.PERCENT_SIZE + 100,
    press = function()
      Protocol.handleCommandSave(field)
    end
  })
end

function UI.buildFieldWidget(pg, field, folderWidth)
  if not field or not field.name then
    return
  end

  local fieldType = field.type

  if fieldType == Protocol.CRSF.FOLDER then
    return UI.createFolderWidget(pg, field, folderWidth)
  end

  if fieldType == Protocol.CRSF.COMMAND then
    return UI.createCommandWidget(pg, field)
  end

  if fieldType <= Protocol.CRSF.INT16 or fieldType == Protocol.CRSF.FLOAT then
    return UI.createNumberRow(pg, field)
  end

  if fieldType == Protocol.CRSF.TEXT_SELECTION then
    return UI.createChoiceRow(pg, field)
  end

  if fieldType == Protocol.CRSF.STRING or fieldType == Protocol.CRSF.INFO then
    return UI.createInfoRow(pg, field)
  end
end

-- ============================================================================
-- UI: Main build function
-- ============================================================================

function UI.build()
  lvgl.clear()

  local pageOptions = {
    title = "ExpressLRS",
    subtitle = UI.getSubtitle
  }

  if not Navigation.isAtRoot() then
    pageOptions.backButton = true
    pageOptions.back = function()
      App.handleBack()
    end
  else
    pageOptions.back = App.handleBack
  end

  UI.currentPage = lvgl.page(pageOptions)

  local outerContainer = UI.currentPage:box({
    w = lvgl.PERCENT_SIZE + 100,
    flexFlow = lvgl.FLOW_ROW,
    flexPad = lvgl.PAD_TINY,
    align = CENTER
  })

  local fieldContainer = outerContainer:box({
    w = lvgl.PERCENT_SIZE + 100,
    flexFlow = lvgl.FLOW_COLUMN,
    flexPad = lvgl.PAD_TINY
  })

  local currentFolder = Navigation.getCurrent()

  if currentFolder == Navigation.FOLDER_OTHER_DEVICES then
    -- Render device list directly from Protocol.devices
    for _, device in ipairs(Protocol.devices) do
      if device.id ~= Protocol.deviceId then
        fieldContainer:button({
          text = device.name or "Unknown",
          w = lvgl.PERCENT_SIZE + 100,
          press = function()
            App.changeDevice(device.id)
          end
        })
      end
    end
  else
    -- Normal field rendering for root (nil) or a subfolder ID
    if currentFolder == nil then
      -- Device name row at root level
      UI.createInfoRow(fieldContainer, { name = "Device", value = Protocol.deviceName or "Searching..." })
    end

    local fieldsInFolder = Protocol.getFieldsInFolder(currentFolder)
    local FOLDERS_PER_ROW = 3
    local folderWidth = math.floor(100 / FOLDERS_PER_ROW) - 1
    local i = 1
    while i <= #fieldsInFolder do
      local field = fieldsInFolder[i]

      if field.type == Protocol.CRSF.FOLDER then
        -- Batch consecutive folders into rows
        local folderBatch = {}
        while i <= #fieldsInFolder and fieldsInFolder[i].type == Protocol.CRSF.FOLDER do
          table.insert(folderBatch, fieldsInFolder[i])
          i = i + 1
        end

        for j = 1, #folderBatch, FOLDERS_PER_ROW do
          local rowContainer = fieldContainer:box({
            w = lvgl.PERCENT_SIZE + 100,
            flexFlow = lvgl.FLOW_ROW,
            flexPad = lvgl.PAD_SMALL,
            align = CENTER,
            color = COLOR_THEME_PRIMARY2
          })

          for k = 0, FOLDERS_PER_ROW - 1 do
            local folderField = folderBatch[j + k]
            if folderField then
              UI.createFolderWidget(rowContainer, folderField, lvgl.PERCENT_SIZE + folderWidth)
            end
          end
        end
      else
        UI.buildFieldWidget(fieldContainer, field)
        i = i + 1
      end
    end

    -- Synthetic Lua script version row at root when device is TX
    if currentFolder == nil and Protocol.deviceIsELRS_TX then
      UI.createInfoRow(fieldContainer, { name = "Lua script version", value = VERSION })
    end

    -- Append "Other Devices" button at the end when at root with multiple devices
    if currentFolder == nil and #Protocol.devices > 1 and not Navigation.hasDeviceEntry() then
      fieldContainer:button({
        text = "Other Devices",
        w = lvgl.PERCENT_SIZE + 100,
        h = lvgl.UI_ELEMENT_HEIGHT * 2,
        press = function()
          App.openFolder(Navigation.FOLDER_OTHER_DEVICES, "Other Devices")
        end
      })
    end
  end

  fieldContainer:rectangle({
    w = lvgl.PERCENT_SIZE + 100,
    h = lvgl.PAD_SMALL,
    thickness = 0
  })

  UI.uiBuilt = true
end

-- ============================================================================
-- Dialogs Module: Generic LVGL wrappers (no business logic)
-- ============================================================================

Dialogs = {}

function Dialogs.showConfirm(options)
  return lvgl.confirm({
    title = options.title,
    message = options.message,
    confirm = options.onConfirm,
    cancel = options.onCancel,
  })
end

function Dialogs.showMessage(options)
  return lvgl.message({
    title = options.title,
    message = options.message,
  })
end

-- ============================================================================
-- ModelMismatchDialog: Domain-specific dialog
-- ============================================================================

local ModelMismatchDialog = {}

function ModelMismatchDialog.show(onContinue, onExit)
  local dg = lvgl.dialog({
    title = "Model Mismatch",
    flexFlow = lvgl.FLOW_COLUMN,
    flexPad = lvgl.PAD_SMALL
  })

  dg:build({
    {type="box", x=10, flexFlow=lvgl.FLOW_COLUMN, flexPad=lvgl.PAD_SMALL, children={
      {type="label", text="Receiver connected but Model ID doesn't match."},
      {type="label", text="This prevents controlling the wrong model."},
      {type="label", text="To use this receiver:"},
      {type="label", text="Set Model Match to OFF"},
    }},
    {type="box", flexFlow=lvgl.FLOW_ROW, flexPad=lvgl.PAD_SMALL, w=lvgl.PERCENT_SIZE+100, children={
      {type="button", text="Continue", w=lvgl.PERCENT_SIZE+48, press=function()
        dg:close()
        onContinue()
      end},
      {type="button", text="Exit to Change Model", w=lvgl.PERCENT_SIZE+48, press=function()
        dg:close()
        onExit()
      end},
    }},
  })

  return dg
end

-- ============================================================================
-- NoModuleDialog: Domain-specific dialog
-- ============================================================================

local NoModuleDialog = {}

function NoModuleDialog.show(onExit)
  lvgl.clear()

  local dg = lvgl.dialog({
    title = "No Module Found: Check Model Settings",
    flexFlow = lvgl.FLOW_COLUMN,
    flexPad = lvgl.PAD_SMALL,
    close = onExit
  })

  dg:build({
    {type="box", x=10, flexFlow=lvgl.FLOW_COLUMN, flexPad=lvgl.PAD_SMALL, children={
      {type="label", text="- Internal/External module enabled"},
      {type="label", text="- Protocol set to CRSF"},
      {type="label", text="- Minimum Baud rate (depends on packet rate):"},
      {type="label", text="  400k for 250Hz", font=SMLSIZE},
      {type="label", text="  921k for 500Hz", font=SMLSIZE},
      {type="label", text="  1.87M for F1000", font=SMLSIZE},
    }},
    {type="box", flexFlow=lvgl.FLOW_ROW, w=lvgl.PERCENT_SIZE+100, align=CENTER, children={
      {type="button", text="Exit", w=lvgl.PERCENT_SIZE+98, press=function()
        dg:close()
        onExit()
      end},
    }},
  })

  return dg
end

-- ============================================================================
-- CommandPage: Non-modal pages for command confirm/executing states.
-- Uses lvgl.page() instead of lvgl.dialog() so that run() keeps being
-- called and protocol polling continues during command execution.
-- ============================================================================

local CommandPage = {}
local spinnerAngle = 0

local function createSpinner(parent)
  local r = 20
  local d = r * 2
  local wrapper = parent:box({
    flexFlow = lvgl.FLOW_ROW,
    flexPad = lvgl.PAD_MEDIUM,
    color = COLOR_THEME_PRIMARY2,
    w = lvgl.PERCENT_SIZE + 100,
    align = CENTER
  })
  wrapper:arc({
    radius = r,
    thickness = 4,
    rounded = true,
    color = COLOR_THEME_PRIMARY1,
    startAngle = function()
      spinnerAngle = (spinnerAngle + 8) % 360
      return spinnerAngle
    end,
    endAngle = function()
      return spinnerAngle + 120
    end,
  })
end

function CommandPage.showConfirm(name, info, onConfirm, onCancel)
  lvgl.clear()
  local pg = lvgl.page({
    title = "ExpressLRS",
    subtitle = "Send command",
    back = onCancel,
  })

  local container = pg:box({
    w = lvgl.PERCENT_SIZE + 100,
    flexFlow = lvgl.FLOW_COLUMN,
    flexPad = lvgl.PAD_MEDIUM,
    align = CENTER,
  })

  container:build({
    {type="rectangle", w=lvgl.PERCENT_SIZE+100, h=lvgl.PAD_LARGE, thickness=0},
    {type="label", text=name or "Command", w=lvgl.PERCENT_SIZE+100, align=CENTER, font=BOLD},
    {type="label", text=info or "", w=lvgl.PERCENT_SIZE+100, align=CENTER, color=COLOR_THEME_DISABLED},
    {type="rectangle", w=lvgl.PERCENT_SIZE+100, h=lvgl.PAD_LARGE, thickness=0},
    {type="box", w=lvgl.PERCENT_SIZE+100, flexFlow=lvgl.FLOW_ROW, flexPad=lvgl.PAD_SMALL, align=CENTER, children={
      {type="button", text="Confirm", w=lvgl.PERCENT_SIZE+49, press=onConfirm},
      {type="button", text="Cancel", w=lvgl.PERCENT_SIZE+49, press=onCancel},
    }},
  })

  return pg
end

function CommandPage.showExecuting(title, onCancel)
  lvgl.clear()
  local pg = lvgl.page({
    title = "ExpressLRS",
    subtitle = title or "Executing...",
    back = onCancel,
  })

  local container = pg:box({
    w = lvgl.PERCENT_SIZE + 100,
    flexFlow = lvgl.FLOW_COLUMN,
    flexPad = lvgl.PAD_MEDIUM,
    align = CENTER,
  })

  createSpinner(container)

  container:build({
    {type="rectangle", w=lvgl.PERCENT_SIZE+100, h=lvgl.PAD_SMALL, thickness=0},
    {type="label", text="Hold [RTN] to exit and keep running", w=lvgl.PERCENT_SIZE+100, align=CENTER, color=COLOR_THEME_DISABLED},
    {type="rectangle", w=lvgl.PERCENT_SIZE+100, h=lvgl.PAD_LARGE, thickness=0},
    {type="button", text="Cancel command", w=lvgl.PERCENT_SIZE+100, press=onCancel},
  })

  return pg
end

-- ============================================================================
-- Mock data for simulator
-- ============================================================================

local function setMock()
  local _, rv = getVersion()
  if string.sub(rv, -5) ~= "-simu" then return end
  local mockModule = loadScript("/SCRIPTS/TOOLS/csrfsimulator/csrfsimulator.lua")
  if mockModule == nil then return end
  local mock = mockModule()
  Protocol.pop = mock.pop
  Protocol.push = mock.push
  Protocol.hasCrsfModule = function() return mock.moduleFound end
end

-- ============================================================================
-- Coordination: Command popup handling (in run loop)
-- ============================================================================

local function onCommandCancel()
  Protocol.commandCancel()
  App.commandDialog = nil
  UI.invalidate()
end

local function handleCommandPopup()
  if not Protocol.fieldPopup then
    -- Command finished: rebuild normal UI if we were showing a command page
    if App.commandDialog then
      App.commandDialog = nil
      UI.invalidate()
    end
    return
  end

  if Protocol.fieldPopup.status == Protocol.CRSF.CMD_IDLE and Protocol.fieldPopup.lastStatus ~= Protocol.CRSF.CMD_IDLE then
    Protocol.reloadAllFields()
    Protocol.fieldPopup = nil
    App.commandDialog = nil
    UI.invalidate()
  elseif Protocol.fieldPopup.status == Protocol.CRSF.CMD_ASKCONFIRM then
    if not App.commandDialog or Protocol.fieldPopup.lastStatus ~= Protocol.CRSF.CMD_ASKCONFIRM then
      App.commandDialog = CommandPage.showConfirm(
        Protocol.fieldPopup.name,
        Protocol.fieldPopup.info,
        function() Protocol.commandConfirm() end,
        onCommandCancel
      )
    end
    Protocol.fieldPopup.lastStatus = Protocol.fieldPopup.status
  elseif Protocol.fieldPopup.status == Protocol.CRSF.CMD_EXECUTING then
    if not App.commandDialog or Protocol.fieldPopup.lastStatus ~= Protocol.CRSF.CMD_EXECUTING then
      App.commandDialog = CommandPage.showExecuting(
        Protocol.fieldPopup.name or Protocol.fieldPopup.info,
        onCommandCancel
      )
    end
    Protocol.fieldPopup.lastStatus = Protocol.fieldPopup.status
  end
end

-- ============================================================================
-- Coordination: Warning handling (in run loop)
-- ============================================================================

local function handleWarning()
  if App.shouldExit then return end
  if Protocol.elrsFlags > Protocol.CRSF.ELRS_FLAGS_WARNING_THRESHOLD then
    if not App.warningDialog and not App.warningDismissed then
      if Protocol.elrsFlagsInfo == "Model Mismatch" then
        App.warningDialog = ModelMismatchDialog.show(
          function()
            App.warningDismissed = true
            UI.invalidate()
          end,
          function()
            App.warningDismissed = true
            App.shouldExit = true
          end
        )
      else
        Dialogs.showMessage({
          title = "Warning",
          message = Protocol.elrsFlagsInfo
        })
        App.warningDialog = true
      end
    end
  else
    App.warningDialog = nil
    App.warningDismissed = false
  end
end

-- ============================================================================
-- Init
-- ============================================================================

local function init()
  if lvgl == nil then
    return
  end

  setMock()
end

-- ============================================================================
-- LVGL support check
-- ============================================================================

local function showLvglRequired()
  lcd.clear()
  lcd.drawText(10, 10, "LVGL support required", MIDSIZE)
  lcd.drawText(10, 30, "EdgeTX 2.11.4+ needed", 0)
end

-- ============================================================================
-- Run (main coordinator)
-- ============================================================================

local function run(event, touchState)
  if event == nil then return 2 end

  -- Check for LVGL support
  if lvgl == nil then
    showLvglRequired()
    return 0
  end

  -- Check for CRSF module
  if not App.checkCrsfModule() then
    if not UI.uiBuilt then
      NoModuleDialog.show(function() App.shouldExit = true end)
      UI.uiBuilt = true
    end
    if App.shouldExit then
      return 2
    end
    return 0
  end

  -- CRSF polling
  local pollResult = Protocol.poll()

  -- Handle device info update
  if pollResult.deviceInfo then
    -- Change device if protocol indicates current device was updated
    if pollResult.deviceInfo.shouldChangeDevice then
      App.changeDevice(pollResult.deviceInfo.deviceId)
    end
    -- Refresh UI if a new device appeared (shows "Other Devices" button at root,
    -- or updates the device list if already viewing that folder).
    -- At root level, wait until the folder has finished loading before rebuilding.
    if pollResult.deviceInfo.isNewDevice and UI.folderWasReady then
      UI.invalidate()
    elseif Navigation.getCurrent() == Navigation.FOLDER_OTHER_DEVICES then
      UI.invalidate()
    end
  end

  -- Handle command popups
  handleCommandPopup()

  -- When a command page is active, skip normal UI management
  if not App.commandDialog then
    -- Handle warnings
    handleWarning()

    -- Rebuild UI once when the current folder's fields become fully loaded
    local currentFolder = Navigation.getCurrent()
    local folderReady = Protocol.isFolderLoaded(currentFolder)
    if folderReady and not UI.folderWasReady then
      -- Reclaim memory from temporary tables created during field parsing.
      -- Done once per folder load rather than per-field to avoid repeated
      -- full GC cycles on the hot path (fieldGetStrOrOpts).
      collectgarbage("collect")
      if UI.uiBuilt then
        UI.invalidate()
      end
      -- Start background preloading of subfolder contents once root is ready
      if currentFolder == nil and not Protocol.backgroundLoading then
        Protocol.startBackgroundLoad()
      end
    end
    UI.folderWasReady = folderReady

    -- Build/rebuild UI when needed
    if not UI.uiBuilt and #Protocol.fields > 0 then
      UI.build()
    end
  end

  if App.shouldExit then
    return 2
  end

  return 0
end

-- ============================================================================
-- Return
-- ============================================================================

return { init = init, run = run, useLvgl = true }
