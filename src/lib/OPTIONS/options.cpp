#include "targets.h"
#include "options.h"
#include "helpers.h"

#include "logging.h"

#define QUOTE(arg) #arg
#define STR(macro) QUOTE(macro)
const unsigned char target_name[] = "\xBE\xEF\xCA\xFE" STR(TARGET_NAME);
const uint8_t target_name_size = sizeof(target_name);
const char commit[] {LATEST_COMMIT, 0};
#if defined(UNIT_TEST)
const char version[] = "1.2.3";
#else
const char version[] = {LATEST_VERSION, 0};
#endif

#if defined(TARGET_TX)
const char *wifi_hostname = "elrs_tx";
const char *wifi_ap_ssid = "ExpressLRS TX";
#else
const char *wifi_hostname = "elrs_rx";
const char *wifi_ap_ssid = "ExpressLRS RX";
#endif
const char *wifi_ap_password = "expresslrs";
const char *wifi_ap_address = "10.0.0.1";

#if !defined(TARGET_UNIFIED_TX) && !defined(TARGET_UNIFIED_RX)
const char device_name[] = DEVICE_NAME;
const char *product_name = (const char *)(target_name+4);

__attribute__ ((used)) const firmware_options_t firmwareOptions = {
    ._magic_ = {0xBE, 0xEF, 0xBA, 0xBE, 0xCA, 0xFE, 0xF0, 0x0D},
    ._version_ = 1,
#if defined(Regulatory_Domain_ISM_2400)
    .domain = 0,
#else
    #if defined(Regulatory_Domain_AU_915)
    .domain = 0,
    #elif defined(Regulatory_Domain_FCC_915)
    .domain = 1,
    #elif defined(Regulatory_Domain_EU_868)
    .domain = 2,
    #elif defined(Regulatory_Domain_IN_866)
    .domain = 3,
    #elif defined(Regulatory_Domain_AU_433)
    .domain = 4,
    #elif defined(Regulatory_Domain_EU_433)
    .domain = 5,
    #else
    #error No regulatory domain defined, please define one in user_defines.txt
    #endif
#endif
#if defined(MY_UID)
    .hasUID = true,
    .uid = { MY_UID },
#else
    .hasUID = false,
    .uid = {},
#endif
#if defined(PLATFORM_ESP32) || defined(PLATFORM_ESP8266)
    #if defined(AUTO_WIFI_ON_INTERVAL)
        .wifi_auto_on_interval = AUTO_WIFI_ON_INTERVAL * 1000,
    #else
        .wifi_auto_on_interval = -1,
    #endif
    #if defined(HOME_WIFI_SSID)
        .home_wifi_ssid = {HOME_WIFI_SSID},
    #else
        .home_wifi_ssid = {},
    #endif
    #if defined(HOME_WIFI_PASSWORD)
        .home_wifi_password = {HOME_WIFI_PASSWORD},
    #else
        .home_wifi_password = {},
    #endif
#endif
#if defined(TARGET_RX)
#if defined(RCVR_UART_BAUD)
    .uart_baud = RCVR_UART_BAUD,
#else
    .uart_baud = 420000,
#endif
#if defined(RCVR_INVERT_TX)
    .invert_tx = true,
#else
    .invert_tx = false,
#endif
#if defined(LOCK_ON_FIRST_CONNECTION)
    .lock_on_first_connection = true,
#else
    .lock_on_first_connection = false,
#endif
#if defined(USE_R9MM_R9MINI_SBUS)
    .r9mm_mini_sbus = true,
#else
    .r9mm_mini_sbus = false,
#endif
#endif
#if defined(TARGET_TX)
#if defined(TLM_REPORT_INTERVAL_MS)
    .tlm_report_interval = TLM_REPORT_INTERVAL_MS,
#else
    .tlm_report_interval = 240U,
#endif
#if defined(FAN_MIN_RUNTIME)
    .fan_min_runtime = FAN_MIN_RUNTIME,
#else
    .fan_min_runtime = 30,
#endif
#if defined(UART_INVERTED) // Only on ESP32
    .uart_inverted = true,
#else
    .uart_inverted = false,
#endif
#if defined(UNLOCK_HIGHER_POWER)
    .unlock_higher_power = true,
#else
    .unlock_higher_power = false,
#endif
#if defined(GPIO_PIN_BUZZER)
    #if defined(DISABLE_ALL_BEEPS)
    .buzzer_mode = buzzerQuiet,
    .buzzer_melody = {},
    #elif defined(JUST_BEEP_ONCE)
    .buzzer_mode = buzzerOne,
    .buzzer_melody = {},
    #elif defined(DISABLE_STARTUP_BEEP)
    .buzzer_mode = buzzerTune,
    .buzzer_melody = {{400, 200}, {480, 200}},
    #elif defined(MY_STARTUP_MELODY)
    .buzzer_mode = buzzerTune,
    .buzzer_melody = MY_STARTUP_MELODY_ARR,
    #else
    .buzzer_mode = buzzerTune,
    .buzzer_melody = {{659, 300}, {659, 300}, {523, 100}, {659, 300}, {783, 550}, {392, 575}},
    #endif
#endif
#endif
};

