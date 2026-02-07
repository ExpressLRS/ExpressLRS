-- ============================================================================
-- CRSF Simulator: Packet-level mock for crossfireTelemetryPop/Push
-- ============================================================================
-- This module simulates the CRSF protocol at the packet level, allowing the
-- ELRS Lua script to exercise the full communication flow (device discovery,
-- parameter loading, value writes, ELRS status) in the EdgeTX simulator.
--
-- Usage: loaded by elrs_lvgl3.lua setMock() when running in simulator mode.
-- Returns a table with { pop, push, moduleFound } fields.
-- ============================================================================

-- ============================================================================
-- Configuration: Change scenario here to test different states
-- ============================================================================

-- Scenarios:
--   "normal"         TX + RX connected. Happy path with full telemetry, link
--                    stats, and all parameters from both devices.
--   "disconnected"   TX present but no RX. Shows "No link" in subtitle.
--                    No receiver device in Other Devices list.
--   "reconnect"      Starts disconnected, then transitions to connected after
--                    ~5 seconds. Tests auto-discovery of Other Devices on
--                    reconnect without restarting the script.
--   "model_mismatch" TX + RX connected but with Model ID mismatch flag set.
--                    Triggers the Model Mismatch warning dialog.
--   "armed"          TX + RX connected with the "is Armed" warning flag set.
--                    Shows armed warning in subtitle.
--   "no_module"      No CRSF module found at all. Triggers the "No Module
--                    Found" error dialog immediately.
local config = {
  scenario = "normal",
}

-- ============================================================================
-- CRSF Protocol Constants (local copies, independent of Protocol.CRSF)
-- ============================================================================

local CRSF = {
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

  -- Field types0
  UINT8          = 0,
  INT8           = 1,
  UINT16         = 2,
  INT16          = 3,
  FLOAT          = 8,
  TEXT_SELECTION  = 9,
  STRING         = 10,
  FOLDER         = 11,
  INFO           = 12,
  COMMAND        = 13,

  -- ELRS identification
  ELRS_SERIAL_ID = 0x454C5253,

  -- Command steps
  CMD_IDLE       = 0,
  CMD_CLICK      = 1,
  CMD_EXECUTING  = 2,
  CMD_ASKCONFIRM = 3,
  CMD_CONFIRMED  = 4,
  CMD_CANCEL     = 5,
  CMD_QUERY      = 6,
}

-- ============================================================================
-- Rate configuration table (matches SX128X 2.4GHz from common.cpp)
-- Maps Packet Rate option index to Hz, interval (µs), and default TLM ratio
-- TLM ratio indices into "Std;Off;1:128;1:64;1:32;1:16;1:8;1:4;1:2;Race":
--   0=Std, 1=Off, 2=1:128, 3=1:64, 4=1:32, 5=1:16, 6=1:8, 7=1:4, 8=1:2, 9=Race
-- ============================================================================

local rateConfigs = {
  [0] = { hz = 50,  interval = 20000, defaultTlm = 5 },  -- TLM_RATIO_1_16
  [1] = { hz = 150, interval = 6666,  defaultTlm = 4 },  -- TLM_RATIO_1_32
  [2] = { hz = 250, interval = 4000,  defaultTlm = 3 },  -- TLM_RATIO_1_64
  [3] = { hz = 500, interval = 2000,  defaultTlm = 2 },  -- TLM_RATIO_1_128
}

-- ============================================================================
-- FIFO Packet Queue
-- ============================================================================

local packetQueue = {}
local queueHead = 1

-- Deferred packets simulate OTA relay delay (e.g., RX DEVICE_INFO arriving
-- later than TX DEVICE_INFO). They are delivered in the NEXT poll cycle,
-- after the main queue has been drained and a nil has been returned.
local deferredQueue = {}
local deferredReady = false

