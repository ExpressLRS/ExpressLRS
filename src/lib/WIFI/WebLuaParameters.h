#pragma once

#include <ESPAsyncWebServer.h>
#include <ArduinoJson.h>

/**
 * Handles HTTP GET /lua-parameters endpoint.
 * Returns all CRSF Lua parameters from the active endpoint (TX or RX).
 */
void HandleGetLuaParameters(AsyncWebServerRequest *request);

/**
 * Handles HTTP POST /lua-parameters/save endpoint.
 * Saves parameter changes to the active endpoint (TX or RX).
 * 
 * This signature matches AsyncCallbackJsonWebHandler callback requirements.
 */
void HandleSaveLuaParameters(AsyncWebServerRequest *request, JsonVariant &json);
