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

#if !defined(TARGET_UNIFIED_TX) && !defined(TARGET_UNIFIED_RX)
const char device_name[] = DEVICE_NAME;
const char *product_name = (const char *)(target_name+4);

__attribute__ ((used)) static firmware_options_t flashedOptions = {
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
    #elif defined(Regulatory_Domain_US_433)
    .domain = 6,
    #elif defined(Regulatory_Domain_US_433_WIDE)
    .domain = 7,
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
#if defined(USE_AIRPORT_AT_BAUD)
    .uart_baud = USE_AIRPORT_AT_BAUD,
#elif defined(USE_SBUS_PROTOCOL) || defined(USE_DJI_RS_PRO_PROTOCOL)
    .uart_baud = 100000,
#elif defined(USE_SUMD_PROTOCOL)
    .uart_baud = 115200,
#elif defined(USE_HOTT_TLM_PROTOCOL)
    .uart_baud = 19200,
#elif defined(USE_MAVLINK_PROTOCOL)
    .uart_baud = 460800,
#elif defined(RCVR_UART_BAUD)
    .uart_baud = RCVR_UART_BAUD,
#else
    .uart_baud = 420000,
#endif
    ._unused1 = false,
#if defined(LOCK_ON_FIRST_CONNECTION)
    .lock_on_first_connection = true,
#else
    .lock_on_first_connection = false,
#endif
    ._unused2 = false,
#if defined(USE_AIRPORT_AT_BAUD)
    .is_airport = true,
#else
    .is_airport = false,
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
    ._unused1 = false,
#if defined(UNLOCK_HIGHER_POWER)
    .unlock_higher_power = true,
#else
    .unlock_higher_power = false,
#endif
#if defined(USE_AIRPORT_AT_BAUD)
    .is_airport = true,
#else
    .is_airport = false,
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
#if defined(USE_AIRPORT_AT_BAUD)
    .uart_baud = USE_AIRPORT_AT_BAUD,
#else
    .uart_baud = 0,
#endif
#endif
};

/*
 * This all seems rather convoluted, but it means that the compiler/linker optimisations
 * don't create multiple copies of the UID. This code forces the firmwareOptions to be copied
 * into RAM and all the other areas of code are forced to use the RAM copy.
 */
firmware_options_t firmwareOptions;
bool options_init()
{
    firmwareOptions = flashedOptions;
    return true;
}

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

char product_name[ELRSOPTS_PRODUCTNAME_SIZE+1];
char device_name[ELRSOPTS_DEVICENAME_SIZE+1];
uint32_t logo_image;

// Discriminator value used to determine if the device has been reflashed and therefore
// the SPIFSS settings are obsolete and the flashed settings should be used in preference
uint32_t flash_discriminator;

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
    DynamicJsonDocument doc(1024);

    if (firmwareOptions.hasUID)
    {
        JsonArray uid = doc.createNestedArray("uid");
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
    #if defined(TARGET_UNIFIED_TX)
    doc["tlm-interval"] = firmwareOptions.tlm_report_interval;
    doc["fan-runtime"] = firmwareOptions.fan_min_runtime;
    doc["unlock-higher-power"] = firmwareOptions.unlock_higher_power;
    doc["airport-uart-baud"] = firmwareOptions.uart_baud;
    #else
    doc["rcvr-uart-baud"] = firmwareOptions.uart_baud;
    doc["lock-on-first-connection"] = firmwareOptions.lock_on_first_connection;
    #endif
    doc["is-airport"] = firmwareOptions.is_airport;
    doc["domain"] = firmwareOptions.domain;
    doc["customised"] = customised;
    doc["flash-discriminator"] = flash_discriminator;

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
    DynamicJsonDocument flashDoc(1024);
    DynamicJsonDocument spiffsDoc(1024);
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

    DynamicJsonDocument &doc = flashDoc;
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
    #if defined(TARGET_UNIFIED_TX)
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
    #endif
    firmwareOptions.domain = doc["domain"] | 0;
    flash_discriminator = doc["flash-discriminator"] | 0U;

    builtinOptions.clear();
    saveOptions(builtinOptions, doc["customised"] | false);
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
        #if defined(TARGET_UNIFIED_RX)
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
