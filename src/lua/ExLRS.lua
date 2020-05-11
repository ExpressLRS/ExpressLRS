--[[
        ChgId

      DanielGeA

  License https://www.gnu.org/licenses/gpl-3.0.en.html

  Ported from erskyTx. Thanks to MikeB

  Lua script for radios X7, X9, X-lite and Horus with openTx 2.2 or higher

  Change Frsky sensor Id

]] --
local version = 'v0.1'
local gotFirstResp = false

local AirRate = {
    editable = true,
    name = 'Pkt. Rate',
    selected = 99,
    list = {'200 Hz', '100 Hz', '50 Hz'},
    values = {0x00, 0x01, 0x02},
    allowed = 3
}
local TLMinterval = {
    editable = true,
    name = 'TLM Ratio',
    selected = 99,
    list = {'Off', '1:128', '1:64', '1:32', '1:16', '1:8', '1:4', '1:2', 'Default'},
    values = {0x00, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 0xFF},
    allowed = 9
}
local MaxPower = {
    editable = true,
    name = 'Power',
    selected = 99,
    list = {'10 mW', '25 mW', '50 mW', '100 mW', '250 mW', '500 mW', '1000 mW', '2000 mW'},
    values = {0x00, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07},
    allowed = 8
}
local RFfreq = {
    editable = false,
    name = 'RF Freq',
    selected = 99,
    list = {'915 MHz', '868 MHz', '433 MHz'},
    values = {0x00, 0x01, 0x02},
    allowed = 3
}

local selection = {
    selected = 1,
    modify = false,
    -- Note: list indexes must match to param handling in tx_main!
    list = {AirRate, TLMinterval, MaxPower, RFfreq},
    allowed = 4
}

-- returns flags to pass to lcd.drawText for inverted and flashing text
local function getFlags(element)
    if selection.selected ~= element then return 0 end
    if selection.selected == element and selection.modify == false then
        return 0 + INVERS
    end
    -- this element is currently selected
    return 0 + INVERS + BLINK
end

-- ################################################

local supportedRadios =
{
    ["128x64"]  =
    {
        highRes         = false,
        textSize        = SMLSIZE,
        xOffset         = 60,
        yOffset         = 10,
        hasTitle        = true,
    },
    ["212x64"]  =
    {
        highRes         = false,
        textSize        = SMLSIZE,
        xOffset         = 60,
        yOffset         = 10,
        hasTitle        = true,
    },
    ["480x272"] =
    {
        highRes         = true,
        textSize        = 0,
        xOffset         = 100,
        yOffset         = 20,
        hasTitle        = false,
    },
    ["320x480"] =
    {
        highRes         = true,
        textSize        = 0,
        xOffset         = 60,
        yOffset         = 20,
        hasTitle        = false,
    },
}

local radio_resolution = LCD_W.."x"..LCD_H
local radio_data = assert(supportedRadios[radio_resolution], radio_resolution.." not supported")

-- redraw the screen
local function refreshLCD()

    local yOffset = 1

    lcd.clear()
    lcd.drawText(1, yOffset, 'ExpressLRS CFG ' .. version, INVERS)
    yOffset = yOffset + radio_data.yOffset + 4

    for idx,item in pairs(selection.list) do
        local value = '?'
        if item.selected <= #item.list and item.selected <= item.allowed then
            value = item.list[item.selected]
        end
        lcd.drawText(1, yOffset, item.name, radio_data.textSize)
        lcd.drawText(radio_data.xOffset, yOffset, value, getFlags(idx) + radio_data.textSize)
        yOffset = yOffset + radio_data.yOffset
    end

    --[[
    lcd.drawText(18, 54, '[Bind]', getFlags(5) + SMLSIZE)
    lcd.drawText(55, 54, '[Web Server]', getFlags(6) + SMLSIZE)

    if selection.selected > 4 then
        if selection.modify == true then
            lcd.drawText(7, 53, 'Press [ENTER] to stop', MEDSIZE)
        end
    end
    ]]--
