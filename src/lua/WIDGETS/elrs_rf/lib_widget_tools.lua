local m_log, app_name = ...

local M = {}
M.m_log = m_log
M.app_name = app_name
M.tele_src_name = nil
M.tele_src_id = nil

local getTime = getTime
local lcd = lcd

-- better font names
local FONT_38 = XXLSIZE -- 38px
local FONT_16 = DBLSIZE -- 16px
local FONT_12 = MIDSIZE -- 12px
local FONT_8 = 0 -- Default 8px
local FONT_6 = SMLSIZE -- 6px

---------------------------------------------------------------------------------------------------
local function log(fmt, ...)
    m_log.info(fmt, ...)
end
---------------------------------------------------------------------------------------------------

-- const's
local UNIT_ID_TO_STRING = {
    "V", "A", "mA", "kts", "m/s", "f/s", "km/h", "mph", "m", "f",
    "°C", "°F", "%", "mAh", "W", "mW", "dB", "rpm", "g", "°",
    "rad", "ml", "fOz", "ml/m", "Hz", "mS", "uS", "km"
}

function M.unitIdToString(unitId)
    if unitId == nil then
        return ""
    end
    -- UNIT_RAW
    if unitId == "0" then
        return ""
    end

    --log("idUnit: " .. unitId)

    if (unitId > 0 and unitId <= #UNIT_ID_TO_STRING) then
        local txtUnit = UNIT_ID_TO_STRING[unitId]
        --log("txtUnit: " .. txtUnit)
        return txtUnit
    end

    return "-#-"
end

---------------------------------------------------------------------------------------------------

function M.periodicInit()
    local t = {}
    t.startTime = -1;
    t.durationMili = -1;
    return t
end

function M.periodicStart(t, durationMili)
    t.startTime = getTime();
    t.durationMili = durationMili;
end

function M.periodicHasPassed(t, show_log)
    -- not started yet
    if (t.durationMili <= 0) then
        return false;
    end

    local elapsed = getTime() - t.startTime;
    --log('elapsed: %d (t.durationMili: %d)', elapsed, t.durationMili)
    if show_log == true then
        log('elapsed: %0.1f/%0.1f sec', elapsed/100, t.durationMili/1000)
    end
    local elapsedMili = elapsed * 10;
    if (elapsedMili < t.durationMili) then
        return false;
    end
    return true;
end

function M.periodicGetElapsedTime(t, show_log)
    local elapsed = getTime() - t.startTime;
    local elapsedMili = elapsed * 10;
    if show_log == true then
        log('elapsed: %0.1f/%0.1f sec', elapsed/100, t.durationMili/1000)
    end
    return elapsedMili;
end

function M.periodicReset(t)
    t.startTime = getTime();
    --log("periodicReset()");
    M.periodicGetElapsedTime(t)
end

function M.getDurationMili(t)
    return t.durationMili
end

---------------------------------------------------------------------------------------------------

function M.isTelemetryAvailable()
    -- select telemetry source
    if not M.tele_src_id then
        log("select telemetry source")
        local tele_src = getFieldInfo("RSSI")
        if not tele_src then tele_src = getFieldInfo("RxBt") end
        if not tele_src then tele_src = getFieldInfo("A1") end
        if not tele_src then tele_src = getFieldInfo("A2") end
        if not tele_src then tele_src = getFieldInfo("1RSS") end
        if not tele_src then tele_src = getFieldInfo("2RSS") end
        if not tele_src then tele_src = getFieldInfo("RQly") end
        if not tele_src then tele_src = getFieldInfo("TRSS") end
        if not tele_src then tele_src = getFieldInfo("VFR%") end

        if tele_src == nil then
            log("no telemetry sensor found")
            M.tele_src_id = nil
            M.tele_src_name = "---"
            return false
        else
            log("telemetry sensor found: " .. tele_src.name)
            M.tele_src_id = tele_src.id
            M.tele_src_name = tele_src.name
        end
    end

    if M.tele_src_id == nil then
        return false
    end

    local rx_val = getValue(M.tele_src_id)
    if rx_val ~= 0 then
        return true
    end
    return false
end

---------------------------------------------------------------------------------------------------

-- workaround to detect telemetry-reset event, until a proper implementation on the lua interface will be created
-- this workaround assume that:
--   RSSI- is always going down
--   RSSI- is reset on the C++ side when a telemetry-reset is pressed by user
--   widget is calling this func on each refresh/background
-- on event detection, the function onTelemetryResetEvent() will be trigger
--
function M.detectResetEvent(wgt, callback_onTelemetryResetEvent)

    local currMinRSSI = getValue('RSSI-')
    if (currMinRSSI == nil) then
        log("telemetry reset event: can not be calculated")
        return
    end
    if (currMinRSSI == wgt.telemResetLowestMinRSSI) then
        --log("telemetry reset event: not found")
        return
    end

    if (currMinRSSI < wgt.telemResetLowestMinRSSI) then
        -- rssi just got lower, record it
        wgt.telemResetLowestMinRSSI = currMinRSSI
        --log("telemetry reset event: not found")
        return
    end

    -- reset telemetry detected
    wgt.telemResetLowestMinRSSI = 101
    log("telemetry reset event detected")

    -- notify event
    callback_onTelemetryResetEvent(wgt)
end

---------------------------------------------------------------------------------------------------
function M.getSensorPrecession(sensorName)

    for i=0, 30, 1 do
        local s2 = model.getSensor(i)
        --type (number) 0 = custom, 1 = calculated
        --name (string) Name
        --unit (number) See list of units in the appendix of the OpenTX Lua Reference Guide
        --prec (number) Number of decimals
        --id (number) Only custom sensors
        --instance (number) Only custom sensors
        --formula (number) Only calculated sensors. 0 = Add etc. see list of formula choices in Companion popup

        --log("getSensorPrecession: %d. name: %s, unit: %s , prec: %s , id: %s , instance: %s ", i, s2.name, s2.unit, s2.prec, s2.id, s2.instance)

        if s2.name == sensorName then
            return s2.prec
        end
    end

    return -1
end

--function M.getSensors()
--    local sensors = {}
--    for i=0, 30, 1 do
--        local s1 = {}
--        local s2 = model.getSensor(i)
--
--        --type (number) 0 = custom, 1 = calculated
--        s1.type = s2.type
--        --name (string) Name
--        s1.name = s2.name
--        --unit (number) See list of units in the appendix of the OpenTX Lua Reference Guide
--        s1.unit = s2.unit
--        --prec (number) Number of decimals
--        s1.prec = s2.prec
--        --id (number) Only custom sensors
--        s1.id = s2.id
--        --instance (number) Only custom sensors
--        s1.instance = s2.instance
--        --formula (number) Only calculated sensors. 0 = Add etc. see list of formula choices in Companion popup
--        s1.formula = s2.formula
--
--        s1.appendix
--
--        log("getSensorPrecession: %d. name: %s, unit: %s , prec: %s , id: %s , instance: %s ", i, s2.name, s2.unit, s2.prec, s2.id, s2.instance)
--
--        if s2.name == sensorName then
--            return s2.prec
--        end
--    end
--
--    return -1
--end

---------------------------------------------------------------------------------------------------
-- workaround for bug in getFiledInfo()  -- ???? why?
function M.cleanInvalidCharFromGetFiledInfo(sourceName)

    if string.byte(string.sub(sourceName, 1, 1)) > 127 then
        sourceName = string.sub(sourceName, 2, -1)
    end
    if string.byte(string.sub(sourceName, 1, 1)) > 127 then
        sourceName = string.sub(sourceName, 2, -1)
    end

    return sourceName
end

------------------------------------------------------------------------------------------------------

function M.lcdSizeTextFixed(txt, font_size)
    local ts_w, ts_h = lcd.sizeText(txt, font_size)

    local v_offset = 0
    if font_size == FONT_38 then
        v_offset = -11
    elseif font_size == FONT_16 then
        v_offset = -5
    elseif font_size == FONT_12 then
        v_offset = -4
    elseif font_size == FONT_8 then
        v_offset = -3
    elseif font_size == FONT_6 then
        v_offset = 0
    end
    return ts_w, ts_h, v_offset
end

------------------------------------------------------------------------------------------------------

function M.drawBadgedText(txt, txtX, txtY, font_size, text_color, background_color)
    local ts_w, ts_h = lcd.sizeText(txt, font_size)
    local r = ts_h / 2
    lcd.drawFilledCircle(txtX , txtY + r, r, background_color)
    lcd.drawFilledCircle(txtX + ts_w , txtY + r, r, background_color)
    lcd.drawFilledRectangle(txtX, txtY , ts_w, ts_h, background_color)
    lcd.drawText(txtX, txtY, txt, font_size + text_color)
end

------------------------------------------------------------------------------------------------------

return M
