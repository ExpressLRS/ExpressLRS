-- TNS|ExpressLRS|TNE
---- #########################################################################
---- #                                                                       #
---- # Copyright (C) OpenTX, adapted for ExpressLRS                          #
-----#                                                                       #
---- # License GPLv2: http://www.gnu.org/licenses/gpl-2.0.html               #
---- #                                                                       #
---- #########################################################################

-- Global API caching for performance
local _lcd = lcd
local _crossfireTelemetryPush = crossfireTelemetryPush
local _crossfireTelemetryPop = crossfireTelemetryPop
local _getTime = getTime
local _getVersion = getVersion
local _loadScript = loadScript
local _popupConfirmation = popupConfirmation
local _model = model
local _bit32 = bit32
local _string = string
local _table = table
local _math = math
local _ipairs = ipairs
local _pairs = pairs
local _tonumber = tonumber
local _tostring = tostring
local _type = type
local _select = select
local _pcall = pcall
local _collectgarbage = collectgarbage

local EXITVER = "-- EXIT (Lua r16) --"
local deviceId = 0xEE
local handsetId = 0xEF
local deviceName = nil
local lineIndex = 1
local pageOffset = 0
local edit = nil
local fieldPopup
local fieldTimeout = 0
local loadQ = {}
local fieldChunk = 0
local fieldData = nil
local fields = {}
local devices = {}
local goodBadPkt = ""
local elrsFlags = 0
local elrsFlagsInfo = ""
local fields_count = 0
local devicesRefreshTimeout = 50
local currentFolderId = nil
local commandRunningIndicator = 1
local expectChunksRemain = -1
local deviceIsELRS_TX = nil
local linkstatTimeout = 100
local titleShowWarn = nil
local titleShowWarnTimeout = 100
local exitscript = 0

-- Cached visible fields for faster navigation
local visibleFields = {}
local visibleFieldsCount = 0

local COL1
local COL2
local maxLineIndex
local textYoffset
local textSize
local barTextSpacing

-- Pre-allocated character tables for string building
local charCache = {}
for i = 0, 255 do
  charCache[i] = _string.char(i)
end

-- Animation pattern cache
local spinnerChars = "|/-\\"

-- Arrow character mapping cache
local arrowMap = {
  [192] = CHAR_UP or (__opentx and __opentx.CHAR_UP),
  [193] = CHAR_DOWN or (__opentx and __opentx.CHAR_DOWN)
}