local function queuePush(command, data)
  packetQueue[#packetQueue + 1] = { command = command, data = data }
end

local function queuePushDeferred(command, data)
  deferredQueue[#deferredQueue + 1] = { command = command, data = data }
end

local function queuePop()
  -- Serve from main queue first
  if queueHead <= #packetQueue then
    local pkt = packetQueue[queueHead]
    queueHead = queueHead + 1
    deferredReady = false
    return pkt.command, pkt.data
  end

  -- Main queue empty, reset it
  packetQueue = {}
  queueHead = 1

  -- Serve deferred packets only after a nil has been returned (next poll cycle)
  if deferredReady and #deferredQueue > 0 then
    local pkt = table.remove(deferredQueue, 1)
    return pkt.command, pkt.data
  end

  -- Mark deferred as ready for the next poll cycle
  if #deferredQueue > 0 then
    deferredReady = true
  end

  return nil
end

-- ============================================================================
-- String-to-bytes helper
-- ============================================================================

local function appendString(tbl, str)
  for i = 1, #str do
    tbl[#tbl + 1] = string.byte(str, i)
  end
  tbl[#tbl + 1] = 0  -- null terminator
end

local function appendU32BE(tbl, val)
  tbl[#tbl + 1] = bit32.band(bit32.rshift(val, 24), 0xFF)
  tbl[#tbl + 1] = bit32.band(bit32.rshift(val, 16), 0xFF)
  tbl[#tbl + 1] = bit32.band(bit32.rshift(val, 8), 0xFF)
  tbl[#tbl + 1] = bit32.band(val, 0xFF)
end

local function appendU16BE(tbl, val)
  tbl[#tbl + 1] = bit32.band(bit32.rshift(val, 8), 0xFF)
  tbl[#tbl + 1] = bit32.band(val, 0xFF)
end

-- ============================================================================
-- CRSF Packet Encoders
-- ============================================================================

--- Encode a DEVICE_INFO response packet (frame type 0x29)
-- @param device  table with: id, name, serialNo, hwVer, swVer, fieldCount
-- @param destAddr  destination address (usually ADDRESS_RADIO_TRANSMITTER)
-- @return data table suitable for queuePush(FRAMETYPE_DEVICE_INFO, data)
local function encodeDeviceInfo(device, destAddr)
  local data = {}
  data[1] = destAddr or CRSF.ADDRESS_RADIO_TRANSMITTER
  data[2] = device.id
  -- Device name (null-terminated)
  appendString(data, device.name)
  -- Serial number (4 bytes BE)
  appendU32BE(data, device.serialNo or CRSF.ELRS_SERIAL_ID)
  -- Hardware version (4 bytes BE)
  appendU32BE(data, device.hwVer or 0)
  -- Software version (4 bytes BE)
  appendU32BE(data, device.swVer or 0x00030500)  -- 3.5.0
  -- Field count
  data[#data + 1] = device.fieldCount
  -- Parameter version
  data[#data + 1] = 0
  return data
end

--- Encode a PARAMETER_SETTINGS_ENTRY packet (frame type 0x2B)
-- Encodes one chunk of a parameter. Currently only single-chunk (chunk 0) supported.
-- @param device  the device table (for id)
-- @param param   the parameter definition table
-- @param chunk   chunk index (0 for single-chunk params)
-- @param destAddr  destination address
-- @return data table suitable for queuePush(FRAMETYPE_PARAMETER_SETTINGS_ENTRY, data)
local function encodeParameterEntry(device, param, chunk, destAddr)
  local data = {}
  data[1] = destAddr or CRSF.ADDRESS_RADIO_TRANSMITTER
  data[2] = device.id
  data[3] = param.id         -- Field ID
  data[4] = 0                -- Chunks remaining (0 = single chunk)
  data[5] = param.parent or 0  -- Parent ID (0 = root)
  data[6] = param.type       -- Type byte (with hidden flag if needed)
  if param.hidden then
    data[6] = bit32.bor(data[6], 0x80)
  end

  -- Parameter name (null-terminated) — use dynamic name if set (e.g. folder summaries)
  appendString(data, param.dynName or param.name)

  -- Type-specific value data
  local t = bit32.band(param.type, 0x7F)

  if t == CRSF.TEXT_SELECTION then
    -- Options string (semicolon-separated, null-terminated)
    appendString(data, param.options)
    -- Value (current selection index)
    data[#data + 1] = param.value or 0
    -- Min
    data[#data + 1] = 0
    -- Max (count of options - 1)
    local optCount = 1
    for i = 1, #param.options do
      if string.byte(param.options, i) == 59 then  -- ';'
        optCount = optCount + 1
      end
    end
    data[#data + 1] = optCount - 1
    -- Default
    data[#data + 1] = 0
    -- Units (null-terminated)
    appendString(data, param.units or "")

  elseif t == CRSF.COMMAND then
    -- Status
    data[#data + 1] = param.status or CRSF.CMD_IDLE
    -- Timeout (in 10ms ticks, 200 = 2s)
    data[#data + 1] = param.timeout or 200
    -- Info string (null-terminated)
    appendString(data, param.info or "")

  elseif t == CRSF.FOLDER then
    -- Folder contains a list of child parameter IDs terminated by 0xFF.
    -- This allows the Lua script to know which fields to load for this folder.
    -- We need the device context to scan for children.
    if param._device then
      for _, p in ipairs(param._device.params) do
        if (param.id == 0 and (p.parent == 0 or p.parent == nil)) or
           (param.id ~= 0 and p.parent == param.id) then
          data[#data + 1] = p.id
        end
      end
    end
    data[#data + 1] = 0xFF  -- terminator

  elseif t == CRSF.INFO then
    -- Info value string (null-terminated)
    appendString(data, param.value or "")

  elseif t == CRSF.STRING then
    -- String value (null-terminated)
    appendString(data, param.value or "")

  elseif t == CRSF.UINT8 then
    -- value, min, max (1 byte each)
    data[#data + 1] = param.value or 0
    data[#data + 1] = param.min or 0
    data[#data + 1] = param.max or 255
    -- default
    data[#data + 1] = param.default or 0
    -- units
    appendString(data, param.units or "")

  elseif t == CRSF.INT8 then
    -- Same as UINT8 but values may be signed (stored as unsigned in wire format)
    local v = param.value or 0
    if v < 0 then v = v + 256 end
    local mn = param.min or 0
    if mn < 0 then mn = mn + 256 end
    local mx = param.max or 127
    if mx < 0 then mx = mx + 256 end
    data[#data + 1] = v
    data[#data + 1] = mn
    data[#data + 1] = mx
    data[#data + 1] = param.default or 0
    appendString(data, param.units or "")

  elseif t == CRSF.UINT16 or t == CRSF.INT16 then
    -- value, min, max (2 bytes BE each)
    appendU16BE(data, param.value or 0)
    appendU16BE(data, param.min or 0)
    appendU16BE(data, param.max or 65535)
    -- default (2 bytes)
    appendU16BE(data, param.default or 0)
    appendString(data, param.units or "")

  elseif t == CRSF.FLOAT then
    -- value, min, max, default (4 bytes BE each), precision (1 byte), step (4 bytes BE)
    appendU32BE(data, param.value or 0)
    appendU32BE(data, param.min or 0)
    appendU32BE(data, param.max or 0)
    appendU32BE(data, param.default or 0)
    data[#data + 1] = param.prec or 0
    appendU32BE(data, param.step or 1)
    appendString(data, param.units or "")
  end

  return data
end

--- Encode an ELRS_STATUS packet (frame type 0x2E)
-- @param deviceId   source device address
-- @param destAddr   destination address
-- @param badPkts    bad packets count (uint8)
-- @param goodPkts   good packets count (uint16)
-- @param flags      warning flags byte
-- @param flagsInfo  warning message string
-- @return data table
local function encodeElrsStatus(deviceId, destAddr, badPkts, goodPkts, flags, flagsInfo)
  local data = {}
  data[1] = destAddr or CRSF.ADDRESS_RADIO_TRANSMITTER
  data[2] = deviceId
  data[3] = badPkts or 0
  -- Good packets as uint16 BE
  appendU16BE(data, goodPkts or 0)
  data[#data + 1] = flags or 0
  -- Warning info string (null-terminated)
  appendString(data, flagsInfo or "")
  return data
end

-- ============================================================================
-- TX Device Definition (address 0xEE)
-- Matches TXModuleParameters.cpp parameter structure
-- ============================================================================

local txDevice = {
  id = CRSF.ADDRESS_CRSF_TRANSMITTER,
  name = "TX16S MK3",
  serialNo = CRSF.ELRS_SERIAL_ID,
  hwVer = 0,
  swVer = 0x00030500,  -- 3.5.0
  fieldCount = 21,  -- total parameter count
  params = {
    { id = 1,  parent = 0, type = CRSF.TEXT_SELECTION, name = "Packet Rate",
      options = "50(-117dBm);150(-112dBm);250(-108dBm);500(-105dBm)", value = 2, units = "Hz" },
    { id = 2,  parent = 0, type = CRSF.TEXT_SELECTION, name = "Telem Ratio",
      options = "Std;Off;1:128;1:64;1:32;1:16;1:8;1:4;1:2;Race", value = 0, units = " (1:64)" },
    { id = 3,  parent = 0, type = CRSF.TEXT_SELECTION, name = "Switch Mode",
      options = "Hybrid;Wide", value = 1, units = "" },
    { id = 4,  parent = 0, type = CRSF.TEXT_SELECTION, name = "Model Match",
      options = "Off;On", value = 0, units = "(ID: 1)" },
    { id = 5,  parent = 0, type = CRSF.TEXT_SELECTION, name = "Antenna Mode",
      options = "Gemini;Ant 1;Ant 2;Switch", value = 0, units = "" },

    -- TX Power folder
    { id = 6,  parent = 0, type = CRSF.FOLDER, name = "TX Power" },
    { id = 7,  parent = 6, type = CRSF.TEXT_SELECTION, name = "Max Power",
      options = "10;25;50;100;250", value = 4, units = "mW" },
    { id = 8,  parent = 6, type = CRSF.TEXT_SELECTION, name = "Dynamic",
      options = "Off;Dyn;AUX9;AUX10;AUX11;AUX12", value = 1, units = "" },
    { id = 9,  parent = 6, type = CRSF.TEXT_SELECTION, name = "Fan Thresh",
      options = "10mW;25mW;50mW;100mW;250mW", value = 3, units = "" },

    -- VTX Administrator folder
    { id = 10, parent = 0, type = CRSF.FOLDER, name = "VTX Administrator" },
    { id = 11, parent = 10, type = CRSF.TEXT_SELECTION, name = "Band",
      options = "Off;A;B;E;F;R;L", value = 5, units = "" },
    { id = 12, parent = 10, type = CRSF.UINT8, name = "Channel",
      value = 1, min = 1, max = 8, units = "" },
    { id = 13, parent = 10, type = CRSF.TEXT_SELECTION, name = "Pwr Lvl",
      options = "-;1;2;3;4;5;6;7;8", value = 0, units = "" },
    { id = 14, parent = 10, type = CRSF.TEXT_SELECTION, name = "Pitmode",
      options = "Off;On", value = 0, units = "" },
    { id = 15, parent = 10, type = CRSF.COMMAND, name = "Send VTx",
      status = CRSF.CMD_IDLE, timeout = 50, info = "" },

    -- WiFi Connectivity folder
    { id = 16, parent = 0, type = CRSF.FOLDER, name = "WiFi Connectivity" },
    { id = 17, parent = 16, type = CRSF.COMMAND, name = "Enable WiFi",
      status = CRSF.CMD_IDLE, timeout = 50, info = "", persistent = true },  -- runs until cancelled
    { id = 18, parent = 16, type = CRSF.COMMAND, name = "Enable Rx WiFi",
      status = CRSF.CMD_IDLE, timeout = 50, info = "", persistent = true },  -- runs until cancelled

    -- Root-level commands and info
    { id = 19, parent = 0, type = CRSF.COMMAND, name = "Bind",
      status = CRSF.CMD_IDLE, timeout = 50, info = "" },

    -- Bad/Good (hidden from ELRS Lua, visible to other UIs)
    { id = 20, parent = 0, type = CRSF.INFO, name = "Bad/Good",
      value = "0/250", hidden = true },

    -- Version + regulatory domain (name = version+domain, value = commit hash)
    { id = 21, parent = 0, type = CRSF.INFO, name = "3.5.0 ISM2G4",
      value = "825ed8" },
  },
}

-- ============================================================================
-- RX Device Definition (address 0xEC)
-- Matches RXParameters.cpp parameter structure
-- ============================================================================

local rxDevice = {
  id = CRSF.ADDRESS_CRSF_RECEIVER,
  name = "ELRS 2400RX",
  serialNo = CRSF.ELRS_SERIAL_ID,
  hwVer = 0,
  swVer = 0x00030500,  -- 3.5.0
  fieldCount = 22,  -- total parameter count
  params = {
    { id = 1,  parent = 0, type = CRSF.TEXT_SELECTION, name = "Protocol",
      options = "CRSF;Inverted CRSF;SBUS;Inverted SBUS;SUMD;DJI RS Pro;HoTT Telemetry;MAVLink;DisplayPort;GPS",
      value = 0, units = "" },
    { id = 2,  parent = 0, type = CRSF.TEXT_SELECTION, name = "SBUS failsafe",
      options = "No Pulses;Last Pos", value = 0, units = "" },
    { id = 3,  parent = 0, type = CRSF.TEXT_SELECTION, name = "Ant. Mode",
      options = "Antenna A;Antenna B;Diversity", value = 2, units = "" },
    { id = 4,  parent = 0, type = CRSF.TEXT_SELECTION, name = "Tlm Power",
      options = "10;25;50;100;250;MatchTX", value = 2, units = "mW" },

    -- Team Race folder
    { id = 5,  parent = 0, type = CRSF.FOLDER, name = "Team Race" },
    { id = 6,  parent = 5, type = CRSF.TEXT_SELECTION, name = "Channel",
      options = "AUX2;AUX3;AUX4;AUX5;AUX6;AUX7;AUX8;AUX9;AUX10;AUX11;AUX12",
      value = 0, units = "" },
    { id = 7,  parent = 5, type = CRSF.TEXT_SELECTION, name = "Position",
      options = "Disabled;1/Low;2;3;Mid;4;5;6/High", value = 0, units = "" },

    -- Output Mapping folder
    { id = 8,  parent = 0, type = CRSF.FOLDER, name = "Output Mapping" },
    { id = 9,  parent = 8, type = CRSF.UINT8, name = "Output Ch",
      value = 1, min = 1, max = 4, units = "" },
    { id = 10, parent = 8, type = CRSF.UINT8, name = "Input Ch",
      value = 1, min = 1, max = 16, units = "" },
    { id = 11, parent = 8, type = CRSF.TEXT_SELECTION, name = "Output Mode",
      options = "50Hz;60Hz;100Hz;160Hz;333Hz;400Hz;10kHzDuty;On/Off;DShot",
      value = 0, units = "" },
    { id = 12, parent = 8, type = CRSF.TEXT_SELECTION, name = "Invert",
      options = "Off;On", value = 0, units = "" },

    -- PWM Channel 1 subfolder (nested inside Output Mapping)
    { id = 13, parent = 8, type = CRSF.FOLDER, name = "PWM Ch1" },
    { id = 14, parent = 13, type = CRSF.UINT8, name = "Failsafe",
      value = 0, min = 0, max = 100, units = "%" },
    { id = 15, parent = 13, type = CRSF.TEXT_SELECTION, name = "Mode",
      options = "50Hz;60Hz;100Hz;160Hz;333Hz;400Hz", value = 0, units = "" },

    -- PWM Channel 2 subfolder (nested inside Output Mapping)
    { id = 16, parent = 8, type = CRSF.FOLDER, name = "PWM Ch2" },
    { id = 17, parent = 16, type = CRSF.UINT8, name = "Failsafe",
      value = 0, min = 0, max = 100, units = "%" },
    { id = 18, parent = 16, type = CRSF.TEXT_SELECTION, name = "Mode",
      options = "50Hz;60Hz;100Hz;160Hz;333Hz;400Hz", value = 0, units = "" },

    -- Bind Storage & Bind Mode
    { id = 19, parent = 0, type = CRSF.TEXT_SELECTION, name = "Bind Storage",
      options = "Persistent;Volatile;Returnable;Administered", value = 0, units = "" },
    { id = 20, parent = 0, type = CRSF.COMMAND, name = "Enter Bind Mode",
      status = CRSF.CMD_IDLE, timeout = 50, info = "" },

    -- Model Id
    { id = 21, parent = 0, type = CRSF.INFO, name = "Model Id",
      value = "12" },

    -- Info fields
    { id = 22, parent = 0, type = CRSF.INFO, name = "RX Version",
      value = "3.5.0 825ed8" },
  },
}

-- ============================================================================
-- Parameter lookup helper
-- ============================================================================

local function findParam(device, fieldId)
  for _, p in ipairs(device.params) do
    if p.id == fieldId then
      return p
    end
  end
  return nil
end

local function findDeviceByAddr(addr)
  if addr == txDevice.id then return txDevice end
  if rxDevice and addr == rxDevice.id then return rxDevice end
  return nil
end

--- Extract the Nth label (0-indexed) from a semicolon-separated options string.
-- Matches the firmware's findSelectionLabel() behavior.
-- @param options  semicolon-separated string (e.g. "10;25;50;100;250")
-- @param index    0-based index
-- @return label string, or "" if index is out of range
local function getOptionLabel(options, index)
  local i = 0
  for label in string.gmatch(options, "([^;]+)") do
    if i == index then return label end
    i = i + 1
  end
  return ""
end

-- ============================================================================
-- Dynamic folder names (mirrors TXModuleParameters.cpp updateFolderNames)
-- ============================================================================

--- Update the dynName field on TX Power and VTX Administrator folders
-- so the simulator matches real firmware behavior where folder names show
-- a summary of the current settings in parentheses.
-- @param device  the device table whose params to update
local function updateFolderNames(device)
  -- TX Power folder (id=6): children Max Power (id=7), Dynamic (id=8)
  local txPwrFolder = findParam(device, 6)
  local maxPower    = findParam(device, 7)
  local dynamic     = findParam(device, 8)
  if txPwrFolder and maxPower then
    local pwrLabel = getOptionLabel(maxPower.options, maxPower.value or 0)
    local name = "TX Power (" .. pwrLabel
    if dynamic and (dynamic.value or 0) > 0 then
      local dynLabel = getOptionLabel(dynamic.options, dynamic.value)
      name = name .. " " .. dynLabel
    end
    name = name .. ")"
    txPwrFolder.dynName = name
  end

  -- VTX Administrator folder (id=10): children Band (id=11), Channel (id=12),
  -- Pwr Lvl (id=13), Pitmode (id=14)
  local vtxFolder = findParam(device, 10)
  local vtxBand   = findParam(device, 11)
  local vtxChan   = findParam(device, 12)
  local vtxPwr    = findParam(device, 13)
  local vtxPit    = findParam(device, 14)
  if vtxFolder and vtxBand then
    local bandVal = vtxBand.value or 0
    if bandVal == 0 then
      -- Band is "Off" -> use static name (no dynamic suffix)
      vtxFolder.dynName = nil
    else
      local bandLabel = getOptionLabel(vtxBand.options, bandVal)
      local chanLabel = tostring((vtxChan and vtxChan.value) or 1)
      local name = "VTX Admin (" .. bandLabel .. ":" .. chanLabel

      local pwrVal = (vtxPwr and vtxPwr.value) or 0
      if pwrVal > 0 then
        local pwrLabel = getOptionLabel(vtxPwr.options, pwrVal)
        name = name .. ":" .. pwrLabel

        local pitVal = (vtxPit and vtxPit.value) or 0
        if pitVal == 1 then
          name = name .. ":P"
        elseif pitVal > 1 then
          local pitLabel = getOptionLabel(vtxPit.options, pitVal)
          name = name .. ":" .. pitLabel
        end
      end

      name = name .. ")"
      vtxFolder.dynName = name
    end
  end
end

-- ============================================================================
-- Dynamic telemetry bandwidth (mirrors TXModuleParameters.cpp updateTlmBandwidth)
-- ============================================================================

--- Convert a TLM ratio option index to its divisor value.
-- Matches firmware TLMratioEnumToValue().
-- Options: 0=Std, 1=Off, 2=1:128, 3=1:64, 4=1:32, 5=1:16, 6=1:8, 7=1:4, 8=1:2, 9=Race
-- @param enumval  option index (0-based)
-- @return divisor integer (e.g. 128, 64, 32, …)
local function tlmRatioEnumToValue(enumval)
  if enumval <= 1 then return 1 end       -- Std/Off -> 1 (caller handles display)
  if enumval >= 9 then return 1 end       -- Race -> same as Std
  -- 2=1:128 -> 128, 3=1:64 -> 64, … 8=1:2 -> 2
  -- Formula: 2^(8 + 1 - enumval)  (matching firmware: 1 << (8 + TLM_RATIO_NO_TLM - enumval))
  return math.floor(2 ^ (9 - enumval))
end

--- Compute TLM burst max for a given rate and ratio divisor.
-- Matches firmware TLMBurstMaxForRateRatio().
-- @param rateHz   packet rate in Hz
-- @param ratioDiv ratio divisor (e.g. 128, 64, …)
-- @return burst count (>= 1)
local function tlmBurstMaxForRateRatio(rateHz, ratioDiv)
  local retVal = math.floor(512 * rateHz / ratioDiv / 1000)
  if retVal > 1 then
    retVal = retVal - 1
  else
    retVal = 1
  end
  return retVal
end

--- Update the Telem Ratio units field to show bandwidth or default ratio.
-- Mirrors firmware updateTlmBandwidth() from TXModuleParameters.cpp.
-- @param device  the device table (txDevice)
local function updateTlmBandwidth(device)
  local packetRate = findParam(device, 1)  -- Packet Rate
  local telemRatio = findParam(device, 2)  -- Telem Ratio
  local switchMode = findParam(device, 3)  -- Switch Mode
  if not packetRate or not telemRatio then return end

  local rateIdx = packetRate.value or 0
  local rateCfg = rateConfigs[rateIdx]
  if not rateCfg then return end

  local tlmVal = telemRatio.value or 0

  -- Std (0) or Race (9): display the rate's default ratio
  if tlmVal == 0 or tlmVal == 9 then
    local defaultDiv = tlmRatioEnumToValue(rateCfg.defaultTlm)
    telemRatio.units = " (1:" .. defaultDiv .. ")"
    return
  end

  -- Off (1): empty units
  if tlmVal == 1 then
    telemRatio.units = ""
    return
  end

  -- Specific ratio (2-8): compute bandwidth in bps
  local hz = rateCfg.hz
  local ratioDiv = tlmRatioEnumToValue(tlmVal)
  local burst = tlmBurstMaxForRateRatio(hz, ratioDiv)

  -- Wide mode (value=1) uses 8ch/fullres OTA -> 10 bytes per call
  -- Hybrid mode (value=0) uses 4ch/std OTA -> 5 bytes per call
  local isFullRes = switchMode and (switchMode.value or 0) == 1
  local bytesPerCall = isFullRes and 10 or 5

  local bandwidth = math.floor(bytesPerCall * 8 * burst * hz / ratioDiv / (burst + 1))

  -- FullRes correction: extra bandwidth from telemetry packed into LinkStats packet
  -- sizeof(OTA_LinkStats_s) = 4 bytes
  if isFullRes then
    bandwidth = bandwidth + 8 * (10 - 4)
  end

  telemRatio.units = " (" .. bandwidth .. "bps)"
end

-- Set initial dynamic folder names based on default parameter values
updateFolderNames(txDevice)
-- Set initial telemetry bandwidth display
updateTlmBandwidth(txDevice)

-- ============================================================================
-- Scenario State
-- ============================================================================

-- Reconnect scenario timing
local reconnectDelay = 500  -- ~5 seconds (getTime() ticks at 10ms)
local startTime = nil       -- set on first mockPush/mockPop call

-- Dynamic RX availability (replaces static hasRxDevice boolean)
local function isRxAvailable()
  if config.scenario == "reconnect" then
    if not startTime then return false end
    return getTime() - startTime >= reconnectDelay
  end
  return config.scenario ~= "disconnected"
end

-- ELRS Lua flag bits (from TXModuleEndpoint.h):
--   bit 0: LUA_FLAG_CONNECTED
--   bit 1: LUA_FLAG_STATUS1
--   bit 2: LUA_FLAG_MODEL_MATCH (warning)
--   bit 3: LUA_FLAG_ISARMED (warning)
--   bit 4: LUA_FLAG_WARNING1
--   bit 5: LUA_FLAG_ERROR_CONNECTED (critical)
--   bit 6: LUA_FLAG_ERROR_BAUDRATE (critical)
local function getElrsFlags()
  if config.scenario == "reconnect" then
    return isRxAvailable() and 0x01 or 0x00
  elseif config.scenario == "model_mismatch" then
    return 0x05  -- connected + model mismatch
  elseif config.scenario == "armed" then
    return 0x09  -- connected + armed
  elseif config.scenario == "normal" then
    return 0x01  -- connected
  else
    return 0x00  -- disconnected
  end
end

local function getElrsFlagsInfo()
  if config.scenario == "model_mismatch" then
    return "Model Mismatch"
  elseif config.scenario == "armed" then
    return "is Armed!"
  end
  return ""
end

-- ============================================================================
-- Command state machine (per-parameter)
-- ============================================================================

local commandStates = {}  -- keyed by "deviceId:paramId"

local function getCommandKey(deviceId, paramId)
  return tostring(deviceId) .. ":" .. tostring(paramId)
end

-- Number of CMD_QUERY polls a command stays in CMD_EXECUTING before completing.
-- Keep low for snappy simulator testing; real hardware controls its own timing.
local COMMAND_EXECUTE_POLLS = 1

local function handleCommandWrite(device, param, newStatus)
  local key = getCommandKey(device.id, param.id)
  if not commandStates[key] then
    commandStates[key] = { status = CRSF.CMD_IDLE, info = "" }
  end
  local state = commandStates[key]

  if newStatus == CRSF.CMD_CLICK or newStatus == CRSF.CMD_CONFIRMED then
    local needsConfirm = param.persistent and config.scenario == "normal"
    if newStatus == CRSF.CMD_CLICK and needsConfirm then
      -- WiFi/BLE commands ask for confirmation only when connected (scenario "normal")
      state.status = CRSF.CMD_ASKCONFIRM
      state.info = "Confirm " .. param.name .. "?"
    else
      -- Go straight to executing (matches real ELRS firmware behavior:
      -- most commands skip confirmation and execute immediately)
      state.status = CRSF.CMD_EXECUTING
      state.info = "Executing..."
      if param.persistent then
        state.queriesRemaining = nil  -- runs until cancelled (e.g., WiFi)
      else
        state.queriesRemaining = COMMAND_EXECUTE_POLLS
      end
    end
  elseif newStatus == CRSF.CMD_CANCEL then
    state.status = CRSF.CMD_IDLE
    state.info = ""
  elseif newStatus == CRSF.CMD_QUERY then
    -- Advance executing commands toward completion.
    -- Commands with queriesRemaining = nil run indefinitely until cancelled.
    if state.status == CRSF.CMD_EXECUTING and state.queriesRemaining then
      state.queriesRemaining = state.queriesRemaining - 1
      if state.queriesRemaining <= 0 then
        state.status = CRSF.CMD_IDLE
        state.info = "Complete"
      end
    end
  end

  -- Update the param for encoding
  param.status = state.status
  param.info = state.info
end

-- ============================================================================
-- mockPush: Processes commands sent by the Lua script
-- ============================================================================

local function mockPush(command, data)
  if not startTime then startTime = getTime() end

  if command == CRSF.FRAMETYPE_DEVICE_PING then
    local destAddr = data[2] or CRSF.ADDRESS_RADIO_TRANSMITTER

    -- TX module responds immediately (local to handset)
    queuePush(CRSF.FRAMETYPE_DEVICE_INFO, encodeDeviceInfo(txDevice, destAddr))

    -- RX device responds with delay (relayed over air link)
    -- Uses deferred delivery so it arrives in the next poll cycle,
    -- after the TX DEVICE_INFO has been processed
    if isRxAvailable() then
      queuePushDeferred(CRSF.FRAMETYPE_DEVICE_INFO, encodeDeviceInfo(rxDevice, destAddr))
    end
    return true

  elseif command == CRSF.FRAMETYPE_PARAMETER_READ then
    -- Parameter read request: data = { deviceId, handsetId, fieldId, chunk }
    local deviceId = data[1]
    local fieldId = data[3]
    local chunk = data[4] or 0
    local destAddr = data[2] or CRSF.ADDRESS_RADIO_TRANSMITTER

    local device = findDeviceByAddr(deviceId)
    if device then
      local param
      if fieldId == 0 then
        -- Field 0 is the root folder (synthetic, not in params list)
        param = { id = 0, parent = 0, type = CRSF.FOLDER, name = device.name, _device = device }
      else
        param = findParam(device, fieldId)
      end
      if param then
        -- Check if there's a command state override
        local key = getCommandKey(device.id, param.id)
        if commandStates[key] and bit32.band(param.type, 0x7F) == CRSF.COMMAND then
          param.status = commandStates[key].status
          param.info = commandStates[key].info
        end
        -- Set device context for folder child ID encoding
        param._device = param._device or device
        local entry = encodeParameterEntry(device, param, chunk, destAddr)
        param._device = nil  -- clean up temporary reference
        queuePush(CRSF.FRAMETYPE_PARAMETER_SETTINGS_ENTRY, entry)
      end
    end
    return true

  elseif command == CRSF.FRAMETYPE_PARAMETER_WRITE then
    -- Parameter write: data = { deviceId, handsetId, fieldId, value/status }
    local deviceId = data[1]
    local fieldId = data[3]
    local writeValue = data[4]

    -- Special case: ELRS status request (fieldId == 0)
    if fieldId == 0 then
      local flags = getElrsFlags()
      local flagsInfo = getElrsFlagsInfo()
      local destAddr = data[2] or CRSF.ADDRESS_RADIO_TRANSMITTER
      queuePush(CRSF.FRAMETYPE_ELRS_STATUS,
        encodeElrsStatus(deviceId, destAddr, 0, 250, flags, flagsInfo))
      return true
    end

    local device = findDeviceByAddr(deviceId)
    if device then
      local param = findParam(device, fieldId)
      if param then
        local t = bit32.band(param.type, 0x7F)
        if t == CRSF.COMMAND then
          -- Command: handle state machine
          handleCommandWrite(device, param, writeValue)
          -- Queue the updated parameter entry as response
          local destAddr = data[2] or CRSF.ADDRESS_RADIO_TRANSMITTER
          queuePush(CRSF.FRAMETYPE_PARAMETER_SETTINGS_ENTRY,
            encodeParameterEntry(device, param, 0, destAddr))
        else
          -- Value write: update the stored value
          param.value = writeValue
          -- Refresh dynamic folder names (e.g. "TX Power (250 Dyn)")
          updateFolderNames(device)
          -- Refresh telemetry bandwidth display (affected by Packet Rate, Telem Ratio, Switch Mode)
          updateTlmBandwidth(device)
          -- If this param lives inside a folder, re-send the parent folder entry
          -- so the Lua script picks up the updated dynName in the UI header
          if param.parent and param.parent ~= 0 then
            local parentParam = findParam(device, param.parent)
            if parentParam and bit32.band(parentParam.type, 0x7F) == CRSF.FOLDER then
              local destAddr = data[2] or CRSF.ADDRESS_RADIO_TRANSMITTER
              parentParam._device = device
              queuePush(CRSF.FRAMETYPE_PARAMETER_SETTINGS_ENTRY,
                encodeParameterEntry(device, parentParam, 0, destAddr))
              parentParam._device = nil
            end
          end
        end
      end
    end
    return true
  end

  -- Unknown command - ignore
  return true
end

-- ============================================================================
-- mockPop: Returns next queued packet or nil
-- ============================================================================

local function mockPop()
  if not startTime then startTime = getTime() end
  return queuePop()
end

-- ============================================================================
-- Module found depends on scenario
-- ============================================================================

local moduleFound = (config.scenario ~= "no_module")

-- ============================================================================
-- Return mock interface
-- ============================================================================

return {
  pop = mockPop,
  push = mockPush,
  moduleFound = moduleFound,
}
