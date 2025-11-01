#include "targets.h"

#if defined(RADIO_LR1121)

#include "ArduinoJson.h"
#include "AsyncJson.h"
#include <ESPAsyncWebServer.h>
#include <StreamString.h>
#include <SPIFFS.h>

#include "common.h"
#include "logging.h"

#if defined(TARGET_TX)
#include "wifiJoystick.h"
#endif

extern LR1121Hal hal;

static void WebUploadLR1121ResponseHandler(AsyncWebServerRequest *request)
{
    const int uploadError = Radio.EndUpdate();

    String msg;
    if (uploadError == 0)
    {
        msg = String(R"({"status": "ok", "msg": "Update complete. Refresh page to see new version information."})");
        // add tag file for lr1121 custom firmware
        if (!SPIFFS.exists("/lr1121.txt"))
        {
            File tagFile = SPIFFS.open("/lr1121.txt", "w");
            tagFile.close();
        }
        DBGLN("Update complete");
    }
    else
    {
        StreamString p;
        if (uploadError == -1)
        {
            p.print("Not enough data uploaded!");
        }
        else
        {
            p.print("Update failed, refresh and try again.");
        }
        DBGLN("Failed to upload firmware: %s", p.c_str());
        msg = String(R"({"status": "error", "msg": ")") + p + R"("})";
    }
    AsyncWebServerResponse *response = request->beginResponse(200, "application/json", msg);
    response->addHeader("Connection", "close");
    request->send(response);
}

static void WebUploadLR1121DataHandler(const AsyncWebServerRequest *request, const String& filename, const size_t index, uint8_t *data, const size_t len, bool final)
{
    if (index == 0)
    {
#if defined(TARGET_TX)
        WifiJoystick::StopJoystickService();
#endif
        const uint32_t expectedFilesize = request->header("X-FileSize").toInt();
        const uint32_t updatingRadio = request->header("X-Radio").toInt();
        DBGLN("Update: '%s' size %u on radio %d", filename.c_str(), expectedFilesize, updatingRadio);

        Radio.BeginUpdate(updatingRadio, expectedFilesize);
    }
    if (len)
    {
        Radio.WriteUpdateBytes(data, len);
    }
}

static void ReadStatusForRadio(const JsonObject json, const SX12XX_Radio_Number_t radio)
{
    const firmware_version_t version = Radio.GetFirmwareVersion(radio);
    json["hardware"] = version.hardware;
    json["type"] = version.type;
    json["firmware"] = version.version;
}

static void GetLR1121Status(AsyncWebServerRequest *request)
{
    const auto response = new AsyncJsonResponse();
    const JsonObject json = response->getRoot();
    hal.end();
    hal.init();
    hal.reset();
    json["manual"] = SPIFFS.exists("/lr1121.txt");
    ReadStatusForRadio(json["radio1"].to<JsonObject>(), SX12XX_Radio_1);
    if (GPIO_PIN_NSS_2 != UNDEF_PIN)
    {
        ReadStatusForRadio(json["radio2"].to<JsonObject>(), SX12XX_Radio_2);
    }
    response->setLength();
    request->send(response);
}

void addLR1121Handlers(AsyncWebServer &server)
{
    server.on("/lr1121.json", HTTP_GET, GetLR1121Status);
    server.on("/lr1121", HTTP_POST, WebUploadLR1121ResponseHandler, WebUploadLR1121DataHandler);
}
#endif
