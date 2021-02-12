--[[
  Change ExpressLRS parameters

  License https://www.gnu.org/licenses/gpl-3.0.en.html

  Lua script for radios X7, X9, X-lite and Horus with openTx 2.2 or higher

  Original author: AlessandroAU + Cruwaller
]] --
local commitSha = '??????'
local shaLUT = {'0', '1', '2', '3', '4', '5', '6', '7', '8', '9', 'a', 'b', 'c', 'd', 'e', 'f'}
local version = 'v0.1'
local gotFirstResp = false
local needResp = false
local NewReqTime = 0;
local ReqWaitTime = 100;
local UartGoodPkts = 0;
local UartBadPkts = 0;
local StopUpdate = false;

local bindmode = false;
local wifiupdatemode = false;

local SX127x_RATES = {
	list = {'25 Hz', '50 Hz', '100 Hz', '200 Hz'},
    values = {0x06, 0x05, 0x04, 0x02},
}
local SX128x_RATES = {
	list = {'25 Hz', '50 Hz', '150 Hz', '250 Hz', '500 Hz'},
    values = {0x06, 0x05, 0x03, 0x01, 0x00},
}

local AirRate = {
    index = 1,
    editable = true,
    name = 'Pkt. Rate',
    selected = 99,
    list = SX127x_RATES.list,
    values = SX127x_RATES.values,
    max_allowed = #SX127x_RATES.values,
}
local TLMinterval = {
    index = 2,
    editable = true,
    name = 'TLM Ratio',
    selected = 99,
    list = {'Off', '1:128', '1:64', '1:32', '1:16', '1:8', '1:4', '1:2'},
    values = {0x00, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07},
    max_allowed = 8,
}

local MaxPower = {
    index = 3,
    editable = true,
    name = 'Power',
    selected = 99,
    list =  {'10 mW', '25 mW', '50 mW', '100 mW', '250 mW', '500 mW', '1000 mW', '2000 mW'},
    values = {0x00, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07},
    max_allowed = 8,
}

local FuncMode = {
    index = 4,
    editable = true,
    name = 'mode',
    selected = 99,
	list = {'0','hybrid8','analog7','3'},
    values = {0x00, 0x01, 0x02, 0x03},
    max_allowed = 4,
}

local RFfreq = {
    index = 5,
    editable = false,
    name = 'RF Freq',
    selected = 99,
    list = {'915 AU', '915 FCC', '868 EU', '433 AU', '433 EU', '2.4G ISM'},
    values = {0x01, 0x02, 0x03, 0x04, 0x05, 0x06},
    max_allowed = 6,
}

local function binding(item, event)
	if (bindmode == true) then
		crossfireTelemetryPush(0x2D, {0xEE, 0xEA, 0xFF, 0x00})
	else
		crossfireTelemetryPush(0x2D, {0xEE, 0xEA, 0xFF, 0x01})
	end
	
	playTone(2000, 50, 0)
    item.exec = false
    return 0
end

