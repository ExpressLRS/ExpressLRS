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
local edit = false
local charIndex = 1
local fieldPopup
local fieldTimeout = 0
local fieldId = 1
local fieldChunk = 0
local fieldData = {}
local fields = {}
local badPkt = 0
local goodPkt = 0
local elrsFlags = 0
local elrsFlagsInfo = "no"
local fields_count = 0
local backButtonId = 2
local devicesRefreshTimeout = 100
local allParamsLoaded = 0
local folderAccess = 0
local statusComplete = 0
local commandRunningIndicator = 1
local expectedChunks = -1

local COL2 = 70
local maxLineIndex = 7
local textXoffset = 0
local textYoffset = 1
local textSize = 8
local lcdIsColor

local devices = { }

local function clearAllField()
  for i=1, fields_count + 2 + #devices do
    fields[i] = { }
  end
  backButtonId = fields_count + 2 + #devices
  fields[backButtonId] = {id = backButtonId, name="----BACK----", parent = 255, type=14}
  if folderAccess ~= 0 then
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
    if not field.hidden then
      -- parent will be nil if it is in the list but the details are not loaded yet
      if field.parent == nil or folderAccess == field.parent then
        if counter < line then
          counter = counter + 1
        else
          return field
        end
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
      min = field.min
      max = field.max
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
  if lineIndex > maxLineIndex + pageOffset then 	-- NOTE: increased from 7 to 11 to allow 11 lines in Horus display
    pageOffset = lineIndex - maxLineIndex 		-- NOTE: increased from 7 to 11 to allow 11 lines in Horus display
  elseif lineIndex <= pageOffset then
    pageOffset = lineIndex - 1
  end
end

local function split(str)
  local t = {}
  local i = 1
  for s in string.gmatch(str, "([^;]+)") do
    t[i] = s
    i = i + 1
  end
  return t
end

local function fieldGetString(data, offset)
  local result = ""
  while data[offset] ~= 0 do
    result = result .. string.char(data[offset])
    offset = offset + 1
  end
  offset = offset + 1
  return result, offset
end

local function getBitBin(data, bitPosition)
  if data ~= nil then
    return bit32.band(bit32.rshift(data,bitPosition),1)
  end
    return 0
  end

  local function createDevice(devId, devName)
    local device = {
      id = devId,
      name = devName,
      timeout = 0
    }
    return device
  end
  
  local function getDevice(name)
    for i=1, #devices do
      if devices[i].name == name then
        return devices[i]
      end
    end
    return nil
  end
  