local function allocateFields()
  -- fields table is real fields, then the Other Devices item, then devices, then Exit/Back
  fields = {}
  for i = 1, fields_count do
    fields[i] = {}
  end
  fields[#fields+1] = {id = fields_count + 1, name = "Other Devices", parent = 255, type = 16}
  fields[#fields+1] = {name = EXITVER, type = 14}
end

local function createDeviceFields() -- put other devices in the field list
  -- move back button to the end of the list, so it will always show up at the bottom.
  fields[fields_count + #devices + 2] = fields[#fields]
  for i = 1, #devices do
    local parent = (devices[i].id == deviceId) and 255 or (fields_count + 1)
    fields[fields_count + 1 + i] = {id = devices[i].id, name = devices[i].name, parent = parent, type = 15}
  end
end

local function reloadAllField()
  fieldTimeout = 0
  fieldChunk = 0
  fieldData = nil
  -- loadQ is actually a stack - pre-allocate with known size
  loadQ = {}
  local idx = 0
  for fieldId = fields_count, 1, -1 do
    idx = idx + 1
    loadQ[idx] = fieldId
  end
end

-- Build visible fields cache when folder changes
local function rebuildVisibleFieldsCache()
  visibleFields = {}
  visibleFieldsCount = 0
  for i = 1, #fields do
    local field = fields[i]
    if currentFolderId == field.parent and not field.hidden then
      visibleFieldsCount = visibleFieldsCount + 1
      visibleFields[visibleFieldsCount] = field
    end
  end
end

-- Track folder changes to rebuild cache
local lastFolderId = nil

local function getField(line)
  -- Rebuild cache if folder changed
  if currentFolderId ~= lastFolderId then
    rebuildVisibleFieldsCache()
    lastFolderId = currentFolderId
  end
  return visibleFields[line]
end

-- Count visible fields for navigation
local function getVisibleFieldCount()
  if currentFolderId ~= lastFolderId then
    rebuildVisibleFieldsCache()
    lastFolderId = currentFolderId
  end
  return visibleFieldsCount
end

local function incrField(step)
  local field = getField(lineIndex)
  if not field then return end
  
  local min, max = 0, 0
  local ftype = field.type
  if ftype <= 8 then
    min = field.min or 0
    max = field.max or 0
    step = (field.step or 1) * step
  elseif ftype == 9 then
    min = 0
    max = #field.values - 1
  end

  local newval = field.value
  local values = field.values
  repeat
    newval = newval + step
    if newval < min then
      newval = min
    elseif newval > max then
      newval = max
    end

    -- keep looping until a non-blank selection value is found
    if values == nil or #values[newval + 1] ~= 0 then
      field.value = newval
      return
    end
  until (newval == min or newval == max)
end

-- Select the next or previous editable field
local function selectField(step)
  local newLineIndex = lineIndex
  local field
  local visibleCount = getVisibleFieldCount()
  
  repeat
    newLineIndex = newLineIndex + step
    if newLineIndex <= 0 then
      newLineIndex = visibleCount
    elseif newLineIndex > visibleCount then
      newLineIndex = 1
      pageOffset = 0
    end
    field = getField(newLineIndex)
  until newLineIndex == lineIndex or (field and field.name)
  
  lineIndex = newLineIndex
  if lineIndex > maxLineIndex + pageOffset then
    pageOffset = lineIndex - maxLineIndex
  elseif lineIndex <= pageOffset then
    pageOffset = lineIndex - 1
  end
end

local function fieldGetStrOrOpts(data, offset, last, isOpts)
  -- For isOpts: Split a table of byte values (string) with ; separator into a table
  -- Else just read a string until the first null byte
  local r = last or (isOpts and {})
  
  if not last then
    -- Use table for building string (much faster than concatenation)
    local optParts = {}
    local optIdx = 0
    local vcnt = 0
    
    repeat
      local b = data[offset]
      offset = offset + 1
      
      if b == 59 or b == 0 then -- ';' or null terminator
        if optIdx > 0 then
          vcnt = vcnt + 1
          r[vcnt] = _table.concat(optParts, "", 1, optIdx)
          optIdx = 0
        end
      elseif b ~= 0 then
        optIdx = optIdx + 1
        -- Use arrow mapping or char cache
        optParts[optIdx] = arrowMap[b] or charCache[b]
      end
    until b == 0
    
    return r, offset, vcnt
  else
    -- Just find the null terminator
    repeat
      offset = offset + 1
    until data[offset - 1] == 0
    return r, offset, 0
  end
end

local function getDevice(id)
  for _, device in _ipairs(devices) do
    if device.id == id then
      return device
    end
  end
end

-- Optimized fieldGetValue with local bit operations
local _lshift = _bit32.lshift
local _band = _bit32.band

local function fieldGetValue(data, offset, size)
  local result = 0
  for i = 0, size - 1 do
    result = _lshift(result, 8) + data[offset + i]
  end
  return result
end

local function reloadCurField()
  local field = getField(lineIndex)
  if not field then return end
  fieldTimeout = 0
  fieldChunk = 0
  fieldData = nil
  loadQ[#loadQ + 1] = field.id
end

-- UINT8/INT8/UINT16/INT16 + FLOAT + TEXTSELECT
local function fieldUnsignedLoad(field, data, offset, size, unitoffset)
  field.value = fieldGetValue(data, offset, size)
  field.min = fieldGetValue(data, offset + size, size)
  field.max = fieldGetValue(data, offset + 2 * size, size)
  field.unit = fieldGetStrOrOpts(data, offset + (unitoffset or (4 * size)), field.unit)
  -- Only store the size if it isn't 1 (covers most fields / selection)
  if size ~= 1 then
    field.size = size
  end
end

local function fieldUnsignedToSigned(field, size)
  local bandval = _lshift(0x80, (size - 1) * 8)
  field.value = field.value - _band(field.value, bandval) * 2
  field.min = field.min - _band(field.min, bandval) * 2
  field.max = field.max - _band(field.max, bandval) * 2
end

local function fieldSignedLoad(field, data, offset, size, unitoffset)
  fieldUnsignedLoad(field, data, offset, size, unitoffset)
  fieldUnsignedToSigned(field, size)
  -- signed ints are INTdicated by a negative size
  field.size = -size
end

local function fieldIntLoad(field, data, offset)
  -- Type is U8/I8/U16/I16, use that to determine the size and signedness
  local ftype = field.type
  local loadFn = (ftype % 2 == 0) and fieldUnsignedLoad or fieldSignedLoad
  loadFn(field, data, offset, _math.floor(ftype / 2) + 1)
end

local _rshift = _bit32.rshift

local function fieldIntSave(field)
  local value = field.value
  local size = field.size or 1
  -- Convert signed to 2s complement
  if size < 0 then
    size = -size
    if value < 0 then
      value = _lshift(0x100, (size - 1) * 8) + value
    end
  end

  local frame = {deviceId, handsetId, field.id}
  for i = size - 1, 0, -1 do
    frame[#frame + 1] = _rshift(value, 8 * i) % 256
  end
  _crossfireTelemetryPush(0x2D, frame)
end

-- Cache for value+unit concatenation
local displayCache = {}

local function fieldIntDisplay(field, y, attr)
  -- Simple concatenation is faster than format for this case
  _lcd.drawText(COL2, y, field.value .. field.unit, attr)
end

-- FLOAT
local function fieldFloatLoad(field, data, offset)
  fieldSignedLoad(field, data, offset, 4, 21)
  local prec = data[offset + 16]
  if prec > 3 then
    prec = 3
  end
  field.prec = prec
  field.step = fieldGetValue(data, offset + 17, 4)

  -- precompute the format string to preserve the precision
  field.fmt = "%." .. prec .. "f" .. field.unit
  -- Convert precision to a divider
  field.prec = 10 ^ prec
end

local _format = _string.format

local function fieldFloatDisplay(field, y, attr)
  _lcd.drawText(COL2, y, _format(field.fmt, field.value / field.prec), attr)
end

-- TEXT SELECTION
local function fieldTextSelLoad(field, data, offset)
  local vcnt
  local cached = field.nc == nil and field.values
  field.values, offset, vcnt = fieldGetStrOrOpts(data, offset, cached, true)
  -- 'Disable' the line if values only has one option in the list
  if not cached then
    field.grey = vcnt <= 1
  end
  field.value = data[offset]
  -- units never uses cache
  field.unit = fieldGetStrOrOpts(data, offset + 4)
  field.nc = nil -- use cache next time
end

-- Consolidated text selection display
local function fieldTextSelDisplay(field, y, attr, color, isColor)
  local val = field.values[field.value + 1] or "ERR"
  if isColor then
    _lcd.drawText(COL2, y, val, attr + color)
    local strPix = _lcd.sizeText and _lcd.sizeText(val) or (10 * #val)
    _lcd.drawText(COL2 + strPix, y, field.unit, color)
  else
    _lcd.drawText(COL2, y, val, attr)
    _lcd.drawText(_lcd.getLastPos(), y, field.unit, 0)
  end
end

-- Color version
local function fieldTextSelDisplay_color(field, y, attr, color)
  fieldTextSelDisplay(field, y, attr, color, true)
end

-- B&W version
local function fieldTextSelDisplay_bw(field, y, attr)
  fieldTextSelDisplay(field, y, attr, 0, false)
end

-- STRING
local function fieldStringLoad(field, data, offset)
  field.value, offset = fieldGetStrOrOpts(data, offset)
  if #data >= offset then
    field.maxlen = data[offset]
  end
end

local function fieldStringDisplay(field, y, attr)
  _lcd.drawText(COL2, y, field.value, attr)
end

local function fieldFolderOpen(field)
  currentFolderId = field.id
  local backFld = fields[#fields]
  backFld.name = "----BACK----"
  -- Store the lineIndex and pageOffset to return to in the backFld
  backFld.li = lineIndex
  backFld.po = pageOffset
  backFld.parent = currentFolderId

  lineIndex = 1
  pageOffset = 0
  -- Invalidate visible fields cache
  lastFolderId = nil
end

local function fieldFolderDeviceOpen(field)
  -- Make sure device fields are in the folder when it opens
  createDeviceFields()
  return fieldFolderOpen(field)
end

local function fieldFolderDisplay(field, y, attr)
  _lcd.drawText(COL1, y, "> " .. field.name, attr + BOLD)
end

local function fieldCommandLoad(field, data, offset)
  field.status = data[offset]
  field.timeout = data[offset + 1]
  field.info = fieldGetStrOrOpts(data, offset + 2)
  if field.status == 0 then
    fieldPopup = nil
  end
end

local function fieldCommandSave(field)
  reloadCurField()

  if field.status ~= nil then
    if field.status < 4 then
      field.status = 1
      _crossfireTelemetryPush(0x2D, {deviceId, handsetId, field.id, field.status})
      fieldPopup = field
      fieldPopup.lastStatus = 0
      fieldTimeout = _getTime() + field.timeout
    end
  end
end

local function fieldCommandDisplay(field, y, attr)
  _lcd.drawText(10, y, "[" .. field.name .. "]", attr + BOLD)
end

local function fieldBackExec(field)
  if field.parent then
    lineIndex = field.li or 1
    pageOffset = field.po or 0

    field.name = EXITVER
    field.parent = nil
    field.li = nil
    field.po = nil
    currentFolderId = nil
    -- Invalidate visible fields cache
    lastFolderId = nil
  else
    exitscript = 1
  end
end

local function changeDeviceId(devId) --change to selected device ID
  local device = getDevice(devId)
  if deviceId == devId and fields_count == device.fldcnt then return end

  deviceId = devId
  elrsFlags = 0
  currentFolderId = nil
  -- Invalidate cache
  lastFolderId = nil
  deviceName = device.name
  fields_count = device.fldcnt
  deviceIsELRS_TX = device.isElrs and devId == 0xEE or nil -- ELRS and ID is TX module
  handsetId = deviceIsELRS_TX and 0xEF or 0xEA -- Address ELRS_LUA vs RADIO_TRANSMITTER

  allocateFields()
  reloadAllField()
end

local function fieldDeviceIdSelect(field)
  return changeDeviceId(field.id)
end

-- Cache for device lookup by id
local deviceIdCache = {}

local function parseDeviceInfoMessage(data)
  local id = data[2]
  local newName, offset = fieldGetStrOrOpts(data, 3)
  local device = deviceIdCache[id] or getDevice(id)
  if device == nil then
    device = {id = id}
    devices[#devices + 1] = device
    deviceIdCache[id] = device
  end
  device.name = newName
  device.fldcnt = data[offset + 12]
  device.isElrs = fieldGetValue(data, offset, 4) == 0x454C5253 -- SerialNumber = 'E L R S'

  if deviceId == id then
    changeDeviceId(id)
  end
  -- DeviceList change while in Other Devices, refresh list
  if currentFolderId == fields_count + 1 then
    createDeviceFields()
    -- Invalidate cache since device fields changed
    lastFolderId = nil
  end
end

-- Cached field type functions lookup
local fieldFunctions = {
  {load = fieldIntLoad, save = fieldIntSave, display = fieldIntDisplay}, --1 UINT8(0)
  {load = fieldIntLoad, save = fieldIntSave, display = fieldIntDisplay}, --2 INT8(1)
  {load = fieldIntLoad, save = fieldIntSave, display = fieldIntDisplay}, --3 UINT16(2)
  {load = fieldIntLoad, save = fieldIntSave, display = fieldIntDisplay}, --4 INT16(3)
  nil,
  nil,
  nil,
  nil,
  {load = fieldFloatLoad, save = fieldIntSave, display = fieldFloatDisplay}, --9 FLOAT(8)
  {load = fieldTextSelLoad, save = fieldIntSave, display = nil}, --10 SELECT(9)
  {load = fieldStringLoad, save = nil, display = fieldStringDisplay}, --11 STRING(10) editing NOTIMPL
  {load = nil, save = fieldFolderOpen, display = fieldFolderDisplay}, --12 FOLDER(11)
  {load = fieldStringLoad, save = nil, display = fieldStringDisplay}, --13 INFO(12)
  {load = fieldCommandLoad, save = fieldCommandSave, display = fieldCommandDisplay}, --14 COMMAND(13)
  {load = nil, save = fieldBackExec, display = fieldCommandDisplay}, --15 back/exit(14)
  {load = nil, save = fieldDeviceIdSelect, display = fieldCommandDisplay}, --16 device(15)
  {load = nil, save = fieldFolderDeviceOpen, display = fieldFolderDisplay}, --17 deviceFOLDER(16)
}

-- Fast lookup for field functions
local function getFieldFunctions(fieldType)
  return fieldFunctions[fieldType + 1]
end

local _btest = _bit32.btest
local _band_func = _bit32.band

local function parseParameterInfoMessage(data)
  local fieldId = (fieldPopup and fieldPopup.id) or loadQ[#loadQ]
  if data[2] ~= deviceId or data[3] ~= fieldId then
    fieldData = nil
    fieldChunk = 0
    return
  end
  local field = fields[fieldId]
  local chunksRemain = data[4]
  -- If no field or the chunksremain changed when we have data, don't continue
  if not field or (fieldData and chunksRemain ~= expectChunksRemain) then
    return
  end

  local offset
  -- If data is chunked, copy it to persistent buffer
  if chunksRemain > 0 or fieldChunk > 0 then
    fieldData = fieldData or {}
    for i = 5, #data do
      fieldData[#fieldData + 1] = data[i]
    end
    offset = 1
  else
    -- All data arrived in one chunk, operate directly on data
    fieldData = data
    offset = 5
  end

  if chunksRemain > 0 then
    fieldChunk = fieldChunk + 1
    expectChunksRemain = chunksRemain - 1
  else
    -- Field data stream is now complete, process into a field
    loadQ[#loadQ] = nil

    if #fieldData > (offset + 2) then
      field.id = fieldId
      field.parent = (fieldData[offset] ~= 0) and fieldData[offset] or nil
      local typeByte = fieldData[offset + 1]
      field.type = _band_func(typeByte, 0x7F)
      field.hidden = _btest(typeByte, 0x80) or nil
      field.name, offset = fieldGetStrOrOpts(fieldData, offset + 2, field.name)
      
      local funcs = getFieldFunctions(field.type)
      if funcs and funcs.load then
        funcs.load(field, fieldData, offset)
      end
      
      if field.min == 0 then field.min = nil end
      if field.max == 0 then field.max = nil end
    end

    fieldChunk = 0
    fieldData = nil
    -- Invalidate visible fields cache since field data changed
    lastFolderId = nil

    -- Return value is if the screen should be updated
    -- If deviceId is TX module, then the Bad/Good drives the update; for other
    -- devices update each new item. and always update when the queue empties
    return deviceId ~= 0xEE or #loadQ == 0
  end
end

local function parseElrsInfoMessage(data)
  if data[2] ~= deviceId then
    fieldData = nil
    fieldChunk = 0
    return
  end

  local badPkt = data[3]
  local goodPkt = (data[4] * 256) + data[5]
  local newFlags = data[6]
  -- If flags are changing, reset the warning timeout to display/hide message immediately
  if newFlags ~= elrsFlags then
    elrsFlags = newFlags
    titleShowWarnTimeout = 0
  end
  elrsFlagsInfo = fieldGetStrOrOpts(data, 7)

  local state = (_btest(elrsFlags, 1) and "C") or "-"
  goodBadPkt = badPkt .. "/" .. goodPkt .. "   " .. state
end

local function parseElrsV1Message(data)
  if (data[1] ~= 0xEA) or (data[2] ~= 0xEE) then
    return
  end

  fieldPopup = {id = 0, status = 2, timeout = 0xFF, info = "ERROR: 1.x firmware"}
  fieldTimeout = _getTime() + 0xFFFF
end

local function refreshNext(skipPush)
  local command, data, forceRedraw
  repeat
    command, data = _crossfireTelemetryPop()
    if command == 0x29 then
      parseDeviceInfoMessage(data)
    elseif command == 0x2B then
      if parseParameterInfoMessage(data) then
        forceRedraw = true
      end
      if #loadQ > 0 then
        fieldTimeout = 0 -- request next chunk immediately
      elseif fieldPopup then
        fieldTimeout = _getTime() + fieldPopup.timeout
      end
    elseif command == 0x2D then
      parseElrsV1Message(data)
    elseif command == 0x2E then
      parseElrsInfoMessage(data)
      forceRedraw = true
    end
  until command == nil

  -- Don't even bother with return value, skipPush implies redraw
  if skipPush then return end

  local time = _getTime()
  if fieldPopup then
    if time > fieldTimeout and fieldPopup.status ~= 3 then
      _crossfireTelemetryPush(0x2D, {deviceId, handsetId, fieldPopup.id, 6}) -- lcsQuery
      fieldTimeout = time + fieldPopup.timeout
    end
  elseif time > devicesRefreshTimeout and #devices == 0 then
    forceRedraw = true -- handles initial screen draw
    devicesRefreshTimeout = time + 100 -- 1s
    _crossfireTelemetryPush(0x28, {0x00, 0xEA})
  elseif time > linkstatTimeout then
    if deviceIsELRS_TX then
      _crossfireTelemetryPush(0x2D, {deviceId, handsetId, 0x0, 0x0}) --request linkstat
    else
      goodBadPkt = ""
    end
    linkstatTimeout = time + 100
  elseif time > fieldTimeout and fields_count ~= 0 then
    if #loadQ > 0 then
      _crossfireTelemetryPush(0x2C, {deviceId, handsetId, loadQ[#loadQ], fieldChunk})
      fieldTimeout = time + (deviceIsELRS_TX and 50 or 500) -- 0.5s for local / 5s for remote devices
    end
  end

  if time > titleShowWarnTimeout then
    -- if elrsFlags bit set is bit higher than bit 0 and bit 1, it is warning flags
    titleShowWarn = (elrsFlags > 3 and not titleShowWarn) or nil
    titleShowWarnTimeout = time + 100
    forceRedraw = true
  end

  return forceRedraw
end

local lcd_title -- holds function that is color/bw version
local function lcd_title_color()
  _lcd.clear()

  local EBLUE = _lcd.RGB(0x43, 0x61, 0xAA)
  local EGREEN = _lcd.RGB(0x9f, 0xc7, 0x6f)
  local EGREY1 = _lcd.RGB(0x91, 0xb2, 0xc9)
  local EGREY2 = _lcd.RGB(0x6f, 0x62, 0x7f)

  -- Field display area (white w/ 2px green border)
  _lcd.setColor(CUSTOM_COLOR, EGREEN)
  _lcd.drawRectangle(0, 0, LCD_W, LCD_H, CUSTOM_COLOR)
  _lcd.drawRectangle(1, 0, LCD_W - 2, LCD_H - 1, CUSTOM_COLOR)
  -- title bar
  _lcd.drawFilledRectangle(0, 0, LCD_W, barHeight, CUSTOM_COLOR)
  _lcd.setColor(CUSTOM_COLOR, EGREY1)
  _lcd.drawFilledRectangle(LCD_W - textSize, 0, textSize, barHeight, CUSTOM_COLOR)
  _lcd.setColor(CUSTOM_COLOR, EGREY2)
  _lcd.drawRectangle(LCD_W - textSize, 0, textSize, barHeight - 1, CUSTOM_COLOR)
  _lcd.drawRectangle(LCD_W - textSize, 1, textSize - 1, barHeight - 2, CUSTOM_COLOR) -- left and bottom line only 1px, make it look bevelled
  _lcd.setColor(CUSTOM_COLOR, BLACK)
  if titleShowWarn then
    _lcd.drawText(COL1 + 1, barTextSpacing, elrsFlagsInfo, CUSTOM_COLOR)
  else
    _lcd.drawText(COL1 + 1, barTextSpacing, deviceName, CUSTOM_COLOR)
    _lcd.drawText(LCD_W - 5, barTextSpacing, goodBadPkt, RIGHT + BOLD + CUSTOM_COLOR)
  end
  -- progress bar
  if #loadQ > 0 and fields_count > 0 then
    local barW = (COL2 - 4) * (fields_count - #loadQ) / fields_count
    _lcd.setColor(CUSTOM_COLOR, EBLUE)
    _lcd.drawFilledRectangle(2, barTextSpacing / 2 + textSize, barW, barTextSpacing, CUSTOM_COLOR)
    _lcd.setColor(CUSTOM_COLOR, WHITE)
    _lcd.drawFilledRectangle(2 + barW, barTextSpacing / 2 + textSize, COL2 - 2 - barW, barTextSpacing, CUSTOM_COLOR)
  end
end

local function lcd_title_bw()
  _lcd.clear()
  -- B&W screen
  local barHeight = 9
  if not titleShowWarn then
    _lcd.drawText(LCD_W - 1, 1, goodBadPkt, RIGHT)
    _lcd.drawLine(LCD_W - 10, 0, LCD_W - 10, barHeight - 1, SOLID, INVERS)
  end

  if #loadQ > 0 and fields_count > 0 then
    _lcd.drawFilledRectangle(COL2, 0, LCD_W, barHeight, GREY_DEFAULT)
    _lcd.drawGauge(0, 0, COL2, barHeight, fields_count - #loadQ, fields_count, 0)
  else
    _lcd.drawFilledRectangle(0, 0, LCD_W, barHeight, GREY_DEFAULT)
    if titleShowWarn then
      _lcd.drawText(COL1, 1, elrsFlagsInfo, INVERS)
    else
      _lcd.drawText(COL1, 1, deviceName, INVERS)
    end
  end
end

local function lcd_warn()
  _lcd.drawText(COL1, textSize * 2, "Error:")
  _lcd.drawText(COL1, textSize * 3, elrsFlagsInfo)
  _lcd.drawText(LCD_W / 2, textSize * 5, "[OK]", BLINK + INVERS + CENTER)
end

local function reloadRelatedFields(field)
  -- Reload the parent folder to update the description
  if field.parent then
    loadQ[#loadQ + 1] = field.parent
    fields[field.parent].name = nil
  end

  local parentId = field.parent
  local fieldId = field.id
  
  -- Reload all editable fields at the same level as well as the parent item
  for fid = fields_count, 1, -1 do
    -- Skip this field, will be added to end
    local fldTest = fields[fid]
    local fldType = fldTest.type or 99 -- type could be nil if still loading
    if fid ~= fieldId
      and fldTest.parent == parentId
      and (fldType < 11 or fldType == 12) then -- ignores FOLDER/COMMAND/devices/EXIT
      fldTest.nc = true -- "no cache" the options
      loadQ[#loadQ + 1] = fid
    end
  end

  -- Reload this field
  loadQ[#loadQ + 1] = fieldId
  -- with a short delay to allow the module EEPROM to commit
  fieldTimeout = _getTime() + 20
  -- Also push the next bad/good update further out
  linkstatTimeout = fieldTimeout + 100
end

local function handleDevicePageEvent(event)
  if #fields == 0 then --if there is no field yet
    return
  else
    if fields[#fields].name == nil then --if back button is not assigned yet, means there is no field yet.
      return
    end
  end

  if event == EVT_VIRTUAL_EXIT then -- Cancel edit / go up a folder / reload all
    if edit then
      edit = nil
      reloadCurField()
    else
      if currentFolderId == nil and #loadQ == 0 then -- only do reload if we're in the root folder and finished loading
        if deviceId ~= 0xEE then
          changeDeviceId(0xEE)
        else
          reloadAllField()
        end
        _crossfireTelemetryPush(0x28, {0x00, 0xEA})
      else
        fieldBackExec(fields[#fields])
      end
    end
  elseif event == EVT_VIRTUAL_ENTER then -- toggle editing/selecting current field
    if elrsFlags > 0x1F then
      elrsFlags = 0
      _crossfireTelemetryPush(0x2D, {deviceId, handsetId, 0x2E, 0x00})
    else
      local field = getField(lineIndex)
      if field and field.name then
        -- Editable fields
        if not field.grey and field.type < 10 then
          edit = not edit
          if not edit then
            reloadRelatedFields(field)
          end
        end
        if not edit then
          local funcs = getFieldFunctions(field.type)
          if funcs and funcs.save then
            funcs.save(field)
          end
        end
      end
    end
  elseif edit then
    if event == EVT_VIRTUAL_NEXT then
      incrField(1)
    elseif event == EVT_VIRTUAL_PREV then
      incrField(-1)
    end
  else
    if event == EVT_VIRTUAL_NEXT then
      selectField(1)
    elseif event == EVT_VIRTUAL_PREV then
      selectField(-1)
    end
  end
end

-- Main
local function runDevicePage(event)
  handleDevicePageEvent(event)

  lcd_title()

  if #devices > 1 then -- show other device folder
    fields[fields_count + 1].parent = nil
  end
  if elrsFlags > 0x1F then
    lcd_warn()
  else
    for y = 1, maxLineIndex + 1 do
      local field = getField(pageOffset + y)
      if not field then
        break
      elseif field.name ~= nil then
        local attr = lineIndex == (pageOffset + y)
          and ((edit and BLINK or 0) + INVERS)
          or 0
        local color = field.grey and COLOR_THEME_DISABLED or 0
        local ftype = field.type
        if ftype < 11 or ftype == 12 then -- if not folder, command, or back
          _lcd.drawText(COL1, y * textSize + textYoffset, field.name, color)
        end
        local funcs = getFieldFunctions(ftype)
        if funcs and funcs.display then
          funcs.display(field, y * textSize + textYoffset, attr, color)
        end
      end
    end
  end
end

local function popupCompat(t, m, e)
  -- Only use 2 of 3 arguments for older platforms
  return _popupConfirmation(t, e)
end

local function runPopupPage(event)
  if event == EVT_VIRTUAL_EXIT then
    _crossfireTelemetryPush(0x2D, {deviceId, handsetId, fieldPopup.id, 5}) -- lcsCancel
    fieldTimeout = _getTime() + 200 -- 2s
  end

  if fieldPopup.status == 0 and fieldPopup.lastStatus ~= 0 then -- stopped
    popupCompat(fieldPopup.info, "Stopped!", event)
    reloadAllField()
    fieldPopup = nil
  elseif fieldPopup.status == 3 then -- confirmation required
    local result = popupCompat(fieldPopup.info, "PRESS [OK] to confirm", event)
    fieldPopup.lastStatus = fieldPopup.status
    if result == "OK" then
      _crossfireTelemetryPush(0x2D, {deviceId, handsetId, fieldPopup.id, 4}) -- lcsConfirmed
      fieldTimeout = _getTime() + fieldPopup.timeout -- we are expecting an immediate response
      fieldPopup.status = 4
    elseif result == "CANCEL" then
      fieldPopup = nil
    end
  elseif fieldPopup.status == 2 then -- running
    if fieldChunk == 0 then
      commandRunningIndicator = (commandRunningIndicator % 4) + 1
    end
    local spinner = spinnerChars:sub(commandRunningIndicator, commandRunningIndicator)
    local result = popupCompat(fieldPopup.info .. " [" .. spinner .. "]", "Press [RTN] to exit", event)
    fieldPopup.lastStatus = fieldPopup.status
    if result == "CANCEL" then
      _crossfireTelemetryPush(0x2D, {deviceId, handsetId, fieldPopup.id, 5}) -- lcsCancel
      fieldTimeout = _getTime() + fieldPopup.timeout -- we are expecting an immediate response
      fieldPopup = nil
    end
  end
end

local function touch2evt(event, touchState)
  -- Convert swipe events to normal events Left/Right/Up/Down -> EXIT/ENTER/PREV/NEXT
  -- PREV/NEXT are swapped if editing
  -- TAP is converted to ENTER
  touchState = touchState or {}
  return (touchState.swipeLeft and EVT_VIRTUAL_EXIT)
    or (touchState.swipeRight and EVT_VIRTUAL_ENTER)
    or (touchState.swipeUp and (edit and EVT_VIRTUAL_NEXT or EVT_VIRTUAL_PREV))
    or (touchState.swipeDown and (edit and EVT_VIRTUAL_PREV or EVT_VIRTUAL_NEXT))
    or (event == EVT_TOUCH_TAP and EVT_VIRTUAL_ENTER)
end

local function setLCDvar()
  -- Set the title function depending on if LCD is color, and free the other function and
  -- set textselection unit function, use GetLastPost or sizeText
  if (_lcd.RGB ~= nil) then
    lcd_title = lcd_title_color
    fieldFunctions[10].display = fieldTextSelDisplay_color
  else
    lcd_title = lcd_title_bw
    fieldFunctions[10].display = fieldTextSelDisplay_bw
    touch2evt = nil
  end
  -- Determine if popupConfirmation takes 3 arguments or 2
  -- major 1 is assumed to be FreedomTX
  local _, _, major = _getVersion()
  if major ~= 1 then
    popupCompat = _popupConfirmation
  end

  if (_lcd.RGB ~= nil) then
    local ver, radio, maj, minor, rev, osname = _getVersion()

    if osname ~= nil and osname == "EdgeTX" then
      textWidth, textSize = _lcd.sizeText("Qg") -- determine standard font height for EdgeTX
    else
      textSize = 21 -- use this for OpenTX
    end

    COL1 = 3
    COL2 = LCD_W / 2
    barTextSpacing = 4
    barHeight = textSize + barTextSpacing + barTextSpacing
    textYoffset = 2 * barTextSpacing + 2
    maxLineIndex = _math.floor(((LCD_H - barHeight - textYoffset) / textSize)) - 1
  else
    if LCD_W == 212 then
      COL2 = 110
    else
      COL2 = 70
    end
    if LCD_H == 96 then
      maxLineIndex = 9
    else
      maxLineIndex = 6
    end
    COL1 = 0
    textYoffset = 3
    textSize = 8
  end
end

local function setMock()
  -- Setup fields to display if running in Simulator
  local _, rv = _getVersion()
  if _string.sub(rv, -5) ~= "-simu" then return end
  local mock = _loadScript("mockup/elrsmock.lua")
  if mock == nil then return end
  fields, goodBadPkt, deviceName = mock()
  fields_count = #fields - 1
  loadQ = {fields_count}
  deviceIsELRS_TX = true
end

-- Error message lines - pre-allocated
local crsfErrorMsgs = {
  " Enable a CRSF Internal",
  "   or External module in",
  "       Model settings",
  "  If module is internal",
  " also set Internal RF to",
  " CRSF in SYS->Hardware",
}

local function checkCrsfModule()
  -- Loop through the modules and look for one set to CRSF (5)
  for modIdx = 0, 1 do
    local mod = _model.getModule(modIdx)
    if mod and (mod.Type == nil or mod.Type == 5) then
      -- CRSF found, put module type in Loading message
      local modDescrip = (mod.Type == nil) and " awaiting" or (modIdx == 0) and " Internal" or " External"
      -- Prefix with "Lua rXXX" from between EXITVER parens
      deviceName = _string.match(EXITVER, "%((.*)%)") .. modDescrip .. " TX..."
      checkCrsfModule = nil
      return 0
    end
  end

  -- No CRSF module found, save an error message for run()
  _lcd.clear()
  local y = 0
  _lcd.drawText(2, y, "  No ExpressLRS", MIDSIZE)
  y = y + (textSize * 2) - 2
  
  for i, msg in _ipairs(crsfErrorMsgs) do
    _lcd.drawText(2, y, msg)
    y = y + textSize
    if i == 3 then
      _lcd.drawLine(0, y, LCD_W, y, SOLID, INVERS)
      y = y + 2
    end
  end

  return 0
end

-- Init
local function init()
  setLCDvar()
  setMock()
  setLCDvar = nil
  setMock = nil
end

-- Main
local function run(event, touchState)
  if event == nil then return 2 end
  if checkCrsfModule then return checkCrsfModule() end

  event = (touch2evt and touch2evt(event, touchState)) or event
  -- If ENTER pressed, skip any pushing this loop to reserve queue for the save command
  local forceRedraw = refreshNext(event == EVT_VIRTUAL_ENTER)

  if fieldPopup ~= nil then
    runPopupPage(event)
  elseif event ~= 0 or forceRedraw or edit then
    runDevicePage(event)
  end

  return exitscript
end

return {init = init, run = run}
