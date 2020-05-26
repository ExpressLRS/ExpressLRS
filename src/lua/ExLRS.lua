--[[
  Change ExpressLRS parameters

  License https://www.gnu.org/licenses/gpl-3.0.en.html

  Lua script for radios X7, X9, X-lite and Horus with openTx 2.2 or higher

  Original author: AlessandroAU.
]] --

local version = 'v0.1'
local gotFirstResp = false

local AirRate = {
    index = 1,
    editable = true,
    name = 'Pkt. Rate',
    selected = 99,
    list = {'200 Hz', '100 Hz', '50 Hz'},
    values = {0x00, 0x01, 0x02},
    max_allowed = 3,
}

local TLMinterval = {
    index = 2,
    editable = true,
    name = 'TLM Ratio',
    selected = 99,
    list = {'Off', '1:128', '1:64', '1:32', '1:16', '1:8', '1:4', '1:2', 'Default'},
    values = {0x00, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 0xFF},
    max_allowed = 9,
}

local MaxPower = {
    index = 3,
    editable = true,
    name = 'Power',
    selected = 99,
    list = {'Dynamic', '10 mW', '25 mW', '50 mW', '100 mW', '250 mW', '500 mW', '1000 mW', '2000 mW'},
    values = {0x00, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 0x08},
    max_allowed = 9,
}

local RFfreq = {
    index = 4,
    editable = false,
    name = 'RF Freq',
    selected = 99,
    list = {'915 MHz', '868 MHz', '433 MHz'},
    values = {0x00, 0x01, 0x02},
    max_allowed = 3,
}

local function binding(item, event)
    playTone(2000, 50, 0)
    item.exec = false
    return 0
end
local Bind = {
    index = 5,
    editable = false,
    name = '[Bind]',
    exec = false,
    func = binding,
    selected = 99,
    list = {},
    values = {},
    max_allowed = 0,
    offsets = {left=5, right=0, top=5, bottom=5},
}

local function web_server_start(item, event)
    playTone(2000, 50, 0)
    item.exec = false
    return 0
end
local WebServer = {
    index = 5,
    editable = false,
    name = '[Web Server]',
    exec = false,
    func = web_server_start,
    selected = 99,
    list = {},
    values = {},
    max_allowed = 0,
    offsets = {left=65, right=0, top=5, bottom=5},
}

local exit_script = {
    index = 6,
    editable = false,
    action = 'exit',
    name = '[EXIT]',
    selected = 99,
    list = {},
    values = {},
    max_allowed = 0,
    offsets = {left=5, right=0, top=5, bottom=5},
}

local menu = {
    selected = 1,
    modify = false,
    -- Note: list indexes must match to param handling in tx_main!
    --list = {AirRate, TLMinterval, MaxPower, RFfreq, Bind, WebServer, exit_script},
    list = {AirRate, TLMinterval, MaxPower, RFfreq, exit_script},
}

-- returns flags to pass to lcd.drawText for inverted and flashing text
local function getFlags(element)
    if menu.selected ~= element then return 0 end
    if menu.selected == element and menu.modify == false then
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
        --highRes         = false,
        textSize        = SMLSIZE,
        xOffset         = 60,
        yOffset         = 10,
        topOffset       = 1,
        leftOffset      = 1,
    },
    ["212x64"]  =
    {
        --highRes         = false,
        textSize        = SMLSIZE,
        xOffset         = 60,
        yOffset         = 10,
        topOffset       = 1,
        leftOffset      = 1,
    },
    ["480x272"] =
    {
        --highRes         = true,
        textSize        = 0,
        xOffset         = 100,
        yOffset         = 20,
        topOffset       = 1,
        leftOffset      = 1,
    },
    ["320x480"] =
    {
        --highRes         = true,
        textSize        = 0,
        xOffset         = 120,
        yOffset         = 25,
        topOffset       = 5,
        leftOffset      = 5,
    },
}

local radio_resolution = LCD_W.."x"..LCD_H
local radio_data = assert(supportedRadios[radio_resolution], radio_resolution.." not supported")

-- redraw the screen
local function refreshLCD()

    local yOffset = radio_data.topOffset;
    local lOffset = radio_data.leftOffset;

    lcd.clear()
    lcd.drawText(lOffset, yOffset, 'ExpressLRS CFG ' .. version, INVERS)
    yOffset = 5

    for idx,item in pairs(menu.list) do
        local offsets = {left=0, right=0, top=0, bottom=0}
        if item.offsets ~= nil then
            offsets = item.offsets
        end
        lOffset = offsets.left + radio_data.leftOffset
        local item_y = yOffset + offsets.top + radio_data.yOffset * item.index
        if item.action ~= nil or item.func ~= nil then
            lcd.drawText(lOffset, item_y, item.name, getFlags(idx) + radio_data.textSize)
        else
            local value = '?'
            if 0 < item.selected and item.selected <= #item.list and item.selected <= item.max_allowed then
                value = item.list[item.selected]
            end
            lcd.drawText(lOffset, item_y, item.name, radio_data.textSize)
            lcd.drawText(radio_data.xOffset, item_y, value, getFlags(idx) + radio_data.textSize)
        end
    end
end

local function increase(_menu)
    local item = _menu
    if item.modify then
        item = item.list[item.selected]
    end

    if item.selected < #item.list and
       (item.max_allowed == nil or item.selected < item.max_allowed) then
        item.selected = item.selected + 1
        --playTone(2000, 50, 0)
    end
end

local function decrease(_menu)
    local item = _menu
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
                    MaxPower.max_allowed = data[6] + 1 -- limit power list
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

    local type = menu.selected
    local item = menu.list[type]

    if item.exec == true and item.func ~= nil then
        local retval = item.func(item, event)
        refreshLCD()
        return retval
    end

    -- now process key events
    if event == EVT_VIRTUAL_ENTER_LONG or
       event == EVT_ENTER_LONG or
       event == EVT_MENU_LONG then
        -- exit script
        return 2
    elseif event == EVT_VIRTUAL_PREV or
           event == EVT_ROT_LEFT or
           event == EVT_MINUS_BREAK or
           event == EVT_DOWN_BREAK or
           event == EVT_SLIDE_LEFT then
        decrease(menu)

    elseif event == EVT_VIRTUAL_NEXT or
           event == EVT_ROT_RIGHT or
           event == EVT_PLUS_BREAK or
           event == EVT_UP_BREAK or
           event == EVT_SLIDE_RIGHT then
        increase(menu)

    elseif event == EVT_VIRTUAL_ENTER or
           event == EVT_ENTER_BREAK then
        if menu.modify then
            -- update module when edit ready
            local value = 0
            if 0 < item.selected and item.selected <= #item.values then
                value = item.values[item.selected]
            else
                type = 0
            end
            crossfireTelemetryPush(0x2D, {0xEE, 0xEA, type, value})
            menu.modify = false
        elseif item.editable and 0 < item.selected and item.selected <= #item.values then
            -- allow modification only if not readonly and values received from module
            menu.modify = true
        elseif item.func ~= nil then
            item.exec = true
        elseif item.action == 'exit' then
            -- exit script
            return 2
        end

    elseif menu.modify and (event == EVT_VIRTUAL_EXIT or
                            event == EVT_EXIT_BREAK or
                            event == EVT_RTN_FIRST) then
        menu.modify = false
        crossfireTelemetryPush(0x2D, {0xEE, 0xEA, 0x00, 0x00}) -- refresh data
    end

    refreshLCD()

    return 0
end

--return {run = run_func, background = bg_func, init = init_func}
return {run = run_func}