local function parseDeviceInfoMessage(data)
  local offset
  local id = data[2]
  local devicesName = ""
  -- deviceId = data[2]
  devicesName, offset = fieldGetString(data, 3)
  local device = getDevice(devicesName)
  if device == nil then
    device = createDevice(id, devicesName)
    devices[#devices + 1] = device
  end
  if deviceId == id then
    deviceName = devicesName
    fields_count = data[offset+12]
    reloadAllField()
    clearAllField()
    fields[fields_count+1] = {id = fields_count+1, name="Other Devices", parent = 255, type=16} -- add other devices folders
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
  field.default = fieldGetValue(data, offset+3*size, size)
  field.unit, offset = fieldGetString(data, offset+4*size)
  field.step = 1
end

local function fieldUnsignedToSigned(field, size)
  local bandval = bit32.lshift(0x80, (size-1)*8)
  field.value = field.value - bit32.band(field.value, bandval) * 2
  field.min = field.min - bit32.band(field.min, bandval) * 2
  field.max = field.max - bit32.band(field.max, bandval) * 2
  field.default = field.default - bit32.band(field.default, bandval) * 2
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
  -- lcd.drawNumber(COL2, y, field.value, LEFT + attr)    -- NOTE: original code getLastPos not available in Horus
  -- lcd.drawText(lcd.getLastPos(), y, field.unit, attr) -- NOTE: original code getLastPos not available in Horus
  lcd.drawText(COL2, y, field.value .. field.unit, attr)  -- NOTE: Concenated fields instead of get lastPos
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
  field.default = fieldGetValue(data, offset+12, 4)
  fieldUnsignedToSigned(field, 4)
  field.prec = data[offset+16]
  if field.prec > 3 then
    field.prec = 3
  end
  field.step = fieldGetValue(data, offset+17, 4)
  field.unit, offset = fieldGetString(data, offset+21)
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
  local values
  values, offset = fieldGetString(data, offset)
  if values ~= "" then
    field.values = split(values)
  end
  field.value = data[offset]
  field.min = data[offset+1]
  field.max = data[offset+2]
  field.default = data[offset+3]
  field.unit, offset = fieldGetString(data, offset+4)
end

local function fieldTextSelectionSave(field)
  crossfireTelemetryPush(0x2D, { deviceId, handsetId, field.id, field.value })
end

local function fieldTextSelectionDisplay(field, y, attr)
  -- lcd.drawText(COL2, y, field.values[field.value+1], attr)			-- NOTE: original code getLastPos not available in Horus
  -- lcd.drawText(lcd.getLastPos(), y, field.unit, attr) 				-- NOTE: original code getLastPos not available in Horus
  lcd.drawText(COL2, y, field.values[field.value+1] .. field.unit, attr) -- NOTE: Concenated fields instead of get lastPos
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
  if edit == true and attr then
    -- lcd.drawText(COL2, y, field.value, FIXEDWIDTH)	-- NOTE: FIXEDWIDTH unknown....
    -- lcd.drawText(134+6*charIndex, y, string.sub(field.value, charIndex, charIndex), FIXEDWIDTH + attr)	-- NOTE: FIXEDWIDTH unknown....
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
  lineIndex = 1
  pageOffset = 0
  folderAccess = field.id
  fields[backButtonId].parent = folderAccess
end

local function fieldFolderDisplay(field,y ,attr)
  lcd.drawText(textXoffset, y, "> " .. field.name, bit32.bor(attr, BOLD))
end

local function fieldCommandLoad(field, data, offset)
  field.status = data[offset]
  field.timeout = data[offset+1]
  field.info, offset = fieldGetString(data, offset+2)
  if field.status == 0 then
    field.previousInfo = field.info
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

local function UIbackExec(field)
  folderAccess = 0
  fields[backButtonId].parent = 255
end

local function changeDeviceId(devId)
  folderAccess = 0
  clearAllField()
  if devId == 0xEE then
    handsetId = 0xEF
  else
    handsetId = 0xEA
  end
  deviceId = devId
  fields_count = 0
end

local function fieldDeviceIdSelect(field)
  local device = getDevice(field.name)
  changeDeviceId(device.id)
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

local function createDeviceField() -- put other device in the field list
  fields[fields_count+2+#devices] = fields[backButtonId]
  backButtonId = fields_count+2+#devices
  for i=1, #devices do
    if devices[i].id ~= deviceId then
      fields[fields_count+1+i] = {id = fields_count+1+i, name=devices[i].name, parent = fields_count+1, type=15}
    end
  end
end

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
  if chunks ~= expectedChunks and expectedChunks ~= -1 then
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
    local parent = fieldData[1]
    local type = fieldData[2] % 128
    local hidden = (bit32.rshift(fieldData[2], 7) == 1)
    if field.name ~= nil then -- already seen this field before, so we can validate this packet is correct
      if field.parent ~= parent or field.type ~= type or field.hidden ~= hidden then
        fieldData = {}
        return -- no data extraction
      end
    end
    field.parent = parent
    field.type = type
    field.hidden = hidden
    local name, i = fieldGetString(fieldData, 3)
    if name ~= "" then
      local indent = ""
      while parent ~= 0 do
        indent = indent .. " "
        parent = fields[parent].parent
      end
      field.name = indent .. name
    end
    if functions[field.type+1].load then
      functions[field.type+1].load(field, fieldData, i)
    end
    if not fieldPopup then
      if fieldId == fields_count then
        allParamsLoaded = 1
        fieldId = 1
        createDeviceField()
      else
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
  badPkt = data[3]
  goodPkt = (data[4]*256) + data[5]
  elrsFlags = data[6]
  elrsFlagsInfo,offset = fieldGetString(data,7)
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
  elseif time > fieldTimeout and not edit then
    if allParamsLoaded < 1 or statusComplete == 0 then
      crossfireTelemetryPush(0x2C, { deviceId, handsetId, fieldId, fieldChunk })
      fieldTimeout = time + 300 -- 3s
    end
  end
end

local function lcd_title()
  local title = (allParamsLoaded == 1 or elrsFlags > 0) and deviceName or "Loading..."
  if lcdIsColor then
    -- Color screen
    local EBLUE = lcd.RGB(0x43, 0x61, 0xAA)
    local EGREEN = lcd.RGB(0x9f, 0xc7, 0x6f)
    local barHeight = 30

    lcd.clear()
    -- Field display area (white w/ 2px green border)
    lcd.setColor(CUSTOM_COLOR, EGREEN)
    lcd.drawRectangle(0, 0, LCD_W, LCD_H, CUSTOM_COLOR)
    lcd.drawRectangle(1, 0, LCD_W - 2, LCD_H - 1, CUSTOM_COLOR)
    -- title bar
    lcd.drawFilledRectangle(0, 0, LCD_W, barHeight, CUSTOM_COLOR)
    lcd.setColor(CUSTOM_COLOR, BLACK)
    lcd.drawText(textXoffset+1, 4, title, CUSTOM_COLOR)
    lcd.drawText(LCD_W-3, 4, tostring(badPkt) .. "/" .. tostring(goodPkt), RIGHT + BOLD + CUSTOM_COLOR)
    -- progress bar
    if allParamsLoaded ~= 1 and fields_count > 0 then
      local barW = (COL2-4)*fieldId/fields_count
      lcd.setColor(CUSTOM_COLOR, EBLUE)
      lcd.drawFilledRectangle(2, 2+20, barW, barHeight-5-20, CUSTOM_COLOR)
      lcd.setColor(CUSTOM_COLOR, WHITE)
      lcd.drawFilledRectangle(2+barW, 2+20, COL2-2-barW, barHeight-5-20, CUSTOM_COLOR)
    end
  else
    -- B&W screen
    local barHeight = 9

    lcd.clear()
    lcd.drawText(LCD_W, 1, tostring(badPkt) .. "/" .. tostring(goodPkt), RIGHT)
    -- keep the title this way to keep the script from error when module is not set correctly
    if allParamsLoaded ~= 1 and fields_count > 0 then
      lcd.drawFilledRectangle(COL2, 0, LCD_W, barHeight, GREY_DEFAULT)
      lcd.drawGauge(0, 0, COL2, barHeight, fieldId, fields_count, 0)
    else
      lcd.drawFilledRectangle(0, 0, LCD_W, barHeight, GREY_DEFAULT)
      lcd.drawText(textXoffset, 1, title, INVERS)
    end
  end
end
  

local function lcd_warn()
  lcd.drawText(textSize*3,textSize*2,tostring(elrsFlags).." : "..elrsFlagsInfo,0)
  lcd.drawText(textSize*10,textSize*6,"ok",BLINK + INVERS)
end

local function handleDevicePageEvent(event)
  if fields_count == 0 then return end

  if event == EVT_VIRTUAL_EXIT then             -- exit script
    if edit == true then -- reload the field
      edit = false
      local field = getField(lineIndex)
      fieldTimeout = getTime() + 200 -- 2s
      fieldId, fieldChunk = field.id, 0
      fieldData = {}
      crossfireTelemetryPush(0x2C, { deviceId, handsetId, fieldId, fieldChunk })
    else
      if folderAccess == 0 and allParamsLoaded == 1 then -- only do reload if we're in the root folder
        if deviceId ~= 0xEE then
          changeDeviceId(0xEE) --change device id clear the fields_count, therefore the next ping will do reloadAllField()
        else
          reloadAllField()
        end
      end
      folderAccess = 0
      fields[backButtonId].parent = 255
    end
  elseif event == EVT_VIRTUAL_ENTER then        -- toggle editing/selecting current field
    if elrsFlags > 0 then
      elrsFlags = 0
      crossfireTelemetryPush(0x2D, { deviceId, handsetId, 0x2E, 0x00 })
    else
      local field = getField(lineIndex)
      if field and field.name then
        if field.type == 10 then
          if edit == false then
            edit = true
            charIndex = 1
          else
            charIndex = charIndex + 1
          end
        elseif field.type < 11 then
          edit = not edit
        end
        if edit == false then
          fieldTimeout = getTime() + 200 -- 2s
          fieldId, fieldChunk = field.id, 0
          fieldData = {}
          functions[field.type+1].save(field)
          if field.type < 11 then
            -- we need a short time because if the packet rate changes we need time for the module to react
            fieldTimeout = getTime() + 20
            reloadAllField()
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
    fields[fields_count+1].parent = 0
  end

  if elrsFlags > 0 then
    lcd_warn()
  else
    for y = 1, maxLineIndex+1 do
      local field = getField(pageOffset+y)
      if not field then
        break
      elseif field.name ~= nil then
        local attr = lineIndex == (pageOffset+y)
          and ((edit == true and BLINK or 0) + INVERS)
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
  return 0
end

local function runPopupPage(event)
  if event == EVT_VIRTUAL_EXIT then             -- exit script
    crossfireTelemetryPush(0x2D, { deviceId, handsetId, fieldPopup.id, 5 })
    fieldTimeout = getTime() + 200 -- 2s
  end

  local result
  if fieldPopup.status == 0 and fieldPopup.lastStatus ~= 0 then -- stopped
      result = popupConfirmation(fieldPopup.info, "Stopped!", event)
      fieldPopup.lastStatus = status
      reloadAllField()
      fieldPopup = nil
  elseif fieldPopup.status == 3 then -- confirmation required
    result = popupConfirmation(fieldPopup.info, "PRESS [OK] to confirm", event)
    fieldPopup.lastStatus = status
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
    result = popupConfirmation(fieldPopup.info .. " [" .. string.sub("|/-\\", commandRunningIndicator, commandRunningIndicator) .. "]", "Press [RTN] to exit", event)
    fieldPopup.lastStatus = status
    if result == "CANCEL" then
      crossfireTelemetryPush(0x2D, { deviceId, handsetId, fieldPopup.id, 5 })
      fieldTimeout = getTime() + fieldPopup.timeout -- we are expecting an immediate response
      fieldPopup = nil
    end
  end
  return 0
end
  
local function setLCDvar()
  lcdIsColor = lcd.RGB ~= nil
  if LCD_W == 480 then
    COL2 = 240
    maxLineIndex = 10
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
  fields, goodPkt = mock(), 500
  fields_count = #fields - 1
  fieldId = #fields - 3
end

-- Init
local function init()
  setLCDvar()
  setMock()
end

-- Main
local function run(event)
  if event == nil then
    error("Cannot be run as a model script!")
    return 2
  end

  local result
  if fieldPopup ~= nil then
    result = runPopupPage(event)
  else
    result = runDevicePage(event)
  end

  refreshNext()

  return result
end

return { init=init, run=run }
