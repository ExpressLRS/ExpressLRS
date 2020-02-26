--[[
        ChgId

      DanielGeA

  License https://www.gnu.org/licenses/gpl-3.0.en.html

  Ported from erskyTx. Thanks to MikeB

  Lua script for radios X7, X9, X-lite and Horus with openTx 2.2 or higher

  Change Frsky sensor Id

]] --
local version = 'v0.1'
local refresh = 0
local lcdChange = true
local updateValues = false
local readIdState = 0
local sendIdState = 0
local timestamp = 0
local sensorIdTx = 17 -- sensorid 18

local binding = false

local AirRate = {
    selected = 5,
    list = {'AUTO', '200 Hz', '100 Hz', '50 Hz', '------'},
    dataId = {0x10, 0x00, 0x01, 0x02, 0xFF},
    elements = 5
}
local TLMinterval = {
    selected = 9,
    list = {
        'Off', '1:128', '1:64', '1:32', '1:16', '1:8', '1:4', '1:2', '------'
    },
    dataId = {0x00, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 0x08, 0xFF},
    elements = 9
}
local MaxPower = {
    selected = 7,
    list = {
        '500 mW', '250 mW', '100 mW', '50 mW', '25 mW', 'Pit mode', '------'
    },
    dataId = {0x00, 0x01, 0x02, 0x03, 0x04, 0x05, 0xFF},
    elements = 7
}
local RFfreq = {
    selected = 4,
    list = {'915 MHz', '868 MHz', '433 MHz', '------'},
    dataId = {0x00, 0x01, 0x02, 0xFF},
    elements = 4
}

local selection = {
    selected = 1,
    state = false,
    list = {'AirRate', 'TLMinterval', 'MaxPower', 'RFfreq'},
    elements = 4
}

local function getFlags(element)
    if selection.selected ~= element then return 0 end
    if selection.selected == element and selection.state == false then
        return 0 + INVERS
    end
    if selection.selected == element and selection.state == true then
        return 0 + INVERS + BLINK
    end
    return
end

local function increase(data)
    if data.selected < data.elements then
        data.selected = data.selected + 1
        playTone(2000, 50, 0)
    end
    -- if data.selected > data.elements then data.selected = 1 end
end

local function decrease(data)
    if data.selected > 1 then
        data.selected = data.selected - 1
        playTone(2000, 50, 0)
    end
    -- if data.selected < 1 then data.selected = data.elements end
end

local function readId()
    -- stop sensors
    if readIdState >= 1 and readIdState <= 15 and getTime() - timestamp > 11 then
        sportTelemetryPush(sensorIdTx, 0x21, 0xFFFF, 0x80)
        timestamp = getTime()
        readIdState = readIdState + 1
    end
    -- request/read id
    if readIdState >= 16 and readIdState <= 30 then
        if getTime() - timestamp > 11 then
            sportTelemetryPush(sensorIdTx, 0x30,
                               sensor.sensorType.dataId[sensor.sensorType
                                   .selected], 0x01)
            timestamp = getTime()
            readIdState = readIdState + 1
        else
            local physicalId, primId, dataId, value = sportTelemetryPop() -- frsky/lua: phys_id/sensor id, type/frame_id, sensor_id/data_id
            if primId == 0x32 and dataId ==
                sensor.sensorType.dataId[sensor.sensorType.selected] then
                if bit32.band(value, 0xFF) == 1 then
                    sensor.sensorId.selected = ((value - 1) / 256) + 1
                    readIdState = 0
                    lcdChange = true
                end
            end
        end
    end
    if readIdState == 31 then
        readIdState = 0
        lcdChange = true
    end
end

