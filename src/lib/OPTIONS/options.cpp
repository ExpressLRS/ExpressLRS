#include "targets.h"
#include "options.h"

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

#if defined(UNIT_TEST)
char device_name[] = DEVICE_NAME;
firmware_options_t firmwareOptions;
#else
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

char product_name[ELRSOPTS_PRODUCTNAME_SIZE+1];
char device_name[ELRSOPTS_DEVICENAME_SIZE+1];
uint32_t logo_image;

firmware_options_t firmwareOptions;

// hardware_init prototype here as it is called by options_init()
extern bool hardware_init(EspFlashStream &strmFlash);

static StreamString builtinOptions;
String& getOptions()
{
    return builtinOptions;
}

void saveOptions(Stream &stream, bool customised)
{
    JsonDocument doc;

    if (firmwareOptions.hasUID)
    {
        JsonArray uid = doc["uid"].to<JsonArray>();
        copyArray(firmwareOptions.uid, sizeof(firmwareOptions.uid), uid);
    }
    if (firmwareOptions.wifi_auto_on_interval != -1)
    {
        doc["wifi-on-interval"] = firmwareOptions.wifi_auto_on_interval / 1000;
    }
    if (firmwareOptions.home_wifi_ssid[0])
    {
        doc["wifi-ssid"] = firmwareOptions.home_wifi_ssid;
        doc["wifi-password"] = firmwareOptions.home_wifi_password;
    }
    #if defined(TARGET_TX)
    doc["tlm-interval"] = firmwareOptions.tlm_report_interval;
    doc["fan-runtime"] = firmwareOptions.fan_min_runtime;
    doc["unlock-higher-power"] = firmwareOptions.unlock_higher_power;
    doc["airport-uart-baud"] = firmwareOptions.uart_baud;
    #else
    doc["rcvr-uart-baud"] = firmwareOptions.uart_baud;
    doc["lock-on-first-connection"] = firmwareOptions.lock_on_first_connection;
    doc["dji-permanently-armed"] = firmwareOptions.dji_permanently_armed;
    #endif
    doc["is-airport"] = firmwareOptions.is_airport;
    doc["domain"] = firmwareOptions.domain;
    doc["customised"] = customised;
    doc["flash-discriminator"] = firmwareOptions.flash_discriminator;

    serializeJson(doc, stream);
}

void saveOptions()
{
    File options = SPIFFS.open("/options.json", "w");
    saveOptions(options, true);
    options.close();
}

/**
 * @brief:  Checks if the strmFlash currently is pointing to something that looks like
 *          a string (not all 0xFF). Position in the stream will not be changed.
 * @return: true if appears to have a string
 */
bool options_HasStringInFlash(EspFlashStream &strmFlash)
{
    uint32_t firstBytes;
    size_t pos = strmFlash.getPosition();
    strmFlash.readBytes((uint8_t *)&firstBytes, sizeof(firstBytes));
    strmFlash.setPosition(pos);

    return firstBytes != 0xffffffff;
}

/**
 * @brief:  Internal read options from either the flash stream at the end of the sketch or the options.json file
 *          Fills the firmwareOptions variable
 * @return: true if either was able to be parsed
 */
