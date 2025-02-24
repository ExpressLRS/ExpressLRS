#include "targets.h"

#if (defined(PLATFORM_ESP8266) || defined(PLATFORM_ESP32)) && defined(RADIO_LR1121)

#include "ArduinoJson.h"
#include "AsyncJson.h"
#include <ESPAsyncWebServer.h>
#include <StreamString.h>

#include "common.h"
#include "SPIEx.h"
#include "logging.h"

struct lr1121UpdateState_s {
    size_t expectedFilesize;
    size_t totalSize;
    SX12XX_Radio_Number_t updatingRadio;
    size_t left_over;
    struct {
        uint8_t header[6];
        uint8_t buffer[256];
    } __attribute__((packed)) packet;
};
static lr1121UpdateState_s *lr1121UpdateState;

extern LR1121Hal hal;

static void writeLR1121Bytes(uint8_t *data, uint32_t data_size) {
    lr1121UpdateState->packet.header[0] = (uint8_t)(LR11XX_BL_WRITE_FLASH_ENCRYPTED_OC >> 8);
    lr1121UpdateState->packet.header[1] = (uint8_t)(LR11XX_BL_WRITE_FLASH_ENCRYPTED_OC);
    lr1121UpdateState->packet.header[2] = (uint8_t)(lr1121UpdateState->totalSize >> 24);
    lr1121UpdateState->packet.header[3] = (uint8_t)(lr1121UpdateState->totalSize >> 16);
    lr1121UpdateState->packet.header[4] = (uint8_t)(lr1121UpdateState->totalSize >> 8);
    lr1121UpdateState->packet.header[5] = (uint8_t)(lr1121UpdateState->totalSize);

    uint32_t write_size = lr1121UpdateState->left_over;
    if (data != nullptr)
    {
        DBGLN("left %d, new %d", lr1121UpdateState->left_over, data_size);
        memcpy(lr1121UpdateState->packet.buffer + lr1121UpdateState->left_over, data, data_size);
        write_size += data_size;
    }
    DBGLN("flashing %d at %x", write_size, lr1121UpdateState->totalSize);

    // Have to do this the OLD way, so we can pump out more than 64 bytes in one message
    digitalWrite(lr1121UpdateState->updatingRadio == SX12XX_Radio_1 ? GPIO_PIN_NSS : GPIO_PIN_NSS_2, LOW);
    SPIEx.transferBytes(lr1121UpdateState->packet.header, nullptr, 6 + write_size);
    digitalWrite(lr1121UpdateState->updatingRadio == SX12XX_Radio_1 ? GPIO_PIN_NSS : GPIO_PIN_NSS_2, HIGH);

    while (digitalRead(lr1121UpdateState->updatingRadio == SX12XX_Radio_1 ? GPIO_PIN_BUSY : GPIO_PIN_BUSY_2) == HIGH)
    {
        delay(1);
    }
    lr1121UpdateState->totalSize += write_size;
    lr1121UpdateState->left_over = 0;
    DBGLN("flashed");
}

static void readRegister(SX12XX_Radio_Number_t radio, uint16_t reg, uint8_t *buffer, uint32_t buffer_len)
{
    uint8_t read[5];
    read[0] = reg >> 8;
    read[1] = (uint8_t)reg;
    SPIEx.write(radio, read, 2);
    hal.WaitOnBusy(radio);
    memset(buffer, 0, buffer_len);
    SPIEx.read(radio, buffer, buffer_len);
    hal.WaitOnBusy(radio);
}