local function sendId()
    -- stop sensors
    if sendIdState >= 1 and sendIdState <= 15 and getTime() - timestamp > 11 then
        sportTelemetryPush(sensorIdTx, 0x21, 0xFFFF, 0x80)
        timestamp = getTime()
        sendIdState = sendIdState + 1
    end
    -- send id
    if sendIdState >= 16 and sendIdState <= 30 and getTime() - timestamp > 11 then
        sportTelemetryPush(sensorIdTx, 0x31,
                           sensor.sensorType.dataId[sensor.sensorType.selected],
                           0x01 + (sensor.sensorId.selected - 1) * 256)
        timestamp = getTime()
        sendIdState = sendIdState + 1
    end
    -- restart sensors
    if sendIdState >= 31 and sendIdState <= 45 and getTime() - timestamp > 11 then
        sportTelemetryPush(sensorIdTx, 0x20, 0xFFFF, 0x80)
        timestamp = getTime()
        sendIdState = sendIdState + 1
    end
    if sendIdState == 46 then
        sendIdState = 0
        lcdChange = true
    end
end

local function processResp()
    local command, data = crossfireTelemetryPop()

    if (data ~= nil) then

        if (command == 0x2D) and (data[1] == 0xEA) and (data[2] == 0xEE) then
			AirRate.selected = data[3]
			TLMinterval.selected = data[4]
			MaxPower.selected = data[5]
			RFfreq.selected = data[6]
		end


    end
end

local function refreshHorus()
    lcd.clear()
    lcd.drawText(1, 1, 'ExpressLRS CFG v.01', INVERS)
    lcd.drawText(1, 25, 'Pkt. Rate', 0)
    lcd.drawText(1, 45, 'TLM Ratio', 0)
    lcd.drawText(1, 65, 'Set Power', 0)
    lcd.drawText(1, 85, 'RF Freq', 0)

    lcd.drawText(100, 25, AirRate.list[AirRate.selected], getFlags(1))
    lcd.drawText(100, 45, TLMinterval.list[TLMinterval.selected], getFlags(2))
    lcd.drawText(100, 65, MaxPower.list[MaxPower.selected], getFlags(3))
    lcd.drawText(100, 85, RFfreq.list[RFfreq.selected], getFlags(4))

    lcd.drawText(20, 110, '[Bind]', getFlags(5) + SMLSIZE)
    lcd.drawText(60, 110, '[Web Server]', getFlags(6) + SMLSIZE)

    if selection.selected > 4 then
        if selection.state == true then
            lcd.drawText(30, 110, 'Press [ENTER] to stop', MEDSIZE)
        end
    end

    lcdChange = false

end

local function refreshTaranis()
    lcd.clear()
    lcd.drawScreenTitle('ExpressLRS CFG ' .. version, 1, 1)
    lcd.drawText(1, 11, 'Pkt. Rate', 0)
    lcd.drawText(1, 21, 'TLM Ratio', 0)
    lcd.drawText(1, 31, 'Set Power', 0)
    lcd.drawText(1, 41, 'RF Freq', 0)

    lcd.drawText(60, 11, AirRate.list[AirRate.selected], getFlags(1))
    lcd.drawText(60, 21, TLMinterval.list[TLMinterval.selected], getFlags(2))
    lcd.drawText(60, 31, MaxPower.list[MaxPower.selected], getFlags(3))
    lcd.drawText(60, 41, RFfreq.list[RFfreq.selected], getFlags(4))

    lcd.drawText(18, 54, '[Bind]', getFlags(5) + SMLSIZE)
    lcd.drawText(55, 54, '[Web Server]', getFlags(6) + SMLSIZE)

    if selection.selected > 4 then
        if selection.state == true then
            lcd.drawText(7, 53, 'Press [ENTER] to stop', MEDSIZE)
        end
    end

    lcdChange = false

end

local function init_func()
    crossfireTelemetryPush(0x2D, {0xEE, 0xEA, 0x00, 0x00})
    processResp()
	
	        if LCD_W == 480 then
            refreshHorus()
        else
            refreshTaranis()
        end
end

local function bg_func(event)
    if refresh < 25 then refresh = refresh + 1 end
end



local function run_func(event)

