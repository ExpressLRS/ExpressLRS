-- TNS|ExpressLRS|TNE
---- #########################################################################
---- #                                                                       #
---- # Copyright (C) OpenTX                                                  #
-----#                                                                       #
---- # License GPLv2: http://www.gnu.org/licenses/gpl-2.0.html               #
---- #                                                                       #
---- # This program is free software; you can redistribute it and/or modify  #
---- # it under the terms of the GNU General Public License version 2 as     #
---- # published by the Free Software Foundation.                            #
---- #                                                                       #
---- # This program is distributed in the hope that it will be useful        #
---- # but WITHOUT ANY WARRANTY; without even the implied warranty of        #
---- # MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the         #
---- # GNU General Public License for more details.                          #
---- #                                                                       #
---- #########################################################################
local deviceId = 0xEE
local handsetId = 0xEF
local deviceName = ""
local lineIndex = 1
local pageOffset = 0
local edit = nil
local charIndex = 1
local fieldPopup
local fieldTimeout = 0
local fieldId = 1
local fieldChunk = 0
local fieldData = {}
local fields = {}
local devices = {}
local goodBadPkt = "?/???    ?"
local elrsFlags = 0
local elrsFlagsInfo = ""
local fields_count = 0
local backButtonId = 2
local devicesRefreshTimeout = 50
local allParamsLoaded = 0
local folderAccess = nil
local statusComplete = 0
local commandRunningIndicator = 1
local expectedChunks = -1
local deviceIsELRS_TX = nil
local linkstatTimeout = 100
local titleShowWarn = nil
local titleShowWarnTimeout = 100

local COL2
local maxLineIndex
local textXoffset
local textYoffset
local textSize

local function allocateFields()
  fields = {}
  for i=1, fields_count + 2 + #devices do
    fields[i] = { }
  end
  backButtonId = fields_count + 2 + #devices
  fields[backButtonId] = {id = backButtonId, name="----BACK----", parent = 255, type=14}
  if folderAccess ~= nil then
    fields[backButtonId].parent = folderAccess
  end
end

local function reloadAllField()
  allParamsLoaded = 0
  fieldId, fieldChunk = 1, 0
  fieldData = {}
end

local function getField(line)
  local counter = 1
  for i = 1, #fields do
    local field = fields[i]
    if folderAccess == field.parent and not field.hidden then
      if counter < line then
        counter = counter + 1
      else
        return field
      end
    end
  end
end

-- Change display attribute to current field
local function incrField(step)
  local field = getField(lineIndex)
  if field.type == 10 then
    local byte = 32
    if charIndex <= #field.value then
      byte = string.byte(field.value, charIndex) + step
    end
    if byte < 32 then
      byte = 32
    elseif byte > 122 then
      byte = 122
    end
    if charIndex <= #field.value then
      field.value = string.sub(field.value, 1, charIndex-1) .. string.char(byte) .. string.sub(field.value, charIndex+1)
    else
      field.value = field.value .. string.char(byte)
    end
  else
    local min, max = 0, 0
    if ((field.type <= 5) or (field.type == 8)) then
      min = field.min or 0
      max = field.max or 0
      step = field.step * step
    elseif field.type == 9 then
      min = 0
      max = #field.values - 1
    end
    if (step < 0 and field.value > min) or (step > 0 and field.value < max) then
      field.value = field.value + step
    end
  end
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

local function fieldStrFF(data, offset, last)
  while data[offset] ~= 0 do
    offset = offset + 1
  end
  return last, offset + 1
end