static void WebUploadLR1121ResponseHandler(AsyncWebServerRequest *request) {
    // Complete upload and set error flag
    bool uploadError = false;
    writeLR1121Bytes(nullptr, 0);

    DBGLN("finished expected %d, total %d, current %d", lr1121UpdateState->expectedFilesize, lr1121UpdateState->totalSize, lr1121UpdateState->currentSize);

    SPIEx.setHwCs(true);
    if (GPIO_PIN_NSS_2 != UNDEF_PIN)
    {
        spiAttachSS(SPIEx.bus(), 1, GPIO_PIN_NSS_2);
    }

    if (lr1121UpdateState->totalSize == lr1121UpdateState->expectedFilesize)
    {
        DBGLN("reboot 1121");
        uint8_t reboot_cmd[] = {
            (uint8_t)(LR11XX_BL_REBOOT_OC >> 8),
            (uint8_t)LR11XX_BL_REBOOT_OC,
            0
        };
        SPIEx.write(lr1121UpdateState->updatingRadio, reboot_cmd, 3);
        while(!hal.WaitOnBusy(lr1121UpdateState->updatingRadio));

        DBGLN("check not in BL mode");
        uint8_t packet[5];
        readRegister(lr1121UpdateState->updatingRadio, LR11XX_SYSTEM_GET_VERSION_OC, packet, 5);
        uploadError = (packet[2] != 3 && packet[2] != 0xE1);
        DBGLN("hardware %x", packet[1]);
        DBGLN("type %x", packet[2]);
        DBGLN("firmware %x", ( ( uint16_t )packet[3] << 8 ) + ( uint16_t )packet[4]);
    }

    String msg;
    if (!uploadError && lr1121UpdateState->totalSize == lr1121UpdateState->expectedFilesize) {
        msg = String(R"({"status": "ok", "msg": "Update complete. Refresh page to see new version information."})");
        DBGLN("Update complete");
    } else {
        StreamString p = StreamString();
        if (lr1121UpdateState->totalSize != lr1121UpdateState->expectedFilesize) {
            p.print("Not enough data uploaded!");
        } else {
            p.print("Update failed, refresh and try again.");
        }
        DBGLN("Failed to upload firmware: %s", p.c_str());
        msg = String(R"({"status": "error", "msg": ")") + p + R"("})";
    }
    AsyncWebServerResponse *response = request->beginResponse(200, "application/json", msg);
    response->addHeader("Connection", "close");
    request->send(response);
    delete lr1121UpdateState;
    lr1121UpdateState = nullptr;
}

static void WebUploadLR1121DataHandler(AsyncWebServerRequest *request, const String& filename, size_t index, uint8_t *data, size_t len, bool final) {
    if (index == 0) {
#ifdef HAS_WIFI_JOYSTICK
        WifiJoystick::StopJoystickService();
#endif
        lr1121UpdateState = new lr1121UpdateState_s;
        lr1121UpdateState->expectedFilesize = request->header("X-FileSize").toInt();
        lr1121UpdateState->updatingRadio = request->header("X-Radio").toInt();
        DBGLN("Update: '%s' size %u on radio %d", filename.c_str(), lr1121UpdateState->expectedFilesize, lr1121UpdateState->updatingRadio);
        lr1121UpdateState->totalSize = 0;
        lr1121UpdateState->left_over = 0;

        // Reboot to BL mode
        DBGLN("Reboot 1121 to bootloader mode");
        uint8_t reboot_cmd[] = {
            (uint8_t)(LR11XX_SYSTEM_REBOOT_OC >> 8),
            (uint8_t)LR11XX_SYSTEM_REBOOT_OC,
            3
        };
        SPIEx.write(lr1121UpdateState->updatingRadio, reboot_cmd, 3);
        while(!hal.WaitOnBusy(lr1121UpdateState->updatingRadio)) {
            DBGLN("Waiting...");
            delay(10);
        }

        // Ensure we're in BL mode
        DBGLN("Ensure BL mode");
        uint8_t packet[5];
        readRegister(lr1121UpdateState->updatingRadio, LR11XX_BL_GET_VERSION_OC, packet, 5);
        if (packet[2] != 0xDF) {
            AsyncWebServerResponse *response = request->beginResponse(200, "application/json", R"({"status": "error", "msg": "Not in bootloader mode"})");
            response->addHeader("Connection", "close");
            request->send(response);
            request->client()->close();
            return;
        }

        // Erase flash
        DBGLN("Erasing");
        packet[0] = LR11XX_BL_ERASE_FLASH_OC >> 8;
        packet[1] = (uint8_t)LR11XX_BL_ERASE_FLASH_OC;
        SPIEx.write(lr1121UpdateState->updatingRadio, packet, 2);
        while(!hal.WaitOnBusy(lr1121UpdateState->updatingRadio))
        {
            DBGLN("Waiting...");
            delay(100);
        }
        DBGLN("Erased");

        lr1121UpdateState->left_over = 0;
        SPIEx.setHwCs(false);

        pinMode(lr1121UpdateState->updatingRadio == SX12XX_Radio_1 ? GPIO_PIN_NSS : GPIO_PIN_NSS_2, OUTPUT);
        digitalWrite(lr1121UpdateState->updatingRadio == SX12XX_Radio_1 ? GPIO_PIN_NSS : GPIO_PIN_NSS_2, HIGH);
    }
    if (len) {
        DBGLN("got %d", len);
        // Write len bytes to LR1121 from data
        while (len >= 256 - lr1121UpdateState->left_over)
        {
            uint32_t chunk_size = len > 256 - lr1121UpdateState->left_over ? 256 - lr1121UpdateState->left_over : len;
            writeLR1121Bytes(data, chunk_size);
            len -= chunk_size;
            data += chunk_size;
        }
        memcpy(lr1121UpdateState->packet.buffer + lr1121UpdateState->left_over, data, len);
        lr1121UpdateState->left_over += len;
        DBGLN("left-over %d", lr1121UpdateState->left_over);
    }
}

static void ReadStatusForRadio(JsonObject json, SX12XX_Radio_Number_t radio)
{
    uint8_t packet[9];
    readRegister(radio, LR11XX_BL_GET_VERSION_OC, packet, 5);
    json["hardware"] = packet[1];
    json["type"] = packet[2];
    json["firmware"] = ( ( uint16_t )packet[3] << 8 ) + ( uint16_t )packet[4];

    readRegister(radio, LR11XX_BL_GET_PIN_OC, packet, 5);
    copyArray(packet+1, 4, json["pin"].to<JsonArray>());

    readRegister(radio, LR11XX_BL_READ_CHIP_EUI_OC, packet, 9);
    copyArray(packet+1, 8, json["ceui"].to<JsonArray>());

    readRegister(radio, LR11XX_BL_READ_JOIN_EUI_OC, packet, 9);
    copyArray(packet+1, 8, json["jeui"].to<JsonArray>());
}

static void GetLR1121Status(AsyncWebServerRequest *request)
{
    AsyncJsonResponse *response = new AsyncJsonResponse();
    JsonObject json = response->getRoot();
    hal.end();
    hal.init();
    hal.reset();

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