#else // TARGET_UNIFIED_TX || TARGET_UNIFIED_RX

#include <ArduinoJson.h>
#include <StreamString.h>
#if defined(PLATFORM_ESP8266)
#include <FS.h>
#else
#include <SPIFFS.h>
#endif
#if defined(PLATFORM_ESP32)
#include <esp_partition.h>
#include "esp_ota_ops.h"
#endif

char product_name[129];
char device_name[17];

firmware_options_t firmwareOptions;

extern bool hardware_init(uint32_t *config);

static uint32_t buf[2048];

#if defined(PLATFORM_ESP8266)
// We need our own function on ESP8266 because the builtin crashes!
uint32_t myGetSketchSize()
{
    uint32_t result = 0;
    image_header_t &image_header = *(image_header_t *)buf;
    uint32_t pos = APP_START_OFFSET;
    if (spi_flash_read(pos, (uint32_t*) &image_header, sizeof(image_header)) != SPI_FLASH_RESULT_OK) {
        return 0;
    }
    pos += sizeof(image_header);
    int segments = image_header.num_segments;
    for (uint32_t section_index = 0; section_index < segments; ++section_index)
    {
        section_header_t &section_header = *(section_header_t *)buf;
        if (spi_flash_read(pos, (uint32_t*) &section_header, sizeof(section_header)) != SPI_FLASH_RESULT_OK) {
            return 0;
        }
        pos += sizeof(section_header);
        pos += section_header.size;
    }
    result = (pos + 16) & ~15;
    return result;
}
#endif

static StreamString builtinOptions;
String& getOptions()
{
    return builtinOptions;
}

void saveOptions(Stream &stream)
{
    DynamicJsonDocument doc(1024);

    if (firmwareOptions.hasUID)
    {
        JsonArray uid = doc.createNestedArray("uid");
        copyArray(firmwareOptions.uid, sizeof(firmwareOptions.uid), uid);
    }
    doc["wifi-on-interval"] = firmwareOptions.wifi_auto_on_interval / 1000;
    if (firmwareOptions.home_wifi_ssid[0])
    {
        doc["wifi-ssid"] = firmwareOptions.home_wifi_ssid;
        doc["wifi-password"] = firmwareOptions.home_wifi_password;
    }
    #if defined(TARGET_UNIFIED_TX)
    doc["tlm-interval"] = firmwareOptions.tlm_report_interval;
    doc["fan-runtime"] = firmwareOptions.fan_min_runtime;
    doc["uart-inverted"] = firmwareOptions.uart_inverted;
    doc["unlock-higher-power"] = firmwareOptions.unlock_higher_power;
    #else
    doc["rcvr-uart-baud"] = firmwareOptions.uart_baud;
    doc["rcvr-invert-tx"] = firmwareOptions.invert_tx;
    doc["lock-on-first-connection"] = firmwareOptions.lock_on_first_connection;
    #endif
    doc["domain"] = firmwareOptions.domain;

    serializeJson(doc, stream);
}