local function fieldGetSelectOpts(data, offset, last)
  if last then
    return fieldStrFF(data, offset, last)
  end

  -- Split a table of byte values (string) with ; separator into a table
  local r = {}
  local opt = ''
  local b = data[offset]
  while b ~= 0 do
    if b == 59 then -- ';'
      r[#r+1] = opt
      opt = ''
    else
      opt = opt .. string.char(b)
    end
    offset = offset + 1
    b = data[offset]
  end

  r[#r+1] = opt
  return r, offset + 1
end

local function fieldGetString(data, offset, last)
  if last then
    return fieldStrFF(data, offset, last)
  end

  local result = ""
  while data[offset] ~= 0 do
    result = result .. string.char(data[offset])
    offset = offset + 1
  end

  return result, offset + 1
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

local function fieldUnsignedLoad(field, data, offset, size)
  field.value = fieldGetValue(data, offset, size)
  field.min = fieldGetValue(data, offset+size, size)
  field.max = fieldGetValue(data, offset+2*size, size)
  --field.default = fieldGetValue(data, offset+3*size, size)
  field.unit = fieldGetString(data, offset+4*size, field.unit)
  field.step = 1
end

local function fieldUnsignedToSigned(field, size)
  local bandval = bit32.lshift(0x80, (size-1)*8)
  field.value = field.value - bit32.band(field.value, bandval) * 2
  field.min = field.min - bit32.band(field.min, bandval) * 2
  field.max = field.max - bit32.band(field.max, bandval) * 2
  --field.default = field.default - bit32.band(field.default, bandval) * 2
end

  local function fieldSignedLoad(field, data, offset, size)
  fieldUnsignedLoad(field, data, offset, size)
  fieldUnsignedToSigned(field, size)
end

local function fieldIntSave(index, value, size)
  local frame = { deviceId, handsetId, index }
  for i=size-1, 0, -1 do
    frame[#frame + 1] = (bit32.rshift(value, 8*i) % 256)
  end
  crossfireTelemetryPush(0x2D, frame)
end

local function fieldUnsignedSave(field, size)
  local value = field.value
  fieldIntSave(field.id, value, size)
end

local function fieldSignedSave(field, size)
  local value = field.value
  if value < 0 then
    value = bit32.lshift(0x100, (size-1)*8) + value
  end
  fieldIntSave(field.id, value, size)
end

local function fieldIntDisplay(field, y, attr)
  lcd.drawText(COL2, y, field.value .. field.unit, attr)
end

-- UINT8
local function fieldUint8Load(field, data, offset)
  fieldUnsignedLoad(field, data, offset, 1)
end

local function fieldUint8Save(field)
  fieldUnsignedSave(field, 1)
end

-- INT8
local function fieldInt8Load(field, data, offset)
  fieldSignedLoad(field, data, offset, 1)
end

local function fieldInt8Save(field)
  fieldSignedSave(field, 1)
end

-- UINT16
local function fieldUint16Load(field, data, offset)
  fieldUnsignedLoad(field, data, offset, 2)
end

local function fieldUint16Save(field)
  fieldUnsignedSave(field, 2)
end

-- INT16
local function fieldInt16Load(field, data, offset)
  fieldSignedLoad(field, data, offset, 2)
end

local function fieldInt16Save(field)
  fieldSignedSave(field, 2)
end

-- FLOAT
local function fieldFloatLoad(field, data, offset)
  field.value = fieldGetValue(data, offset, 4)
  field.min = fieldGetValue(data, offset+4, 4)
  field.max = fieldGetValue(data, offset+8, 4)
  --field.default = fieldGetValue(data, offset+12, 4)
  fieldUnsignedToSigned(field, 4)
  field.prec = data[offset+16]
  if field.prec > 3 then
    field.prec = 3
  end
  field.step = fieldGetValue(data, offset+17, 4)
  field.unit = fieldGetString(data, offset+21, field.unit)
end

local function formatFloat(num, decimals)
  local mult = 10^(decimals or 0)
  local val = num / mult
  return string.format("%." .. decimals .. "f", val)
end

local function fieldFloatDisplay(field, y, attr)
  lcd.drawText(COL2, y, formatFloat(field.value, field.prec) .. field.unit, attr)
end

local function fieldFloatSave(field)
  fieldUnsignedSave(field, 4)
end

-- TEXT SELECTION
local function fieldTextSelectionLoad(field, data, offset)
  field.values, offset = fieldGetSelectOpts(data, offset, field.values)
  field.value = data[offset]
  -- min max and default (offset+1 to 3) are not used on selections
  field.unit = fieldGetString(data, offset+4, field.unit)
end

local function fieldTextSelectionSave(field)
  crossfireTelemetryPush(0x2D, { deviceId, handsetId, field.id, field.value })
end

local function fieldTextSelectionDisplay(field, y, attr)
  lcd.drawText(COL2, y, (field.values[field.value+1] or "ERR") .. field.unit, attr)
end

-- STRING
local function fieldStringLoad(field, data, offset)
  field.value, offset = fieldGetString(data, offset)
  if #data >= offset then
    field.maxlen = data[offset]
  end
end

local function fieldStringSave(field)
  local frame = { deviceId, handsetId, field.id }
  for i=1, string.len(field.value) do
    frame[#frame + 1] = string.byte(field.value, i)
  end
  frame[#frame + 1] = 0
  crossfireTelemetryPush(0x2D, frame)
end

local function fieldStringDisplay(field, y, attr)
  if edit and attr then
    lcd.drawText(COL2, y, field.value, attr)
    lcd.drawText(COL2+6*(charIndex-1), y, string.sub(field.value, charIndex, charIndex), attr)
  else
    lcd.drawText(COL2, y, field.value, attr)
  end
end

local function fieldFolderOpen(field)
  lineIndex = 1
  pageOffset = 0
  folderAccess = field.id
  fields[backButtonId].parent = folderAccess
end

local function fieldFolderDeviceOpen(field)
  crossfireTelemetryPush(0x28, { 0x00, 0xEA }) --broadcast with standard handset ID to get all node respond correctly
  return fieldFolderOpen(field)
end

local function fieldFolderDisplay(field,y ,attr)
  lcd.drawText(textXoffset, y, "> " .. field.name, bit32.bor(attr, BOLD))
end

local function fieldCommandLoad(field, data, offset)
  field.status = data[offset]
  field.timeout = data[offset+1]
  field.info = fieldGetString(data, offset+2)
  if field.status == 0 then
    fieldPopup = nil
  end
end

local function fieldCommandSave(field)
  if field.status < 4 then
    field.status = 1
    crossfireTelemetryPush(0x2D, { deviceId, handsetId, field.id, field.status })
    fieldPopup = field
    fieldPopup.lastStatus = 0
    commandRunningIndicator = 1
    fieldTimeout = getTime() + field.timeout
  end
end

local function fieldCommandDisplay(field, y, attr)
    lcd.drawText(10, y, "[" .. field.name .. "]", bit32.bor(attr, BOLD))
end

local function UIbackExec()
  folderAccess = nil
  fields[backButtonId].parent = 255
end

local function changeDeviceId(devId) --change to selected device ID
  folderAccess = nil
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
  fields[fields_count + 2 + #devices] = fields[backButtonId]
  backButtonId = fields_count + 2 + #devices  -- move back button to the end of the list, so it will always show up at the bottom.
  for i=1, #devices do
    if devices[i].id == deviceId then
      fields[fields_count+1+i] = {id = fields_count+1+i, name=devices[i].name, parent = 255, type=15}
    else
      fields[fields_count+1+i] = {id = fields_count+1+i, name=devices[i].name, parent = fields_count+1, type=15}
    end
  end
end

local function parseDeviceInfoMessage(data)
  local offset
  local id = data[2]
  local devicesName
  devicesName, offset = fieldGetString(data, 3)
  local device = getDevice(devicesName)
  if device == nil then
    device = { id = id, name = devicesName }
    devices[#devices + 1] = device
  end
  if deviceId == id then
    deviceName = devicesName
    deviceIsELRS_TX = ((fieldGetValue(data,offset,4) == 0x454C5253) and (deviceId == 0xEE)) or nil -- SerialNumber = 'E L R S' and ID is TX module
    local newFieldCount = data[offset+12]
    reloadAllField()
    if newFieldCount ~= fields_count or newFieldCount == 0 then
      fields_count = newFieldCount
      allocateFields()
      fields[fields_count+1] = {id = fields_count+1, name="Other Devices", parent = 255, type=16} -- add other devices folders
      if newFieldCount == 0 then
        allParamsLoaded = 1
        fieldId = 1
        createDeviceFields()
      end
    end
  end
end

local functions = {
  { load=fieldUint8Load, save=fieldUint8Save, display=fieldIntDisplay }, --1 UINT8(0)
  { load=fieldInt8Load, save=fieldInt8Save, display=fieldIntDisplay }, --2 INT8(1)
  { load=fieldUint16Load, save=fieldUint16Save, display=fieldIntDisplay }, --3 UINT16(2)
  { load=fieldInt16Load, save=fieldInt16Save, display=fieldIntDisplay }, --4  INT16(3)
  nil,
  nil,
  nil,
  nil,
  { load=fieldFloatLoad, save=fieldFloatSave, display=fieldFloatDisplay }, --9 FLOAT(8)
  { load=fieldTextSelectionLoad, save=fieldTextSelectionSave, display=fieldTextSelectionDisplay }, --10 SELECT(9)
  { load=fieldStringLoad, save=fieldStringSave, display=fieldStringDisplay }, --11 STRING(10)
  { load=nil, save=fieldFolderOpen, display=fieldFolderDisplay }, --12 FOLDER(11)
  { load=fieldStringLoad, save=fieldStringSave, display=fieldStringDisplay }, --13 INFO(12)
  { load=fieldCommandLoad, save=fieldCommandSave, display=fieldCommandDisplay }, --14 COMMAND(13)
  { load=nil, save=UIbackExec, display=fieldCommandDisplay }, --15 back(14)
  { load=nil, save=fieldDeviceIdSelect, display=fieldCommandDisplay }, --16 device(15)
  { load=nil, save=fieldFolderDeviceOpen, display=fieldFolderDisplay }, --17 deviceFOLDER(16)
}

local function parseParameterInfoMessage(data)
  if data[2] ~= deviceId or data[3] ~= fieldId then
    fieldData = {}
    fieldChunk = 0
    return
  end
  if #fieldData == 0 then
    expectedChunks = -1
  end
  local field = fields[fieldId]
  local chunks = data[4]
  if not field or (chunks ~= expectedChunks and expectedChunks ~= -1) then
    return -- we will ignore this and subsequent chunks till we send a new command
  end
  expectedChunks = chunks - 1
  for i=5, #data do
    fieldData[#fieldData + 1] = data[i]
  end
  if chunks > 0 then
    fieldChunk = fieldChunk + 1
    statusComplete = 0
  else
    fieldChunk = 0
    if #fieldData < 4 then -- short packet, invalid
      fieldData = {}
      return -- no data extraction
    end

    field.id = fieldId
    local parent = (fieldData[1] ~= 0) and fieldData[1] or nil
    local type = fieldData[2] % 128
    local hidden = (bit32.rshift(fieldData[2], 7) == 1) or nil
    local offset
    if field.name ~= nil then -- already seen this field before, so we can validate this packet is correct
      if field.parent ~= parent or field.type ~= type or field.hidden ~= hidden then
        fieldData = {}
        return -- no data extraction
      end
    end
    field.parent = parent
    field.type = type
    field.hidden = hidden
    field.name, offset = fieldGetString(fieldData, 3, field.name)
    if functions[field.type+1].load then
      functions[field.type+1].load(field, fieldData, offset)
    end
    if field.min == 0 then field.min = nil end
    if field.max == 0 then field.max = nil end

    if not fieldPopup then
      if fieldId == fields_count then
        allParamsLoaded = 1
        fieldId = 1
        createDeviceFields()
      elseif allParamsLoaded == 0 then
        -- advance to the next field if doing a full load
        fieldId = 1 + (fieldId % (#fields-1))
      end
      fieldTimeout = getTime() + 200
    else
      fieldTimeout = getTime() + fieldPopup.timeout
    end
    statusComplete = 1
    fieldData = {}
  end
end

local function parseElrsInfoMessage(data)
  if data[2] ~= deviceId then
    fieldData = {}
    fieldChunk = 0
    return
  end

  local badPkt = data[3]
  local goodPkt = (data[4]*256) + data[5]
  elrsFlags = data[6]
  elrsFlagsInfo = fieldGetString(data, 7)

  local state = (bit32.btest(elrsFlags, 1) and "C") or "-"
  goodBadPkt = string.format("%u/%u   %s", badPkt, goodPkt, state)
end

local function refreshNext()
  local command, data = crossfireTelemetryPop()
  if command == 0x29 then
    parseDeviceInfoMessage(data)
  elseif command == 0x2B then
    parseParameterInfoMessage(data)
    if allParamsLoaded < 1 or statusComplete == 0 then
      fieldTimeout = 0 -- go fast until we have complete status record
    end
  elseif command == 0x2E then
    parseElrsInfoMessage(data)
  end

  local time = getTime()
  if fieldPopup then
    if time > fieldTimeout and fieldPopup.status ~= 3 then
      crossfireTelemetryPush(0x2D, { deviceId, handsetId, fieldPopup.id, 6 })
      fieldTimeout = time + fieldPopup.timeout
    end
  elseif time > devicesRefreshTimeout and fields_count < 1  then
    devicesRefreshTimeout = time + 100 -- 1s
    crossfireTelemetryPush(0x28, { 0x00, 0xEA })
  elseif time > fieldTimeout and fields_count ~= 0 and not edit then
    if allParamsLoaded < 1 or statusComplete == 0 then
      crossfireTelemetryPush(0x2C, { deviceId, handsetId, fieldId, fieldChunk })
      fieldTimeout = time + 50 -- 0.5s
    end
  elseif time > linkstatTimeout then
    if not deviceIsELRS_TX and allParamsLoaded == 1 then
      goodBadPkt = ""
      -- enable both line below to do what the legacy lua is doing which is reloading all params in an interval
      -- reloadAllField()
      -- linkstatTimeout = time + 300 --reload all param every 3s if not elrs
    else
      crossfireTelemetryPush(0x2D, { deviceId, handsetId, 0x0, 0x0 }) --request linkstat
    end
    linkstatTimeout = time + 100
  end

  if time > titleShowWarnTimeout then
    -- if elrsFlags bit set is bit higher than bit 0 and bit 1, it is warning flags
    titleShowWarn = (elrsFlags > 3 and not titleShowWarn) or nil
    titleShowWarnTimeout = time + 100
  end
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
    lcd.drawText(textXoffset + 1, 4, elrsFlagsInfo, CUSTOM_COLOR)
    lcd.drawText(LCD_W - textSize - 5, 4, tostring(elrsFlags), RIGHT + BOLD + CUSTOM_COLOR)
  else
    local title = allParamsLoaded == 1 and deviceName or "Loading..."
    lcd.drawText(textXoffset + 1, 4, title, CUSTOM_COLOR)
    lcd.drawText(LCD_W - 5, 4, goodBadPkt, RIGHT + BOLD + CUSTOM_COLOR)
  end
  -- progress bar
  if allParamsLoaded ~= 1 and fields_count > 0 then
    local barW = (COL2-4)*fieldId/fields_count
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
  if titleShowWarn then
    lcd.drawText(LCD_W, 1, tostring(elrsFlags), RIGHT)
  else
    lcd.drawText(LCD_W - 1, 1, goodBadPkt, RIGHT)
    lcd.drawLine(LCD_W - 10, 0, LCD_W - 10, barHeight-1, SOLID, INVERS)
  end

  if allParamsLoaded ~= 1 and fields_count > 0 then
    lcd.drawFilledRectangle(COL2, 0, LCD_W, barHeight, GREY_DEFAULT)
    lcd.drawGauge(0, 0, COL2, barHeight, fieldId, fields_count, 0)
  else
    lcd.drawFilledRectangle(0, 0, LCD_W, barHeight, GREY_DEFAULT)
    if titleShowWarn then
      lcd.drawText(textXoffset, 1, elrsFlagsInfo, INVERS)
    else
      local title = allParamsLoaded == 1 and deviceName or "Loading..."
      lcd.drawText(textXoffset, 1, title, INVERS)
    end
  end
end

local function lcd_warn()
  lcd.drawText(textSize*3, textSize*2, tostring(elrsFlags).." : "..elrsFlagsInfo, 0)
  lcd.drawText(textSize*10, textSize*6, "ok", BLINK + INVERS)
end

local function handleDevicePageEvent(event)
  if #fields == 0 then --if there is no field yet
    return
  else
    if fields[backButtonId].name == nil then --if back button is not assigned yet, means there is no field yet.
      return
    end
  end

  if event == EVT_VIRTUAL_EXIT then             -- exit script
    if edit then -- reload the field
      edit = nil
      local field = getField(lineIndex)
      fieldTimeout = getTime() + 200 -- 2s
      fieldId, fieldChunk = field.id, 0
      fieldData = {}
      crossfireTelemetryPush(0x2C, { deviceId, handsetId, fieldId, fieldChunk })
    else
      if folderAccess == nil and allParamsLoaded == 1 then -- only do reload if we're in the root folder and finished loading.
        if deviceId ~= 0xEE then
          changeDeviceId(0xEE) --change device id clear the fields_count, therefore the next ping will do reloadAllField()
        else
          reloadAllField()
        end
        crossfireTelemetryPush(0x28, { 0x00, 0xEA })
      end
      UIbackExec()
    end
  elseif event == EVT_VIRTUAL_ENTER then        -- toggle editing/selecting current field
    if elrsFlags > 0x1F then
      elrsFlags = 0
      crossfireTelemetryPush(0x2D, { deviceId, handsetId, 0x2E, 0x00 })
    else
      local field = getField(lineIndex)
      if field and field.name then
        if field.type == 10 then
          if not edit then
            edit = true
            charIndex = 1
          else
            charIndex = charIndex + 1
          end
        elseif field.type < 11 then
          edit = not edit
        end
        if not edit then
          if field.type < 11 or field.type == 13 then
            -- For editable field types and commands, request this field's
            -- data again, with a short delay to allow the module EEPROM to
            -- commit. Do this before save() to allow save to override
            fieldTimeout = getTime() + 20
            fieldId, fieldChunk, statusComplete = field.id, 0, 0
            fieldData = {}
          end
          functions[field.type+1].save(field)
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
          lcd.drawText(textXoffset, y*textSize+textYoffset, field.name, 0)
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
  if event == EVT_VIRTUAL_EXIT then             -- exit script
    crossfireTelemetryPush(0x2D, { deviceId, handsetId, fieldPopup.id, 5 })
    fieldTimeout = getTime() + 200 -- 2s
  end

  local result
  if fieldPopup.status == 0 and fieldPopup.lastStatus ~= 0 then -- stopped
      popupCompat(fieldPopup.info, "Stopped!", event)
      reloadAllField()
      fieldPopup = nil
  elseif fieldPopup.status == 3 then -- confirmation required
    result = popupCompat(fieldPopup.info, "PRESS [OK] to confirm", event)
    fieldPopup.lastStatus = fieldPopup.status
    if result == "OK" then
      crossfireTelemetryPush(0x2D, { deviceId, handsetId, fieldPopup.id, 4 })
      fieldTimeout = getTime() + fieldPopup.timeout -- we are expecting an immediate response
      fieldPopup.status = 4
    elseif result == "CANCEL" then
      fieldPopup = nil
    end
  elseif fieldPopup.status == 2 then -- running
    if statusComplete then
      commandRunningIndicator = (commandRunningIndicator % 4) + 1
    end
    result = popupCompat(fieldPopup.info .. " [" .. string.sub("|/-\\", commandRunningIndicator, commandRunningIndicator) .. "]", "Press [RTN] to exit", event)
    fieldPopup.lastStatus = fieldPopup.status
    if result == "CANCEL" then
      crossfireTelemetryPush(0x2D, { deviceId, handsetId, fieldPopup.id, 5 })
      fieldTimeout = getTime() + fieldPopup.timeout -- we are expecting an immediate response
      fieldPopup = nil
    end
  end
end

local function setLCDvar()
  -- Set the title function depending on if LCD is color, and free the other function
  lcd_title = (lcd.RGB ~= nil) and lcd_title_color or lcd_title_bw
  lcd_title_color = nil
  lcd_title_bw = nil
  -- Determine if popupConfirmation takes 3 arguments or 2
  --if pcall(popupConfirmation, "", "", EVT_VIRTUAL_EXIT) then
  -- major 1 is assumed to be FreedomTX
  local ver, radio, major = getVersion()
  if major ~= 1 then
    popupCompat = popupConfirmation
  end
  if LCD_W == 480 then
    COL2 = 240
    maxLineIndex = 10
    textXoffset = 3
    textYoffset = 10
    textSize = 22 --textSize is text Height
  elseif LCD_W == 320 then
    COL2 = 160
    maxLineIndex = 14
    textXoffset = 3
    textYoffset = 10
    textSize = 22
  else
    if LCD_W == 212 then
      COL2 = 110
    else
      COL2 = 70
    end
    maxLineIndex = 6
    textXoffset = 0
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
  fields, goodBadPkt, deviceName = mock(), "0/500   C", "ExpressLRS TX"
  fields_count = #fields - 1
  fieldId = #fields
  deviceIsELRS_TX = true
  allParamsLoaded = 1
  backButtonId = #fields
end

-- Init
local function init()
  setLCDvar()
  setMock()
  setLCDvar = nil
  setMock = nil
end

-- Main
local function run(event)
  if event == nil then
    error("Cannot be run as a model script!")
    return 2
  end

  if fieldPopup ~= nil then
    runPopupPage(event)
  else
    runDevicePage(event)
  end

  refreshNext()

  return 0
end

return { init=init, run=run }