if updateValues == true or refresh == 25 then
init_func()
updateValues = false
end
    -- print(event)
    if refresh == 25 or lcdChange == true or selection.state == true then
        if LCD_W == 480 then
            refreshHorus()
        else
            refreshTaranis()
        end
		refresh = 0
    end

    -- left = up/decrease right = down/increase
    if selection.state == false then
        if event == EVT_ROT_LEFT or event == EVT_MINUS_BREAK or event ==
            EVT_DOWN_BREAK then
            decrease(selection)
            lcdChange = true
        end
        if event == EVT_ROT_RIGHT or event == EVT_PLUS_BREAK or event ==
            EVT_UP_BREAK then
            increase(selection)
            lcdChange = true
        end
    end
    if selection.state == true then
        if event == EVT_ROT_LEFT or event == EVT_MINUS_BREAK or event ==
            EVT_DOWN_BREAK then

            if selection.selected == 1 then
                -- increase(AirRate)
                crossfireTelemetryPush(0x2D, {0xEE, 0xEA, 0x01, 0x00})
            end

            if selection.selected == 2 then
                -- decrease(TLMinterval)
                crossfireTelemetryPush(0x2D, {0xEE, 0xEA, 0x02, 0x00})
            end

            if selection.selected == 3 then
                -- increase(MaxPower)
                crossfireTelemetryPush(0x2D, {0xEE, 0xEA, 0x03, 0x00})
            end

            if selection.selected == 4 then
                -- increase(RFfreq)
                crossfireTelemetryPush(0x2D, {0xEE, 0xEA, 0x04, 0x00})
            end

            lcdChange = true
			updateValues = true

        end
        if event == EVT_ROT_RIGHT or event == EVT_PLUS_BREAK or event ==
            EVT_UP_BREAK then

            if selection.selected == 1 then
                crossfireTelemetryPush(0x2D, {0xEE, 0xEA, 0x01, 0x01})
            end

            if selection.selected == 2 then
                crossfireTelemetryPush(0x2D, {0xEE, 0xEA, 0x02, 0x01})
            end

            if selection.selected == 3 then
                crossfireTelemetryPush(0x2D, {0xEE, 0xEA, 0x03, 0x01})
            end

            if selection.selected == 4 then
                crossfireTelemetryPush(0x2D, {0xEE, 0xEA, 0x04, 0x01})
            end

            lcdChange = true
			updateValues = true

        end
        processResp()
    end

    if event == EVT_ENTER_BREAK and sendIdState == 0 then
        if selection.selected < 5 then
            selection.state = not selection.state
            -- if selection.selected == 1 and sensor.sensorId.selected == 29 and sensor.sensorType.selected ~= 11 and selection.state == false then
            -- readIdState = 1
            -- end
            lcdChange = true
        end
    end
    if event == EVT_EXIT_BREAK then
        if selection.selected == 1 and sensor.sensorId.selected == 29 and
            sensor.sensorType.selected ~= 11 and selection.state == true then
            readIdState = 1
        end
        selection.state = false
        lcdChange = true
    end
    if event == EVT_ENTER_LONG or event == EVT_MENU_LONG then
        if selection.selected > 3 then
            selection.state = not selection.state
            lcdChange = true
        end

        if selection.selected == 5 or selection.selected == 6 then
            if binding then
                playTone(1750, 150, 0)
                playTone(1500, 150, 200)
            else
                playTone(1500, 150, 0)
                playTone(1750, 150, 200)
            end
            binding = not binding
        end
        -- killEvents(EVT_ENTER_LONG) -- not working
        -- if sensor.sensorId.selected ~= 29 and sensor.sensorType.selected ~= 11  then
        --  sendIdState = 1
        --  lcdChange = true
        -- end
    end
    -- if readIdState > 0 then readId() end
    -- if sendIdState > 0 then sendId() end
    
    return 0
end

return {run = run_func, background = bg_func, init = init_func}