void saveOptions()
{
    File options = SPIFFS.open("/options.json", "w");
    saveOptions(options);
    options.close();
}

bool options_init()
{
    debugCreateInitLogger();

    uint32_t partition_start = 0;
    #if defined(PLATFORM_ESP32)
    SPIFFS.begin(true);
    const esp_partition_t *running = esp_ota_get_running_partition();
    if (running) {
        partition_start = running->address;
    }
    uint32_t location = partition_start + ESP.getSketchSize();
    #else
    SPIFFS.begin();
    uint32_t location = partition_start + myGetSketchSize();
    #endif
    ESP.flashRead(location, buf, 2048);

    bool hardware_inited = hardware_init(buf);

    if (buf[0] != 0xFFFFFFFF)
    {
        strlcpy(product_name, (const char *)buf, sizeof(product_name));
        strlcpy(device_name, (const char *)buf + 128, sizeof(device_name));
    }
    else
    {
        #if defined(TARGET_UNIFIED_RX)
        strcpy(product_name, "Unified RX");
        strcpy(device_name, "Unified RX");
        #else
        strcpy(product_name, "Unified TX");
        strcpy(device_name, "Unified TX");
        #endif
    }

    DynamicJsonDocument doc(1024);
    File file = SPIFFS.open("/options.json", "r");
    if (!file || file.isDirectory())
    {
        if (file)
        {
            file.close();
        }
        // Try JSON at the end of the firmware
        DeserializationError error = deserializeJson(doc, ((const char *)buf) + 16 + 128, strnlen(((const char *)buf) + 16 + 128, 512));
        if (error)
        {
            return false;
        }
    }
    else
    {
        DeserializationError error = deserializeJson(doc, file);
        file.close();
        if (error)
        {
            return false;
        }
    }

    if (doc["uid"].is<JsonArray>())
    {
        copyArray(doc["uid"], firmwareOptions.uid, sizeof(firmwareOptions.uid));
        firmwareOptions.hasUID = true;
    }
    else
    {
        firmwareOptions.hasUID = false;
    }
    int32_t wifiInterval = doc["wifi-on-interval"] | -1;
    firmwareOptions.wifi_auto_on_interval = wifiInterval == -1 ? -1 : wifiInterval * 1000;
    strlcpy(firmwareOptions.home_wifi_ssid, doc["wifi-ssid"] | "", sizeof(firmwareOptions.home_wifi_ssid));
    strlcpy(firmwareOptions.home_wifi_password, doc["wifi-password"] | "", sizeof(firmwareOptions.home_wifi_password));
    #if defined(TARGET_UNIFIED_TX)
    firmwareOptions.tlm_report_interval = doc["tlm-interval"] | 240U;
    firmwareOptions.fan_min_runtime = doc["fan-runtime"] | 30U;
    firmwareOptions.uart_inverted = doc["uart-inverted"] | true;
    firmwareOptions.unlock_higher_power = doc["unlock-higher-power"] | false;
    #else
    firmwareOptions.uart_baud = doc["rcvr-uart-baud"] | 420000;
    firmwareOptions.invert_tx = doc["rcvr-invert-tx"] | false;
    firmwareOptions.lock_on_first_connection = doc["lock-on-first-connection"] | true;
    #endif
    firmwareOptions.domain = doc["domain"] | 0;

    builtinOptions.clear();
    saveOptions(builtinOptions);

    debugFreeInitLogger();

    return hardware_inited;
}


#endif
