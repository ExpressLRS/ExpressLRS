-- TNS|ExpressLRS|TNE
---- #########################################################################
---- #                                                                       #
---- # Copyright (C) OpenTX, adapted for ExpressLRS                          #
-----#                                                                       #
---- # License GPLv2: http://www.gnu.org/licenses/gpl-2.0.html               #
---- #                                                                       #
---- #########################################################################
local deviceId = 0xEE
local handsetId = 0xEF
local deviceName = ""
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
local goodBadPkt = "?/???    ?"
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

local COL1
local COL2
local maxLineIndex
local textYoffset
local textSize

local function allocateFields()
  fields = {}
  for i=1, fields_count + 2 + #devices do
    fields[i] = { }
  end
  fields[#fields] = {name="----EXIT----", type=14}
end

local function reloadAllField()
  fieldChunk = 0
  fieldData = nil
  -- loadQ is actually a stack
  loadQ = {}
  for fieldId = fields_count, 1, -1 do
    loadQ[#loadQ+1] = fieldId
  end
end

local function getField(line)
  local counter = 1
  for i = 1, #fields do
    local field = fields[i]
    if currentFolderId == field.parent and not field.hidden then
      if counter < line then
        counter = counter + 1
      else
        return field
      end
    end
  end
end

local function incrField(step)
  local field = getField(lineIndex)
  local min, max = 0, 0
  if field.type <= 8 then
    min = field.min or 0
    max = field.max or 0
    step = (field.step or 1) * step
  elseif field.type == 9 then
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

    -- keep looping until a non-blank selection value is found
    if field.values == nil or #field.values[newval+1] ~= 0 then
      field.value = newval
      return
    end
  until (newval == min or newval == max)
end

-- Select the next or previous editable field
local function selectField(step)
  local newLineIndex = lineIndex
  local field
  repeat
    newLineIndex = newLineIndex + step
    if newLineIndex <= 0 then
      newLineIndex = #fields
    elseif newLineIndex == 1 + #fields then
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
  if last then
    while data[offset] ~= 0 do
      offset = offset + 1
    end
    return last, offset + 1
  end

  -- For isOpts: Split a table of byte values (string) with ; separator into a table
  -- Else just read a string until the first null byte
  local r = isOpts and {}
  local opt = ''
  local b = data[offset]
  while b ~= 0 do
    if r and b == 59 then -- ';'
      r[#r+1] = opt
      opt = ''
    else
      -- On firmwares that have constants defined for the arrow chars, use them in place of
      -- the \xc0 \xc1 chars (which are OpenTX-en)
      -- Use the table to convert the char, else use string.char if not in the table
      opt = opt .. (({
        [192] = CHAR_UP or (__opentx and __opentx.CHAR_UP),
        [193] = CHAR_DOWN or (__opentx and __opentx.CHAR_DOWN)
      })[b] or string.char(b))
    end
    offset = offset + 1
    b = data[offset]
  end

  if r then
    r[#r+1] = opt
    opt = r
  end
  return opt, offset + 1, collectgarbage("collect")
end

local function getDevice(name)
  for i=1, #devices do
    if devices[i].name == name then
      return devices[i]
    end
  end
end

local function fieldGetValue(data, offset, size)
  local result = 0
  for i=0, size-1 do
    result = bit32.lshift(result, 8) + data[offset + i]
  end
  return result
end

-- UINT8/INT8/UINT16/INT16 + FLOAT + TEXTSELECT
local function fieldUnsignedLoad(field, data, offset, size, unitoffset)
  field.value = fieldGetValue(data, offset, size)
  field.min = fieldGetValue(data, offset+size, size)
  field.max = fieldGetValue(data, offset+2*size, size)
  --field.default = fieldGetValue(data, offset+3*size, size)
  field.unit = fieldGetStrOrOpts(data, offset+(unitoffset or (4*size)), field.unit)
  -- Only store the size if it isn't 1 (covers most fields / selection)
  if size ~= 1 then
    field.size = size
  end
end

local function fieldUnsignedToSigned(field, size)
  local bandval = bit32.lshift(0x80, (size-1)*8)
  field.value = field.value - bit32.band(field.value, bandval) * 2
  field.min = field.min - bit32.band(field.min, bandval) * 2
  field.max = field.max - bit32.band(field.max, bandval) * 2
  --field.default = field.default - bit32.band(field.default, bandval) * 2
end

local function fieldSignedLoad(field, data, offset, size, unitoffset)
  fieldUnsignedLoad(field, data, offset, size, unitoffset)
  fieldUnsignedToSigned(field, size)
  -- signed ints are INTdicated by a negative size
  field.size = -size
end

local function fieldIntLoad(field, data, offset)
  -- Type is U8/I8/U16/I16, use that to determine the size and signedness
  local loadFn = (field.type % 2 == 0) and fieldUnsignedLoad or fieldSignedLoad
  loadFn(field, data, offset, math.floor(field.type / 2) + 1)
end

local function fieldIntSave(field)
  local value = field.value
  local size = field.size or 1
  -- Convert signed to 2s complement
  if size < 0 then
    size = -size
    if value < 0 then
      value = bit32.lshift(0x100, (size-1)*8) + value
    end
  end

  local frame = { deviceId, handsetId, field.id }
  for i = size-1, 0, -1 do
    frame[#frame + 1] = bit32.rshift(value, 8*i) % 256
  end
  crossfireTelemetryPush(0x2D, frame)
end

local function fieldIntDisplay(field, y, attr)
  lcd.drawText(COL2, y, field.value .. field.unit, attr)
end

-- -- FLOAT
local function fieldFloatLoad(field, data, offset)
  fieldSignedLoad(field, data, offset, 4, 21)
  field.prec = data[offset+16]
  if field.prec > 3 then
    field.prec = 3
  end
  field.step = fieldGetValue(data, offset+17, 4)

  -- precompute the format string to preserve the precision
  field.fmt = "%." .. tostring(field.prec) .. "f" .. field.unit
  -- Convert precision to a divider
  field.prec = 10 ^ field.prec
end

local function fieldFloatDisplay(field, y, attr)
  lcd.drawText(COL2, y, string.format(field.fmt, field.value / field.prec), attr)
end

-- TEXT SELECTION
local function fieldTextSelLoad(field, data, offset)
  field.values, offset = fieldGetStrOrOpts(data, offset, field.nc == nil and field.values, true)
  field.value = data[offset]
  -- min max and default (offset+1 to 3) are not used on selections
  -- units never uses cache
  field.unit = fieldGetStrOrOpts(data, offset+4)
  field.nc = nil -- use cache next time
end

local function fieldTextSelDisplay_color(field, y, attr)
  local val = field.values[field.value+1] or "ERR"
  lcd.drawText(COL2, y, val, attr)
  local strPix = lcd.sizeText and lcd.sizeText(val) or (10 * #val)
  lcd.drawText(COL2 + strPix, y, field.unit, 0)
end

local function fieldTextSelDisplay_bw(field, y, attr)
  lcd.drawText(COL2, y, field.values[field.value+1] or "ERR", attr)
  lcd.drawText(lcd.getLastPos(), y, field.unit, 0)
end

-- STRING
local function fieldStringLoad(field, data, offset)
  field.value, offset = fieldGetStrOrOpts(data, offset)
  if #data >= offset then
    field.maxlen = data[offset]
  end
end

local function fieldStringDisplay(field, y, attr)
  lcd.drawText(COL2, y, field.value, attr)
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
end

local function fieldFolderDeviceOpen(field)
  crossfireTelemetryPush(0x28, { 0x00, 0xEA }) --broadcast with standard handset ID to get all node respond correctly
  return fieldFolderOpen(field)
end

local function fieldFolderDisplay(field,y ,attr)
  lcd.drawText(COL1, y, "> " .. field.name, bit32.bor(attr, BOLD))
end

local function fieldCommandLoad(field, data, offset)
  field.status = data[offset]
  field.timeout = data[offset+1]
  field.info = fieldGetStrOrOpts(data, offset+2)
  if field.status == 0 then
    fieldPopup = nil
  end
end

local function fieldCommandSave(field)
  if field.status ~= nil then
    if field.status < 4 then
      field.status = 1
      crossfireTelemetryPush(0x2D, { deviceId, handsetId, field.id, field.status })
      fieldPopup = field
      fieldPopup.lastStatus = 0
      fieldTimeout = getTime() + field.timeout
    end
  end
end

local function fieldCommandDisplay(field, y, attr)
    lcd.drawText(10, y, "[" .. field.name .. "]", bit32.bor(attr, BOLD))
end

local function fieldBackExec(field)
  if field.parent then
    lineIndex = field.li or 1
    pageOffset = field.po or 0

    field.name = "----EXIT----"
    field.parent = nil
    field.li = nil
    field.po = nil
    currentFolderId = nil
  else
    exitscript = 1
  end
end

local function changeDeviceId(devId) --change to selected device ID
  currentFolderId = nil
  deviceIsELRS_TX = nil
  elrsFlags = 0
  --if the selected device ID (target) is a TX Module, we use our Lua ID, so TX Flag that user is using our LUA
  if devId == 0xEE then
    handsetId = 0xEF
  else --else we would act like the legacy lua
    handsetId = 0xEA
  end
  deviceId = devId
  fields_count = 0  --set this because next target wouldn't have the same count, and this trigger to request the new count
end

local function fieldDeviceIdSelect(field)
  local device = getDevice(field.name)
  changeDeviceId(device.id)
  crossfireTelemetryPush(0x28, { 0x00, 0xEA })
end

local function createDeviceFields() -- put other devices in the field list
  -- move back button to the end of the list, so it will always show up at the bottom.
  fields[fields_count + 2 + #devices] = fields[#fields]
  for i=1, #devices do
    local parent = (devices[i].id == deviceId) and 255 or (fields_count+1)
    fields[fields_count+1+i] = {name=devices[i].name, parent=parent, type=15}
  end
end

local function parseDeviceInfoMessage(data)
  local offset
  local id = data[2]
  local newName
  newName, offset = fieldGetStrOrOpts(data, 3)
  local device = getDevice(newName)
  if device == nil then
    device = { id = id, name = newName }
    devices[#devices + 1] = device
  end
  if deviceId == id then
    deviceName = newName
    deviceIsELRS_TX = ((fieldGetValue(data,offset,4) == 0x454C5253) and (deviceId == 0xEE)) or nil -- SerialNumber = 'E L R S' and ID is TX module
    local newFieldCount = data[offset+12]
    if newFieldCount ~= fields_count or newFieldCount == 0 then
      fields_count = newFieldCount
      allocateFields()
      reloadAllField()
      fields[fields_count+1] = {id = fields_count+1, name="Other Devices", parent = 255, type=16} -- add other devices folders
      if newFieldCount == 0 then
        -- This device has no fields so the Loading code never starts
        createDeviceFields()
      end
    end
  end
end

local functions = {
  { load=fieldIntLoad, save=fieldIntSave, display=fieldIntDisplay }, --1 UINT8(0)
  { load=fieldIntLoad, save=fieldIntSave, display=fieldIntDisplay }, --2 INT8(1)
  { load=fieldIntLoad, save=fieldIntSave, display=fieldIntDisplay }, --3 UINT16(2)
  { load=fieldIntLoad, save=fieldIntSave, display=fieldIntDisplay }, --4 INT16(3)
  nil,
  nil,
  nil,
  nil,
  { load=fieldFloatLoad, save=fieldIntSave, display=fieldFloatDisplay },  --9 FLOAT(8)
  { load=fieldTextSelLoad, save=fieldIntSave, display=nil }, --10 SELECT(9)
  { load=fieldStringLoad, save=nil, display=fieldStringDisplay }, --11 STRING(10) editing NOTIMPL
  { load=nil, save=fieldFolderOpen, display=fieldFolderDisplay }, --12 FOLDER(11)
  { load=fieldStringLoad, save=nil, display=fieldStringDisplay }, --13 INFO(12)
  { load=fieldCommandLoad, save=fieldCommandSave, display=fieldCommandDisplay }, --14 COMMAND(13)
  { load=nil, save=fieldBackExec, display=fieldCommandDisplay }, --15 back/exit(14)
  { load=nil, save=fieldDeviceIdSelect, display=fieldCommandDisplay }, --16 device(15)
  { load=nil, save=fieldFolderDeviceOpen, display=fieldFolderDisplay }, --17 deviceFOLDER(16)
}

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
    for i=5, #data do
      fieldData[#fieldData + 1] = data[i]
      data[i] = nil
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
      field.type = bit32.band(fieldData[offset+1], 0x7f)
      field.hidden = bit32.btest(fieldData[offset+1], 0x80) or nil
      field.name, offset = fieldGetStrOrOpts(fieldData, offset+2, field.name)
      if functions[field.type+1].load then
        functions[field.type+1].load(field, fieldData, offset)
      end
      if field.min == 0 then field.min = nil end
      if field.max == 0 then field.max = nil end
    end

    fieldChunk = 0
    fieldData = nil

    -- Last field loaded, add the list of devices to the end
    if #loadQ == 0 then
      createDeviceFields()
    end

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
  local goodPkt = (data[4]*256) + data[5]
  local newFlags = data[6]
  -- If flags are changing, reset the warning timeout to display/hide message immediately
  if newFlags ~= elrsFlags then
    elrsFlags = newFlags
    titleShowWarnTimeout = 0
  end
  elrsFlagsInfo = fieldGetStrOrOpts(data, 7)

  local state = (bit32.btest(elrsFlags, 1) and "C") or "-"
  goodBadPkt = string.format("%u/%u   %s", badPkt, goodPkt, state)
end

local function parseElrsV1Message(data)
  if (data[1] ~= 0xEA) or (data[2] ~= 0xEE) then
    return
  end

  -- local badPkt = data[9]
  -- local goodPkt = (data[10]*256) + data[11]
  -- goodBadPkt = string.format("%u/%u   X", badPkt, goodPkt)
  fieldPopup = {id = 0, status = 2, timeout = 0xFF, info = "ERROR: 1.x firmware"}
  fieldTimeout = getTime() + 0xFFFF
end

local function refreshNext()
  local command, data, forceRedraw
  repeat
    command, data = crossfireTelemetryPop()
    if command == 0x29 then
      parseDeviceInfoMessage(data)
    elseif command == 0x2B then
      if parseParameterInfoMessage(data) then
        forceRedraw = true
      end
      if #loadQ > 0 then
        fieldTimeout = 0 -- request next chunk immediately
      elseif fieldPopup then
        fieldTimeout = getTime() + fieldPopup.timeout
      end
    elseif command == 0x2D then
      parseElrsV1Message(data)
    elseif command == 0x2E then
      parseElrsInfoMessage(data)
      forceRedraw = true
    end
  until command == nil

  local time = getTime()
  if fieldPopup then
    if time > fieldTimeout and fieldPopup.status ~= 3 then
      crossfireTelemetryPush(0x2D, { deviceId, handsetId, fieldPopup.id, 6 }) -- lcsQuery
      fieldTimeout = time + fieldPopup.timeout
    end
  elseif time > devicesRefreshTimeout and fields_count < 1 then
    forceRedraw = true -- handles initial screen draw
    devicesRefreshTimeout = time + 100 -- 1s
    crossfireTelemetryPush(0x28, { 0x00, 0xEA })
  elseif time > linkstatTimeout then
    if not deviceIsELRS_TX and #loadQ == 0 then
      goodBadPkt = ""
    else
      crossfireTelemetryPush(0x2D, { deviceId, handsetId, 0x0, 0x0 }) --request linkstat
    end
    linkstatTimeout = time + 100
  elseif time > fieldTimeout and fields_count ~= 0 then
    if #loadQ > 0 then
      crossfireTelemetryPush(0x2C, { deviceId, handsetId, loadQ[#loadQ], fieldChunk })
      fieldTimeout = time + 50 -- 0.5s
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
  lcd.clear()

  local EBLUE = lcd.RGB(0x43, 0x61, 0xAA)
  local EGREEN = lcd.RGB(0x9f, 0xc7, 0x6f)
  local EGREY1 = lcd.RGB(0x91, 0xb2, 0xc9)
  local EGREY2 = lcd.RGB(0x6f, 0x62, 0x7f)
  local barHeight = 30

  -- Field display area (white w/ 2px green border)
  lcd.setColor(CUSTOM_COLOR, EGREEN)
  lcd.drawRectangle(0, 0, LCD_W, LCD_H, CUSTOM_COLOR)
  lcd.drawRectangle(1, 0, LCD_W - 2, LCD_H - 1, CUSTOM_COLOR)
  -- title bar
  lcd.drawFilledRectangle(0, 0, LCD_W, barHeight, CUSTOM_COLOR)
  lcd.setColor(CUSTOM_COLOR, EGREY1)
  lcd.drawFilledRectangle(LCD_W - textSize, 0, textSize, barHeight, CUSTOM_COLOR)
  lcd.setColor(CUSTOM_COLOR, EGREY2)
  lcd.drawRectangle(LCD_W - textSize, 0, textSize, barHeight - 1, CUSTOM_COLOR)
  lcd.drawRectangle(LCD_W - textSize, 1 , textSize - 1, barHeight - 2, CUSTOM_COLOR) -- left and bottom line only 1px, make it look bevelled
  lcd.setColor(CUSTOM_COLOR, BLACK)
  if titleShowWarn then
    lcd.drawText(COL1 + 1, 4, elrsFlagsInfo, CUSTOM_COLOR)
  else
    local title = fields_count > 0 and deviceName or "Loading..."
    lcd.drawText(COL1 + 1, 4, title, CUSTOM_COLOR)
    lcd.drawText(LCD_W - 5, 4, goodBadPkt, RIGHT + BOLD + CUSTOM_COLOR)
  end
  -- progress bar
  if #loadQ > 0 and fields_count > 0 then
    local barW = (COL2-4) * (fields_count - #loadQ) / fields_count
    lcd.setColor(CUSTOM_COLOR, EBLUE)
    lcd.drawFilledRectangle(2, 2+20, barW, barHeight-5-20, CUSTOM_COLOR)
    lcd.setColor(CUSTOM_COLOR, WHITE)
    lcd.drawFilledRectangle(2+barW, 2+20, COL2-2-barW, barHeight-5-20, CUSTOM_COLOR)
  end
end

local function lcd_title_bw()
  lcd.clear()
  -- B&W screen
  local barHeight = 9
  if not titleShowWarn then
    lcd.drawText(LCD_W - 1, 1, goodBadPkt, RIGHT)
    lcd.drawLine(LCD_W - 10, 0, LCD_W - 10, barHeight-1, SOLID, INVERS)
  end

  if #loadQ > 0 and fields_count > 0 then
    lcd.drawFilledRectangle(COL2, 0, LCD_W, barHeight, GREY_DEFAULT)
    lcd.drawGauge(0, 0, COL2, barHeight, fields_count - #loadQ, fields_count, 0)
  else
    lcd.drawFilledRectangle(0, 0, LCD_W, barHeight, GREY_DEFAULT)
    if titleShowWarn then
      lcd.drawText(COL1, 1, elrsFlagsInfo, INVERS)
    else
      local title = fields_count > 0 and deviceName or "Loading..."
      lcd.drawText(COL1, 1, title, INVERS)
    end
  end
end

local function lcd_warn()
  lcd.drawText(COL1, textSize*2, "Error:")
  lcd.drawText(COL1, textSize*3, elrsFlagsInfo)
  lcd.drawText(LCD_W/2, textSize*5, "[OK]", BLINK + INVERS + CENTER)
end

local function reloadCurField()
  local field = getField(lineIndex)
  fieldTimeout = 0
  fieldChunk = 0
  fieldData = nil
  loadQ[#loadQ+1] = field.id
end

local function reloadRelatedFields(field)
  -- Reload the parent folder to update the description
  if field.parent then
    loadQ[#loadQ+1] = field.parent
    fields[field.parent].name = nil
  end

  -- Reload all editable fields at the same level as well as the parent item
  for fieldId = fields_count, 1, -1 do
    -- Skip this field, will be added to end
    local fldTest = fields[fieldId]
    local fldType = fldTest.type or 99 -- type could be nil if still loading
    if fieldId ~= field.id
      and fldTest.parent == field.parent
      and (fldType < 11 or fldType == 12) then -- ignores FOLDER/COMMAND/devices/EXIT
      fldTest.nc = true -- "no cache" the options
      loadQ[#loadQ+1] = fieldId
    end
  end

  -- Reload this field
  loadQ[#loadQ+1] = field.id
  -- with a short delay to allow the module EEPROM to commit
  fieldTimeout = getTime() + 20
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
          changeDeviceId(0xEE) --change device id clear the fields_count, therefore the next ping will do reloadAllField()
        else
          reloadAllField()
        end
        crossfireTelemetryPush(0x28, { 0x00, 0xEA })
      else
        fieldBackExec(fields[#fields])
      end
    end
  elseif event == EVT_VIRTUAL_ENTER then -- toggle editing/selecting current field
    if elrsFlags > 0x1F then
      elrsFlags = 0
      crossfireTelemetryPush(0x2D, { deviceId, handsetId, 0x2E, 0x00 })
    else
      local field = getField(lineIndex)
      if field and field.name then
        if field.type < 10 then
          edit = not edit
        end
        if not edit then
          if field.type < 10 then
            -- Editable fields
            reloadRelatedFields(field)
          elseif field.type == 13 then
            -- Command
            reloadCurField()
          end
          if functions[field.type+1].save then
            functions[field.type+1].save(field)
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
    fields[fields_count+1].parent = nil
  end
  if elrsFlags > 0x1F then
    lcd_warn()
  else
    for y = 1, maxLineIndex+1 do
      local field = getField(pageOffset+y)
      if not field then
        break
      elseif field.name ~= nil then
        local attr = lineIndex == (pageOffset+y)
          and ((edit and BLINK or 0) + INVERS)
          or 0
        if field.type < 11 or field.type == 12 then -- if not folder, command, or back
          lcd.drawText(COL1, y*textSize+textYoffset, field.name, 0)
        end
        if functions[field.type+1].display then
          functions[field.type+1].display(field, y*textSize+textYoffset, attr)
        end
      end
    end
  end
end

local function popupCompat(t, m, e)
  -- Only use 2 of 3 arguments for older platforms
  return popupConfirmation(t, e)
end

local function runPopupPage(event)
  if event == EVT_VIRTUAL_EXIT then
    crossfireTelemetryPush(0x2D, { deviceId, handsetId, fieldPopup.id, 5 }) -- lcsCancel
    fieldTimeout = getTime() + 200 -- 2s
  end

  if fieldPopup.status == 0 and fieldPopup.lastStatus ~= 0 then -- stopped
      popupCompat(fieldPopup.info, "Stopped!", event)
      reloadAllField()
      fieldPopup = nil
  elseif fieldPopup.status == 3 then -- confirmation required
    local result = popupCompat(fieldPopup.info, "PRESS [OK] to confirm", event)
    fieldPopup.lastStatus = fieldPopup.status
    if result == "OK" then
      crossfireTelemetryPush(0x2D, { deviceId, handsetId, fieldPopup.id, 4 }) -- lcsConfirmed
      fieldTimeout = getTime() + fieldPopup.timeout -- we are expecting an immediate response
      fieldPopup.status = 4
    elseif result == "CANCEL" then
      fieldPopup = nil
    end
  elseif fieldPopup.status == 2 then -- running
    if fieldChunk == 0 then
      commandRunningIndicator = (commandRunningIndicator % 4) + 1
    end
    local result = popupCompat(fieldPopup.info .. " [" .. string.sub("|/-\\", commandRunningIndicator, commandRunningIndicator) .. "]", "Press [RTN] to exit", event)
    fieldPopup.lastStatus = fieldPopup.status
    if result == "CANCEL" then
      crossfireTelemetryPush(0x2D, { deviceId, handsetId, fieldPopup.id, 5 }) -- lcsCancel
      fieldTimeout = getTime() + fieldPopup.timeout -- we are expecting an immediate response
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
    or (touchState.swipeRight  and EVT_VIRTUAL_ENTER)
    or (touchState.swipeUp     and (edit and EVT_VIRTUAL_NEXT or EVT_VIRTUAL_PREV))
    or (touchState.swipeDown   and (edit and EVT_VIRTUAL_PREV or EVT_VIRTUAL_NEXT))
    or (event == EVT_TOUCH_TAP and EVT_VIRTUAL_ENTER)
end

local function setLCDvar()
  -- Set the title function depending on if LCD is color, and free the other function and
  -- set textselection unit function, use GetLastPost or sizeText
  if (lcd.RGB ~= nil) then
    lcd_title = lcd_title_color
    functions[10].display = fieldTextSelDisplay_color
  else
    lcd_title = lcd_title_bw
    functions[10].display = fieldTextSelDisplay_bw
    touch2evt = nil
  end
  lcd_title_color = nil
  lcd_title_bw = nil
  fieldTextSelDisplay_bw = nil
  fieldTextSelDisplay_color = nil
  -- Determine if popupConfirmation takes 3 arguments or 2
  -- if pcall(popupConfirmation, "", "", EVT_VIRTUAL_EXIT) then
  -- major 1 is assumed to be FreedomTX
  local _, _, major = getVersion()
  if major ~= 1 then
    popupCompat = popupConfirmation
  end
  if LCD_W == 480 then
    COL1 = 3
    COL2 = 240
    maxLineIndex = 10
    textYoffset = 10
    textSize = 22 --textSize is text Height
  elseif LCD_W == 320 then
    COL1 = 3
    COL2 = 160
    maxLineIndex = 14
    textYoffset = 10
    textSize = 22
  elseif LCD_W == 128 then
    COL1 = 0
    COL2 = 70
    maxLineIndex = 9
    textYoffset = 3
    textSize = 8
  else
    if LCD_W == 212 then
      COL2 = 110
    else
      COL2 = 70
    end
    COL1 = 0
    maxLineIndex = 6
    textYoffset = 3
    textSize = 8
  end
end

local function setMock()
  -- Setup fields to display if running in Simulator
  local _, rv = getVersion()
  if string.sub(rv, -5) ~= "-simu" then return end
  local mock = loadScript("mockup/elrsmock.lua")
  if mock == nil then return end
  fields, goodBadPkt, deviceName = mock()
  fields_count = #fields - 1
  loadQ = { fields_count }
  deviceIsELRS_TX = true
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
  if event == nil then
    error("Cannot be run as a model script!")
    return 2
  end

  local forceRedraw = refreshNext()

  event = (touch2evt and touch2evt(event, touchState)) or event
  if fieldPopup ~= nil then
    runPopupPage(event)
  elseif event ~= 0 or forceRedraw or edit then
    runDevicePage(event)
  end

  return exitscript
end

return { init=init, run=run }