end

local function increase(_selection)
    local item = _selection
    if item.modify then
        item = item.list[item.selected]
    end

    if item.selected < #item.list and item.selected < item.allowed then
        item.selected = item.selected + 1
        --playTone(2000, 50, 0)
    end
end

local function decrease(_selection)
    local item = _selection
    if item.modify then
        item = item.list[item.selected]
    end
    if item.selected > 1 and item.selected <= #item.list then
        item.selected = item.selected - 1
        --playTone(2000, 50, 0)
    end
end

-- ################################################

--[[
It's unclear how the telemetry push/pop system works. We don't always seem to get
a response to a single push event. Can multiple responses be stacked up? Do they timeout?

If there are multiple repsonses we typically want the newest one, so this method
will keep reading until it gets a nil response, discarding the older data. A maximum number
of reads is used to defend against the possibility of this function running for an extended
period.

]]--
local function processResp()
    local tries = 0
    local MAX_TRIES = 5

    while tries < MAX_TRIES
    do
        local command, data = crossfireTelemetryPop()
        if (data == nil) then
            return
        else
            if (command == 0x2D) and (data[1] == 0xEA) and (data[2] == 0xEE) then
                if data[3] < #AirRate.list then
                    AirRate.selected = data[3] + 1
                end
                if data[4] < #TLMinterval.list then
                    TLMinterval.selected = data[4] + 1
                end
                if data[5] < #MaxPower.list then
                    MaxPower.selected = data[5] + 1
                end
                if data[6] < #MaxPower.list then
                    MaxPower.allowed = data[6] + 1 -- limit power list
                end
                if data[7] ~= 0xff and data[7] < #RFfreq.list then
                    RFfreq.selected = data[7] + 1
                end

                gotFirstResp = true -- detect when first contact is made with TX module
            end
        end
        tries = tries + 1
    end
end

local function init_func()
end

local function bg_func(event)
end

--[[
  Called at (unspecified) intervals when the script is running and the screen is visible

  Handles key presses and sends state changes to the tx module.

  Basic strategy:
    read any outstanding telemetry data
    process the event, sending a telemetryPush if necessary
    if there was no push due to events, send the void push to ensure current values are sent for next iteration
    redraw the display

]]--
local function run_func(event)

    processResp() -- first check if we have data from the module

    if gotFirstResp == false then
        crossfireTelemetryPush(0x2D, {0xEE, 0xEA, 0x00, 0x00}) -- ping until we get a resp
    end

    -- now process key events
    if event == EVT_ROT_LEFT or
       event == EVT_MINUS_BREAK or
       event == EVT_DOWN_BREAK then
        decrease(selection)

    elseif event == EVT_ROT_RIGHT or
           event == EVT_PLUS_BREAK or
           event == EVT_UP_BREAK then
        increase(selection)

    elseif event == EVT_ENTER_BREAK then
        if selection.modify then
            -- update module when edit ready
            local type = selection.selected
            local item = selection.list[type]
            local value = 0
            if item.selected <= #item.values then
                value = item.values[item.selected]
            else
                type = 0
            end
            crossfireTelemetryPush(0x2D, {0xEE, 0xEA, type, value})
            selection.modify = false
        elseif selection.list[selection.selected].editable and item.selected <= #item.values then
            -- allow modification only if not readonly and values received from module
            selection.modify = true
        end

    elseif event == EVT_EXIT_BREAK and selection.modify then
        -- I was hoping to find the T16 RTN button as an alternate way of deselecting
        -- a field, but no luck so far
        selection.modify = false
        crossfireTelemetryPush(0x2D, {0xEE, 0xEA, 0x00, 0x00}) -- refresh data
    end

    refreshLCD()

    return 0
end

--return {run = run_func, background = bg_func, init = init_func}
return {run = run_func}