static void options_LoadFromFlashOrFile(EspFlashStream &strmFlash)
{
    JsonDocument flashDoc;
    JsonDocument spiffsDoc;
    bool hasFlash = false;
    bool hasSpiffs = false;

    // Try OPTIONS JSON at the end of the firmware, after PRODUCTNAME DEVICENAME
    constexpr size_t optionConfigOffset = ELRSOPTS_PRODUCTNAME_SIZE + ELRSOPTS_DEVICENAME_SIZE;
    strmFlash.setPosition(optionConfigOffset);
    if (options_HasStringInFlash(strmFlash))
    {
        DeserializationError error = deserializeJson(flashDoc, strmFlash);
        if (error)
        {
            return;
        }
        hasFlash = true;
    }

    // load options.json from the SPIFFS partition
    File file = SPIFFS.open("/options.json", "r");
    if (file && !file.isDirectory())
    {
        DeserializationError error = deserializeJson(spiffsDoc, file);
        if (!error)
        {
            hasSpiffs = true;
        }
    }

    JsonDocument &doc = flashDoc;
    if (hasFlash && hasSpiffs)
    {
        if (flashDoc["flash-discriminator"] == spiffsDoc["flash-discriminator"])
        {
            doc = spiffsDoc;
        }
    }
    else if (hasSpiffs)
    {
        doc = spiffsDoc;
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
    #if defined(TARGET_TX)
    firmwareOptions.tlm_report_interval = doc["tlm-interval"] | 240U;
    firmwareOptions.fan_min_runtime = doc["fan-runtime"] | 30U;
    firmwareOptions.unlock_higher_power = doc["unlock-higher-power"] | false;
    #if defined(USE_AIRPORT_AT_BAUD)
    firmwareOptions.uart_baud = doc["airport-uart-baud"] | USE_AIRPORT_AT_BAUD;
    firmwareOptions.is_airport = doc["is-airport"] | true;
    #else
    firmwareOptions.uart_baud = doc["airport-uart-baud"] | 460800;
    firmwareOptions.is_airport = doc["is-airport"] | false;
    #endif
    #else
    #if defined(USE_AIRPORT_AT_BAUD)
    firmwareOptions.uart_baud = doc["rcvr-uart-baud"] | USE_AIRPORT_AT_BAUD;
    firmwareOptions.is_airport = doc["is-airport"] | true;
    #else
    firmwareOptions.uart_baud = doc["rcvr-uart-baud"] | 420000;
    firmwareOptions.is_airport = doc["is-airport"] | false;
    #endif
    firmwareOptions.lock_on_first_connection = doc["lock-on-first-connection"] | true;
    firmwareOptions.dji_permanently_armed = doc["dji-permanently-armed"] | false;
    #endif
    firmwareOptions.domain = doc["domain"] | 0;
    firmwareOptions.flash_discriminator = doc["flash-discriminator"] | 0U;

    builtinOptions.clear();
    saveOptions(builtinOptions, doc["customised"] | false);
}

/**
 * @brief: Put a blank options.json into SPIFFS to force all options to the coded defaults in options_LoadFromFlashOrFile()
*/
void options_SetTrueDefaults()
{
    JsonDocument doc;
    // The Regulatory Domain is retained, as there is no sensible default
    doc["domain"] = firmwareOptions.domain;
    doc["flash-discriminator"] = firmwareOptions.flash_discriminator;

    File options = SPIFFS.open("/options.json", "w");
    serializeJson(doc, options);
    options.close();
}

/**
 * @brief:  Initializes product_name / device_name either from flash or static values
 * @return: true if the names came from flash, or false if the values are default
*/
static bool options_LoadProductAndDeviceName(EspFlashStream &strmFlash)
{
    if (options_HasStringInFlash(strmFlash))
    {
        strmFlash.setPosition(0);
        // Product name
        strmFlash.readBytes(product_name, ELRSOPTS_PRODUCTNAME_SIZE);
        product_name[ELRSOPTS_PRODUCTNAME_SIZE] = '\0';
        // Device name
        strmFlash.readBytes(device_name, ELRSOPTS_DEVICENAME_SIZE);
        device_name[ELRSOPTS_DEVICENAME_SIZE] = '\0';

        return true;
    }
    else
    {
        #if defined(TARGET_RX)
        strcpy(product_name, "Unified RX");
        strcpy(device_name, "Unified RX");
        #else
        strcpy(product_name, "Unified TX");
        strcpy(device_name, "Unified TX");
        #endif

        return false;
    }
}

bool options_init()
{
    debugCreateInitLogger();

    uint32_t baseAddr = 0;
#if defined(PLATFORM_ESP32)
    SPIFFS.begin(true);
    const esp_partition_t *runningPart = esp_ota_get_running_partition();
    if (runningPart)
    {
        baseAddr = runningPart->address;
    }
#else
    SPIFFS.begin();
    // ESP8266 sketch baseAddr is always 0
#endif

    EspFlashStream strmFlash;
    strmFlash.setBaseAddress(baseAddr + ESP.getSketchSize());

    // Product / Device Name
    options_LoadProductAndDeviceName(strmFlash);
    // options.json
    options_LoadFromFlashOrFile(strmFlash);
    // hardware.json
    bool hasHardware = hardware_init(strmFlash);
    // flash location of logo image in RGB565 format
    logo_image = baseAddr + ESP.getSketchSize() +
        ELRSOPTS_PRODUCTNAME_SIZE +
        ELRSOPTS_DEVICENAME_SIZE +
        ELRSOPTS_OPTIONS_SIZE +
        ELRSOPTS_HARDWARE_SIZE;

    debugFreeInitLogger();

    return hasHardware;
}
#endif