local Bind = {
    index = 6,
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
	crossfireTelemetryPush(0x2D, {0xEE, 0xEA, 0xFE, 0x01})
	playTone(2000, 50, 0)
    item.exec = false
    return 0
end

local WebServer = {
    index = 6,
    editable = false,
    name = '[Wifi Update]',
    exec = false,
    func = web_server_start,
    selected = 99,
    list = {},
    values = {},
    max_allowed = 0,
    offsets = {left=65, right=0, top=5, bottom=5},
}

-- local exit_script = {
    -- index = 7,
    -- editable = false,
    -- action = 'exit',
    -- name = '[EXIT]',
    -- selected = 99,
    -- list = {},
    -- values = {},
    -- max_allowed = 0,
    -- offsets = {left=5, right=0, top=5, bottom=5},
-- }

local menu = {
    selected = 1,
    modify = false,
    -- Note: list indexes must match to param handling in tx_main!
    list = {AirRate, TLMinterval, MaxPower, FuncMode, RFfreq, Bind, WebServer},
    --list = {AirRate, TLMinterval, MaxPower, RFfreq, WebServer, exit_script},
}

-- returns flags to pass to lcd.drawText for inverted and flashing text
local function getFlags(element)
    if menu.selected ~= element then return 0 end
    if menu.selected == element and menu.modify == false then
		StopUpdate = false
        return 0 + INVERS
    end
    -- this element is currently selected
	StopUpdate = true
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
        yOffset         = 8,
        yOffset_val     = 3,
        topOffset       = 1,
        leftOffset      = 1,
    },
    ["128x96"]  =
    {
        --highRes         = false,
        textSize        = SMLSIZE,
        xOffset         = 60,
        yOffset         = 8,
        yOffset_val     = 3,
        topOffset       = 1,
        leftOffset      = 1,
    },
    ["212x64"]  =
    {
        --highRes         = false,
        textSize        = SMLSIZE,
        xOffset         = 60,
        yOffset         = 8,
        yOffset_val     = 3,
        topOffset       = 1,
        leftOffset      = 1,
    },
    ["480x272"] =
    {
        --highRes         = true,
        textSize        = 0,
        xOffset         = 100,
        yOffset         = 20,
        yOffset_val     = 5,
        topOffset       = 1,
        leftOffset      = 1,
    },
    ["320x480"] =
    {
        --highRes         = true,
        textSize        = 0,
        xOffset         = 120,
        yOffset         = 25,
        yOffset_val     = 5,
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
	if wifiupdatemode == true then --make this less hacky later
		lcd.drawText(lOffset, yOffset, "Goto http://10.0.0.1   ", INVERS)
	elseif bindmode == true then
		lcd.drawText(lOffset, yOffset, "Binding not yet needed...", INVERS)
	else	
		lcd.drawText(lOffset, yOffset, 'ExpressLRS ' .. commitSha .. '  ' .. tostring(UartBadPkts) .. ':' .. tostring(UartGoodPkts), INVERS)
	end
    
    yOffset = radio_data.yOffset_val

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
			if 0 < item.selected and item.selected <= #item.list and gotFirstResp then
            --if 0 < item.selected and item.selected <= #item.list and item.selected <= item.max_allowed then
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

function GetIndexOf(t,val)
    for k,v in ipairs(t) do 
        if v == val then 
			return k 
		end
    end
end

local function processResp()
	local command, data = crossfireTelemetryPop()
	if (data == nil) then
		return
	else
		if (command == 0x2D) and (data[1] == 0xEA) and (data[2] == 0xEE) then
		
			if(data[3] == 0xFF) and #data == 12 then
				bindmode = bit32.btest(0x01, data[4]) -- bind mode active 
				wifiupdatemode = bit32.btest(0x02, data[4]) 
				
				if StopUpdate == false then 
					TLMinterval.selected = data[6]
					MaxPower.selected = data[7]
                    FuncMode.selected = data[12]
                    if data[8] == 6 then
						-- ISM 2400 band (SX128x)
						AirRate.list = SX128x_RATES.list
						AirRate.values = SX128x_RATES.values
						AirRate.max_allowed = #SX128x_RATES.values
					else
						-- 433/868/915 (SX127x)
						AirRate.list = SX127x_RATES.list
						AirRate.values = SX127x_RATES.values
						AirRate.max_allowed = #SX127x_RATES.values
					end
					RFfreq.selected = data[8]
					AirRate.selected =  GetIndexOf(AirRate.values, data[5])
                    
					
				end
				
				UartBadPkts = data[9]
				UartGoodPkts = data[10] * 256 + data[11] 

			elseif(data[3] == 0xFE) and #data == 9 then -- First half of commit sha
				commitSha = shaLUT[data[4]+1] .. shaLUT[data[5]+1] .. shaLUT[data[6]+1] .. shaLUT[data[7]+1] .. shaLUT[data[8]+1] .. shaLUT[data[9]+1]
			end
			
			if gotFirstResp == false then -- detect when first contact is made with TX module
				gotFirstResp = true
			end
			if needResp == true then
				needResp = false
			end
		end
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

    if (gotFirstResp == false or commitSha == '??????') and (getTime() > (NewReqTime + ReqWaitTime)) then
        crossfireTelemetryPush(0x2D, {0xEE, 0xEA, 0x00, 0x00}) -- ping until we get a resp
		NewReqTime = getTime()
    end
	
	if needResp == true and (getTime() > (NewReqTime + ReqWaitTime)) then
        crossfireTelemetryPush(0x2D, {0xEE, 0xEA, 0x00, 0x00}) -- ping until we get a resp
		NewReqTime = getTime()
    end
	
	processResp() -- check if we have data from the module

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
           event == EVT_VIRTUAL_PREV_REPT or
           event == EVT_ROT_LEFT or
           --event == EVT_MINUS_BREAK or
           event == EVT_SLIDE_LEFT then
        decrease(menu)

    elseif event == EVT_VIRTUAL_NEXT or
           event == EVT_VIRTUAL_NEXT_REPT or
           event == EVT_ROT_RIGHT or
           --event == EVT_PLUS_BREAK or
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
			NewReqTime = getTime()
			needResp = true
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
		NewReqTime = getTime()
		needResp = true
    end

    refreshLCD()

    return 0
end

--return {run = run_func, background = bg_func, init = init_func}
return {run = run_func}
