#include "device.h"

#include "deferred.h"

#include <AsyncJson.h>
#include <ArduinoJson.h>
#include <LittleFS.h>

#if defined(PLATFORM_ESP32)
#include <esp_wifi.h>
#include <WiFi.h>
#include <ESPmDNS.h>
#include <Update.h>
#include <esp_partition.h>
#include <esp_ota_ops.h>
#include <soc/uart_pins.h>
#else
#include <ESP8266WiFi.h>
#include <ESP8266mDNS.h>
#define wifi_mode_t WiFiMode_t
#endif
#include <DNSServer.h>

#include <set>
#include <StreamString.h>

#include <ESPAsyncWebServer.h>

// String literal kept in flash, for APIs that take a const char* and copy it
// (server routes, default headers, mDNS TXT records). On ESP8266 the String
// temporary lives until the end of the full expression, which is all the
// callee needs. On ESP32 literals are already in flash, so pass them through.
#if defined(PLATFORM_ESP8266)
#define FLASH_CSTR(s) String(F(s)).c_str()
#else
#define FLASH_CSTR(s) (s)
#endif

#include "common.h"
#include "rxtx_intf.h"
#include "POWERMGNT.h"
#include "FHSS.h"
#include "hwTimer.h"
#include "logging.h"
#include "options.h"
#include "helpers.h"
#include "devButton.h"
#if defined(TARGET_RX)
#include "devAnalogVbat.h"
#include "VbatCalibration.h"
#endif
#if defined(TARGET_RX) && defined(PLATFORM_ESP32)
#include "devVTXSPI.h"
#endif

#include "WebContent.h"

#include "config.h"

#if defined(RADIO_LR1121)
#include "lr1121.h"
#endif

#if defined(TARGET_TX)
#include "wifiJoystick.h"

extern void setButtonColors(uint8_t b1, uint8_t b2);
#endif

static char station_ssid[33];
static char station_password[65];

static bool wifiStarted = false;
bool webserverPreventAutoStart = false;

static wl_status_t laststatus = WL_IDLE_STATUS;
volatile WiFiMode_t wifiMode = WIFI_OFF;
static volatile WiFiMode_t changeMode = WIFI_OFF;
static volatile unsigned long changeTime = 0;

static const byte DNS_PORT = 53;
static IPAddress netMsk(255, 255, 255, 0);
static DNSServer dnsServer;
static IPAddress ipAddress;

#if defined(TARGET_RX)
#include "TcpMspConnector.h"
TcpMspConnector wifi2tcp;
#endif

#if defined(PLATFORM_ESP8266)
static bool scanComplete = false;
#endif

static AsyncWebServer server(80);
static bool servicesStarted = false;
static constexpr uint32_t STALE_WIFI_SCAN = 20000;
static uint32_t lastScanTimeMS = 0;

static bool target_seen = false;
static uint8_t target_pos = 0;
static String target_found;
static bool target_complete = false;
static bool force_update = false;
static uint32_t totalSize;

static const char VERSION[] PROGMEM = {LATEST_VERSION, 0};

void setWifiUpdateMode()
{
  // No need to ExitBindingMode(), the radio will be stopped stopped when start the Wifi service.
  // Need to change this before the mode change event so the LED is updated
  InBindingMode = false;
  setConnectionState(wifiUpdate);
}

/** Is this an IP? */
static boolean isIp(const String& str)
{
  for (size_t i = 0; i < str.length(); i++)
  {
    int c = str.charAt(i);
    if (c != '.' && (c < '0' || c > '9'))
    {
      return false;
    }
  }
  return true;
}

/** IP to String? */
static String toStringIp(const IPAddress& ip)
{
  String res = "";
  for (int i = 0; i < 3; i++)
  {
    res += String((ip >> (8 * i)) & 0xFF) + ".";
  }
  res += String(((ip >> 8 * 3)) & 0xFF);
  return res;
}

static bool captivePortal(AsyncWebServerRequest *request)
{
  if (!isIp(request->host()) && request->host() != (String(wifi_hostname) + F(".local")))
  {
    DBGLN("Request redirected to captive portal");
    request->redirect(String(F("http://")) + toStringIp(request->client()->localIP()));
    return true;
  }
  return false;
}

static void WebUpdateSendContent(AsyncWebServerRequest *request)
{
  for (size_t i=0 ; i<WEB_ASSETS_COUNT ; i++) {
    if (request->url().equals(WEB_ASSETS[i].path)) {
      AsyncWebServerResponse *response = request->beginResponse(200, WEB_ASSETS[i].content_type, WEB_ASSETS[i].data, WEB_ASSETS[i].size);
      response->addHeader(F("Content-Encoding"), F("gzip"));
      request->send(response);
      return;
    }
  }
  request->send(404, "text/plain", F("File not found"));
}

static void WebUpdateHandleRoot(AsyncWebServerRequest *request)
{
  if (captivePortal(request))
  { // If captive portal redirect instead of displaying the page.
    return;
  }
  force_update = request->hasArg("force");
  if (connectionState == hardwareUndefined)
  {
    request->redirect(F("/index.html#hardware"));
  }
  else
  {
    request->redirect(F("/index.html"));
  }
}

static void putFile(AsyncWebServerRequest *request, uint8_t *data, size_t len, size_t index, size_t total)
{
  static File file;
  static size_t bytes;
  if (!file ||
    // Request URI starts with a / and LittleFS File::name() does not include it, ESP32 doesn't have File::fullName()
    strcmp(&request->url().c_str()[1], file.name()) != 0)
  {
    file = LittleFS.open(request->url(), "w");
    bytes = 0;
  }
  file.write(data, len);
  bytes += len;
  if (bytes == total)
  {
    file.close();
  }
}

static void getFile(AsyncWebServerRequest *request)
{
  if (request->url() == "/options.json") {
    request->send(200, "application/json", getOptions());
  } else if (request->url() == "/hardware.json") {
    request->send(200, "application/json", getHardware());
  } else {
    request->send(LittleFS, request->url().c_str(), "text/plain", true);
  }
}

static void HandleReboot(AsyncWebServerRequest *request)
{
  AsyncWebServerResponse *response = request->beginResponse(200, "application/json", F("Kill -9, no more CPU time!"));
  response->addHeader(F("Connection"), F("close"));
  request->send(response);
  scheduleRebootTime(200);
}

static void HandleReset(AsyncWebServerRequest *request)
{
  if (request->hasArg("hardware")) {
    LittleFS.remove(F("/hardware.json"));
  }
  if (request->hasArg("options")) {
    LittleFS.remove(F("/options.json"));
#if defined(TARGET_RX)
    config.SetModelId(255);
    config.SetForceTlmOff(false);
    config.Commit();
#endif
  }
  if (request->hasArg("lr1121")) {
    LittleFS.remove(F("/lr1121.txt"));
  }
  if (request->hasArg("model") || request->hasArg("config")) {
    config.SetDefaults(true);
  }
  AsyncWebServerResponse *response = request->beginResponse(200, "application/json", F("Reset complete, rebooting..."));
  response->addHeader(F("Connection"), F("close"));
  request->send(response);
  scheduleRebootTime(100);
}

static void UpdateSettings(AsyncWebServerRequest *request, JsonVariant &json)
{
  if (firmwareOptions.flash_discriminator != json[F("flash-discriminator")].as<uint32_t>()) {
    request->send(409, "text/plain", F("Mismatched device identifier, refresh the page and try again."));
    return;
  }

  File file = LittleFS.open(F("/options.json"), "w");
  serializeJson(json, file);
  file.close();
  String options;
  serializeJson(json, options);
  setOptions(options);
  request->send(200);
}

static const char *GetConfigUidType(const JsonObject json)
{
#if defined(TARGET_RX)
  if (config.GetBindStorage() == BINDSTORAGE_VOLATILE)
    return "Volatile";
  if (config.GetBindStorage() == BINDSTORAGE_RETURNABLE && config.IsOnLoan())
    return "Loaned";
  if (config.GetIsBound())
    return "Bound";
  return "Not Bound";
#else
  if (firmwareOptions.hasUID)
  {
    if (json[F("options")][F("customised")] | false)
      return "Overridden";
    else
      return "Flashed";
  }
  return "Not set (using MAC address)";
#endif
}

static int8_t wifi_GetClientRssi()
{
  if (wifiMode == WIFI_STA)
    return WiFi.RSSI();

#if defined(PLATFORM_ESP32)
  // If AP mode, only return an RSSI if there is just one client connected
  // This could take the request's IP address, find it in tcpip_adapter_get_sta_list(), match it by MAC to ap_sta_list,
  // but there should just be one client
  wifi_sta_list_t staList;
  if (esp_wifi_ap_get_sta_list(&staList) == ESP_OK)
  {
    if (staList.num == 1)
      return staList.sta[0].rssi;
  }
#endif
  // ESP8266 doesn't seem to store connected station RSSI :/

  return 0;
}

#if defined(TARGET_RX)
static uint8_t getDefinedVoltageSourceCount()
{
    uint8_t count = 0;
    if (hardware_pin(HARDWARE_vbat) != UNDEF_PIN)
        ++count;
#if defined(PLATFORM_ESP32)
    if (hardware_pin(HARDWARE_vsrc1) != UNDEF_PIN)
        ++count;
    if (hardware_pin(HARDWARE_vsrc2) != UNDEF_PIN)
        ++count;
    if (hardware_pin(HARDWARE_vsrc3) != UNDEF_PIN)
        ++count;
#endif
    return count;
}

static void populateVoltageSampleJson(JsonObject root, const voltage_source_sample_t &sample)
{
  root[F("rawMax")] = sample.rawMax;
  root[F("adcMedian")] = sample.adcMedian;
  root[F("saturated")] = sample.saturated;
  root[F("hasReading")] = sample.hasReading;
}

static void SampleVoltageSources(AsyncWebServerRequest *request, JsonVariant &json)
{
  JsonArray requests = json[F("requests")].as<JsonArray>();
  if (requests.isNull())
  {
    request->send(400, "text/plain", F("Voltage sample batch requests are required"));
    return;
  }

  auto *response = new AsyncJsonResponse();
  JsonObject root = response->getRoot().to<JsonObject>();
  JsonObject samplesRoot = root[F("samples")].to<JsonObject>();

  bool sampledAny = false;
  Vbat_setCalibrationActive(true);
  for (JsonVariant requestItem : requests)
  {
    uint8_t sourceIdx = 0;
    const char *sourceId = requestItem[F("source")] | "";
    if (!VbatCalibration_findSource(sourceId, &sourceIdx) || !VbatCalibration_isSourceDefined(sourceIdx))
      continue;

    voltage_source_config_t source {};
    VbatCalibration_getSourceConfig(sourceIdx, &source);
    int atten = requestItem[F("atten")] | source.atten;
    uint8_t samples = requestItem[F("samples")] | 24;

    voltage_source_sample_t sample {};
    if (!VbatCalibration_sampleSource(sourceIdx, atten, samples, &sample))
      continue;

    JsonObject sampleRoot = samplesRoot[source.id].to<JsonObject>();
    populateVoltageSampleJson(sampleRoot, sample);
    sampledAny = true;
  }
  Vbat_setCalibrationActive(false);

  if (!sampledAny)
  {
    delete response;
    request->send(400, "text/plain", F("No valid voltage sample batch requests"));
    return;
  }

  response->setLength();
  request->send(response);
}
#endif

#if defined(TARGET_RX)
static void GetGpsStatus(AsyncWebServerRequest *request)
{
  auto *response = new AsyncJsonResponse();
  const auto json = response->getRoot();

  gps_telemetry_t gps;
  if (!getGpsTelemetry(gps))
  {
    // No GPS driver is running (the serial protocol was changed without a reboot, say)
    json["present"] = false;
  }
  else
  {
    json["present"] = true;
    json["state"] = gps.state;
    json["baud"] = gps.baud;
    json["can_configure"] = gps.canConfigure;
    json["protocol"] = gps.protocol;
    json["ubx_configured"] = gps.ubxConfigured;
    json["used_valset"] = gps.usedValset;
    json["nav_interval_ms"] = gps.navIntervalMs;
    json["update_interval_ms"] = gps.updateIntervalMs;
    json["satellites"] = gps.satellites;
    json["fix_type"] = gps.fixType;
    json["fix_valid"] = gps.fixValid;
    json["lat"] = gps.lat;
    json["lon"] = gps.lon;
    json["alt_cm"] = gps.altCm;
    json["speed_kmh100"] = gps.speedKmh100;
    json["heading100"] = gps.heading100;
    json["time_valid"] = gps.timeValid;
    json["year"] = gps.year;
    json["month"] = gps.month;
    json["day"] = gps.day;
    json["hour"] = gps.hour;
    json["minute"] = gps.minute;
    json["second"] = gps.second;
    json["age_ms"] = gps.ageMs;
  }

  response->setLength();
  request->send(response);
}
#endif

static void GetConfiguration(AsyncWebServerRequest *request)
{
  const bool exportMode = request->hasArg("export");
  auto *response = new AsyncJsonResponse();
  const auto json = response->getRoot();

  if (!exportMode)
  {
    JsonDocument options;
    deserializeJson(options, getOptions());
    json[F("options")] = options;
  }

  const auto cfg = json[F("config")].to<JsonObject>();
  const auto uid = cfg[F("uid")].to<JsonArray>();
  copyArray(UID, UID_LEN, uid);

#if defined(TARGET_TX)
  int button_count = 0;
  if (GPIO_PIN_BUTTON != UNDEF_PIN)
    button_count = 1;
  if (GPIO_PIN_BUTTON2 != UNDEF_PIN)
    button_count = 2;
  for (int button=0 ; button<button_count ; button++)
  {
    const tx_button_color_t *buttonColor = config.GetButtonActions(button);
    const auto btn = cfg[F("button-actions")][button].to<JsonObject>();
    if (hardware_int(button == 0 ? HARDWARE_button_led_index : HARDWARE_button2_led_index) != -1) {
      btn[F("color")] = buttonColor->val.color;
    }
    for (int pos=0 ; pos<button_GetActionCnt() ; pos++)
    {
      const auto action = btn[F("action")][pos].to<JsonObject>();
      action[F("is-long-press")] = buttonColor->val.actions[pos].pressType ? true : false;
      action[F("count")] = buttonColor->val.actions[pos].count;
      action[F("action")] = buttonColor->val.actions[pos].action;
    }
  }
  if (exportMode)
  {
    cfg[F("fan-mode")] = config.GetFanMode();
    cfg[F("power-fan-threshold")] = config.GetPowerFanThreshold();
    cfg[F("motion-mode")] = config.GetMotionMode();

    const auto vtxAdmin = cfg[F("vtx-admin")].to<JsonObject>();
    vtxAdmin[F("band")] = config.GetVtxBand();
    vtxAdmin[F("channel")] = config.GetVtxChannel();
    vtxAdmin[F("pitmode")] = config.GetVtxPitmode();
    vtxAdmin[F("power")] = config.GetVtxPower();

    const auto backpack = cfg[F("backpack")].to<JsonObject>();
    backpack[F("disabled")] = config.GetBackpackDisable();
    backpack[F("dvr-start-delay")] = config.GetDvrStartDelay();
    backpack[F("dvr-stop-delay")] = config.GetDvrStopDelay();
    backpack[F("dvr-aux-channel")] = config.GetDvrAux();
    backpack[F("telemetry-mode")] = config.GetBackpackTlmMode();

    for (int model = 0 ; model < CONFIG_TX_MODEL_CNT ; model++)
    {
      const model_config_t &modelConfig = config.GetModelConfig(model);
      String strModel(model);
      const auto modelJson = cfg[F("model")][strModel].to<JsonObject>();
      modelJson[F("packet-rate")] = modelConfig.rate;
      modelJson[F("telemetry-ratio")] = modelConfig.tlm;
      modelJson[F("switch-mode")] = modelConfig.switchMode;
      modelJson[F("link-mode")] = modelConfig.linkMode;
      modelJson[F("model-match")] = modelConfig.modelMatch;
      modelJson[F("tx-antenna")] = modelConfig.txAntenna;
      modelJson[F("ptr-start-chan")] = modelConfig.ptrStartChannel;
      modelJson[F("ptr-enable-chan")] = modelConfig.ptrEnableChannel;
      const auto power = cfg[F("power")].to<JsonObject>();
      power[F("max-power")] = modelConfig.power;
      power[F("dynamic-power")] = modelConfig.dynamicPower;
      power[F("boost-channel")] = modelConfig.boostChannel;
    }
  }
#endif /* TARGET_TX */

  if (!exportMode)
  {
    const auto settings = json[F("settings")].to<JsonObject>();
    #if defined(TARGET_RX)
    cfg[F("serial-protocol")] = config.GetSerialProtocol();
    #if defined(PLATFORM_ESP32)
    if ((GPIO_PIN_SERIAL1_RX != UNDEF_PIN && GPIO_PIN_SERIAL1_TX != UNDEF_PIN) || GPIO_PIN_PWM_OUTPUTS_COUNT > 0)
    {
      cfg[F("serial1-protocol")] = config.GetSerial1Protocol();
    }
    #endif
    cfg[F("sbus-failsafe")] = config.GetFailsafeMode();
    cfg[F("modelid")] = config.GetModelId();
    cfg[F("force-tlm")] = config.GetForceTlmOff();
    cfg[F("vbind")] = config.GetBindStorage();
    for (int ch=0; ch<GPIO_PIN_PWM_OUTPUTS_COUNT; ++ch)
    {
      const auto channel = cfg[F("pwm")][ch].to<JsonObject>();
      channel[F("config")] = config.GetPwmChannel(ch)->raw;
      channel[F("pin")] = GPIO_PIN_PWM_OUTPUTS[ch];
      uint8_t features = 0;
      auto pin = GPIO_PIN_PWM_OUTPUTS[ch];
      if (!OPT_PWM_OUT_ONLY)
      {
        if (pin == U0TXD_GPIO_NUM) features |= 1;  // SerialTX supported
        else if (pin == U0RXD_GPIO_NUM) features |= 2;  // SerialRX supported
        else if (pin == GPIO_PIN_SCL) features |= 4;  // I2C SCL supported (only on this pin)
        else if (pin == GPIO_PIN_SDA) features |= 8;  // I2C SDA supported (only on this pin)
        else if (GPIO_PIN_SCL == UNDEF_PIN || GPIO_PIN_SDA == UNDEF_PIN) features |= 12; // Both I2C SCL/SDA supported (on any pin)
      }
      #if defined(PLATFORM_ESP32)
      if (pin != 0) features |= 16; // DShot supported on all pins but GPIO0
      if (!OPT_PWM_OUT_ONLY)
      {
        if (pin == GPIO_PIN_SERIAL1_RX) features |= 32;  // SERIAL1 RX supported (only on this pin)
        else if (pin == GPIO_PIN_SERIAL1_TX) features |= 64;  // SERIAL1 TX supported (only on this pin)
        else if ((GPIO_PIN_SERIAL1_RX == UNDEF_PIN || GPIO_PIN_SERIAL1_TX == UNDEF_PIN) &&
                 (!(features & 1) && !(features & 2))) features |= 96; // Both Serial1 RX/TX supported (on any pin if not already featured for Serial 1)
      }
      #endif
      channel[F("features")] = features;
    }
    if (GPIO_PIN_RCSIGNAL_RX != UNDEF_PIN && GPIO_PIN_RCSIGNAL_TX != UNDEF_PIN)
    {
        settings[F("has_serial_pins")] = true;
    }
    settings["has_gps"] = config.GetSerialProtocol() == PROTOCOL_GPS
    #if defined(PLATFORM_ESP32)
        || config.GetSerial1Protocol() == PROTOCOL_SERIAL1_GPS
    #endif
        ;
    #endif
    settings[F("product_name")] = product_name;
    settings[F("lua_name")] = device_name;
    settings[F("uidtype")] = GetConfigUidType(json);
    settings[F("ssid")] = station_ssid;
    settings[F("mode")] = wifiMode == WIFI_STA ? F("STA") : F("AP");
    settings[F("wifi_dbm")] = wifi_GetClientRssi();
    settings[F("custom_hardware")] = hardware_flag(HARDWARE_customised);
    settings[F("target")] = &target_name[4];
    settings[F("version")] = FPSTR(VERSION);
    settings[F("git-commit")] = commit;
#if defined(TARGET_TX)
    settings[F("module-type")] = F("TX");
#endif
#if defined(TARGET_RX)
    settings[F("module-type")] = F("RX");
    settings[F("voltage_source_count")] = getDefinedVoltageSourceCount();
#endif
#if defined(RADIO_SX127X)
    settings[F("radio-type")] = F("SX127X");
    settings[F("has_low_band")] = true;
    settings[F("has_high_band")] = false;
    settings[F("reg_domain_low")] = FHSSconfig->domain;
#elif defined(RADIO_SX128X)
    settings[F("radio-type")] = F("SX128X");
    settings[F("has_low_band")] = false;
    settings[F("has_high_band")] = true;
    settings[F("reg_domain_high")] = FHSSconfig->domain;
#elif defined(RADIO_LR1121)
    settings[F("radio-type")] = F("LR1121");
    settings[F("has_low_band")] = POWER_OUTPUT_VALUES_COUNT != 0;
    settings[F("has_high_band")] = POWER_OUTPUT_VALUES_DUAL_COUNT != 0;
    settings[F("reg_domain_low")] = FHSSconfig->domain;
    settings[F("reg_domain_high")] = FHSSconfigDualBand->domain;
#elif defined(RADIO_LR2021)
    settings[F("radio-type")] = F("LR2021");
    settings[F("has_low_band")] = POWER_OUTPUT_VALUES_COUNT != 0;
    settings[F("has_high_band")] = POWER_OUTPUT_VALUES_DUAL_COUNT != 0;
    settings[F("reg_domain_low")] = FHSSconfig->domain;
    settings[F("reg_domain_high")] = FHSSconfigDualBand->domain;
#endif
  }

  response->setLength();
  request->send(response);
}

#if defined(TARGET_TX)
static void UpdateConfiguration(AsyncWebServerRequest *request, JsonVariant &json)
{
  if (json[F("button-actions")].is<JsonVariant>()) {
    const JsonArray &array = json[F("button-actions")].as<JsonArray>();
    for (size_t button=0 ; button<array.size() ; button++)
    {
      tx_button_color_t action;
      for (int pos=0 ; pos<button_GetActionCnt() ; pos++)
      {
        action.val.actions[pos].pressType = array[button][F("action")][pos][F("is-long-press")];
        action.val.actions[pos].count = array[button][F("action")][pos][F("count")];
        action.val.actions[pos].action = array[button][F("action")][pos][F("action")];
      }
      action.val.color = array[button][F("color")];
      config.SetButtonActions(button, &action);
    }
  }
  config.Commit();
  request->send(200, "text/plain", F("Import/update complete"));
}

static void ImportConfiguration(AsyncWebServerRequest *request, JsonVariant &json)
{
  if (json[F("config")].is<JsonVariant>())
  {
    json = json[F("config")];
  }

  if (json[F("fan-mode")].is<JsonVariant>()) config.SetFanMode(json[F("fan-mode")]);
  if (json[F("power-fan-threshold")].is<JsonVariant>()) config.SetPowerFanThreshold(json[F("power-fan-threshold")]);
  if (json[F("motion-mode")].is<JsonVariant>()) config.SetMotionMode(json[F("motion-mode")]);

  if (json[F("vtx-admin")].is<JsonObject>())
  {
    const auto vtxAdmin = json[F("vtx-admin")].as<JsonObject>();
    if (vtxAdmin[F("band")].is<JsonVariant>()) config.SetVtxBand(vtxAdmin[F("band")]);
    if (vtxAdmin[F("channel")].is<JsonVariant>()) config.SetVtxChannel(vtxAdmin[F("channel")]);
    if (vtxAdmin[F("pitmode")].is<JsonVariant>()) config.SetVtxPitmode(vtxAdmin[F("pitmode")]);
    if (vtxAdmin[F("power")].is<JsonVariant>()) config.SetVtxPower(vtxAdmin[F("power")]);
  }

  if (json[F("backpack")].is<JsonVariant>())
  {
    const auto backpack = json[F("backpack")].as<JsonObject>();
    if (backpack[F("disabled")].is<JsonVariant>()) config.SetBackpackDisable(backpack[F("disabled")]);
    if (backpack[F("dvr-start-delay")].is<JsonVariant>()) config.SetDvrStartDelay(backpack[F("dvr-start-delay")]);
    if (backpack[F("dvr-stop-delay")].is<JsonVariant>()) config.SetDvrStopDelay(backpack[F("dvr-stop-delay")]);
    if (backpack[F("dvr-aux-channel")].is<JsonVariant>()) config.SetDvrAux(backpack[F("dvr-aux-channel")]);
    if (backpack[F("telemetry-mode")].is<JsonVariant>()) config.SetBackpackTlmMode(backpack[F("telemetry-mode")]);
  }

  if (json[F("model")].is<JsonVariant>())
  {
    for(JsonPair kv : json[F("model")].as<JsonObject>())
    {
      const uint8_t model = atoi(kv.key().c_str());
      const auto modelJson = kv.value().as<JsonObject>();

      config.SetModelId(model);
      if (modelJson[F("packet-rate")].is<JsonVariant>()) config.SetRate(modelJson[F("packet-rate")]);
      if (modelJson[F("telemetry-ratio")].is<JsonVariant>()) config.SetTlm(modelJson[F("telemetry-ratio")]);
      if (modelJson[F("switch-mode")].is<JsonVariant>()) config.SetSwitchMode(modelJson[F("switch-mode")]);
      if (modelJson[F("link-mode")].is<JsonVariant>()) config.SetLinkMode(modelJson[F("link-mode")]);
      if (modelJson[F("model-match")].is<JsonVariant>()) config.SetModelMatch(modelJson[F("model-match")]);
      if (modelJson[F("tx-antenna")].is<JsonVariant>()) config.SetAntennaMode(modelJson[F("tx-antenna")]);
      if (modelJson[F("ptr-start-chan")].is<JsonVariant>()) config.SetPTRStartChannel(modelJson[F("ptr-start-chan")]);
      if (modelJson[F("ptr-enable-chan")].is<JsonVariant>()) config.SetPTREnableChannel(modelJson[F("ptr-enable-chan")]);
      if (modelJson[F("power")].is<JsonVariant>())
      {
        if (modelJson[F("power")][F("max-power")].is<JsonVariant>()) config.SetPower(modelJson[F("power")][F("max-power")]);
        if (modelJson[F("power")][F("dynamic-power")].is<JsonVariant>()) config.SetDynamicPower(modelJson[F("power")][F("dynamic-power")]);
        if (modelJson[F("power")][F("boost-channel")].is<JsonVariant>()) config.SetBoostChannel(modelJson[F("power")][F("boost-channel")]);
      }
      // have to commit after each model is updated
      config.Commit();
    }
  }

  UpdateConfiguration(request, json);
}

static void WebUpdateButtonColors(AsyncWebServerRequest *request, JsonVariant &json)
{
  int button1Color = json[0].as<int>();
  int button2Color = json[1].as<int>();
  DBGLN("%d %d", button1Color, button2Color);
  setButtonColors(button1Color, button2Color);
  request->send(200);
}
#else
/**
 * @brief: Copy uid to config if changed
*/
static void JsonUidToConfig(JsonVariant &json)
{
  const auto juid = json[F("uid")].as<JsonArray>();
  size_t juidLen = constrain(juid.size(), 0, UID_LEN);
  uint8_t newUid[UID_LEN] = { 0 };

  // Copy only as many bytes as were included, right-justified
  // This supports 6-digit UID as well as 4-digit (OTA bound) UID
  copyArray(juid, &newUid[UID_LEN-juidLen], juidLen);

  if (memcmp(newUid, config.GetUID(), UID_LEN) != 0)
  {
    config.SetUID(newUid);
    config.Commit();
    // Also copy it to the global UID in case the page is reloaded
    memcpy(UID, newUid, UID_LEN);
  }
}
static void UpdateConfiguration(AsyncWebServerRequest *request, JsonVariant &json)
{
  uint8_t protocol = json[F("serial-protocol")] | 0;
  config.SetSerialProtocol((eSerialProtocol)protocol);

#if defined(PLATFORM_ESP32)
  uint8_t protocol1 = json[F("serial1-protocol")] | 0;
  config.SetSerial1Protocol((eSerial1Protocol)protocol1);
#endif

  uint8_t failsafe = json[F("sbus-failsafe")] | 0;
  config.SetFailsafeMode((eFailsafeMode)failsafe);

  long modelid = json[F("modelid")] | 255;
  if (modelid < 0 || modelid > 63) modelid = 255;
  config.SetModelId((uint8_t)modelid);

  long forceTlm = json[F("force-tlm")] | false;
  config.SetForceTlmOff(forceTlm != 0);

  config.SetBindStorage((rx_config_bindstorage_t)(json[F("vbind")] | 0));
  JsonUidToConfig(json);

  JsonArray pwm = json[F("pwm")].as<JsonArray>();
  for(uint32_t channel = 0 ; channel < pwm.size() ; channel++)
  {
    rx_config_pwm_t pwmChannel;
    pwmChannel.raw = pwm[channel];
    if (OPT_PWM_OUT_ONLY &&
        (pwmChannel.val.mode == somSerial || pwmChannel.val.mode == somSCL || pwmChannel.val.mode == somSDA ||
         pwmChannel.val.mode == somSerial1RX || pwmChannel.val.mode == somSerial1TX))
    {
      pwmChannel.val.mode = som50Hz;
    }
    //DBGLN("PWMch(%u)=%u", channel, val);
    config.SetPwmChannelRaw(channel, pwmChannel.raw);
  }

  config.Commit();
  request->send(200, "text/plain", F("Configuration updated"));
}
#endif

static void WebUpdateSendNetworks(AsyncWebServerRequest *request)
{
  int numNetworks = WiFi.scanComplete();
  if (numNetworks >= 0 && millis() - lastScanTimeMS < STALE_WIFI_SCAN) {
    DBGLN("Found %d networks", numNetworks);
    std::set<String> vs;
    String s="[";
    for(int i=0 ; i<numNetworks ; i++) {
      String w = WiFi.SSID(i);
      DBGLN("found %s", w.c_str());
      if (vs.find(w)==vs.end() && w.length()>0) {
        if (!vs.empty()) s += ",";
        s += "\"" + w + "\"";
        vs.insert(w);
      }
    }
    s+="]";
    request->send(200, "application/json", s);
  } else {
    if (WiFi.scanComplete() != WIFI_SCAN_RUNNING)
    {
      #if defined(PLATFORM_ESP8266)
      scanComplete = false;
      WiFi.scanNetworksAsync([](int){
        scanComplete = true;
      });
      #else
      WiFi.scanNetworks(true);
      #endif
      lastScanTimeMS = millis();
    }
    request->send(204, "application/json", F("[]"));
  }
}

static void sendResponse(AsyncWebServerRequest *request, const String &msg, WiFiMode_t mode) {
  AsyncWebServerResponse *response = request->beginResponse(200, "text/plain", msg);
  response->addHeader(F("Connection"), F("close"));
  request->send(response);
  changeTime = millis();
  changeMode = mode;
}

static void WebUpdateAccessPoint(AsyncWebServerRequest *request)
{
  DBGLN("Starting Access Point");
  String msg = String(F("Access Point starting, please connect to access point '")) + wifi_ap_ssid + F("' with password '") + wifi_ap_password + F("'");
  sendResponse(request, msg, WIFI_AP);
}

static void WebUpdateConnect(AsyncWebServerRequest *request)
{
  DBGLN("Connecting to network");
  String msg = String(F("Connecting to network '")) + station_ssid + F("', connect to http://") +
    wifi_hostname + F(".local from a browser on that network");
  sendResponse(request, msg, WIFI_STA);
}

static void WebUpdateSetHome(AsyncWebServerRequest *request)
{
  String ssid = request->arg("network");
  String password = request->arg("password");
  String onInterval = request->arg("wifi-on-interval");

  DBGLN("Setting network %s", ssid.c_str());
  strcpy(station_ssid, ssid.c_str());
  strcpy(station_password, password.c_str());
  if (request->hasArg("save")) {
    strlcpy(firmwareOptions.home_wifi_ssid, ssid.c_str(), sizeof(firmwareOptions.home_wifi_ssid));
    strlcpy(firmwareOptions.home_wifi_password, password.c_str(), sizeof(firmwareOptions.home_wifi_password));
    firmwareOptions.wifi_auto_on_interval = (onInterval.isEmpty() ? -1 : onInterval.toInt()) * 1000;
    saveOptions();
  }
  WebUpdateConnect(request);
}

static void WebUpdateForget(AsyncWebServerRequest *request)
{
  DBGLN("Forget network");
  String onInterval = request->arg("wifi-on-interval");
  firmwareOptions.home_wifi_ssid[0] = 0;
  firmwareOptions.home_wifi_password[0] = 0;
  firmwareOptions.wifi_auto_on_interval = (onInterval.isEmpty() ? -1 : onInterval.toInt()) * 1000;
  saveOptions();
  station_ssid[0] = 0;
  station_password[0] = 0;
  String msg = String(F("Home network forgotten, please connect to access point '")) + wifi_ap_ssid + F("' with password '") + wifi_ap_password + F("'");
  sendResponse(request, msg, WIFI_AP);
}

static void WebUpdateHandleNotFound(AsyncWebServerRequest *request)
{
  if (captivePortal(request))
  { // If captive portal redirect instead of displaying the error page.
    return;
  }
  String message = F("File Not Found\n\n");
  message += F("URI: ");
  message += request->url();
  message += F("\nMethod: ");
  message += (request->method() == HTTP_GET) ? F("GET") : F("POST");
  message += F("\nArguments: ");
  message += request->args();
  message += F("\n");

  for (uint8_t i = 0; i < request->args(); i++)
  {
    message += String(F(" ")) + request->argName(i) + F(": ") + request->arg(i) + F("\n");
  }
  AsyncWebServerResponse *response = request->beginResponse(404, "text/plain", message);
  response->addHeader(F("Cache-Control"), F("no-cache, no-store, must-revalidate"));
  response->addHeader(F("Pragma"), F("no-cache"));
  response->addHeader(F("Expires"), F("-1"));
  request->send(response);
}

static void corsPreflightResponse(AsyncWebServerRequest *request) {
  AsyncWebServerResponse *response = request->beginResponse(204, "text/plain");
  request->send(response);
}

static void WebUploadResponseHandler(AsyncWebServerRequest *request) {
  if (target_seen || Update.hasError()) {
    String msg;
    if (!Update.hasError() && Update.end()) {
      DBGLN("Update complete, rebooting");
      msg = String(F("{\"status\": \"ok\", \"msg\": \"Update complete. "));
      #if defined(TARGET_RX)
        msg += F("Please wait for the LED to resume blinking before disconnecting power.\"}");
      #else
        msg += F("Please wait for a few seconds while the device reboots.\"}");
      #endif
      scheduleRebootTime(200);
    } else {
      StreamString p = StreamString();
      if (Update.hasError()) {
        Update.printError(p);
      } else {
        p.println(F("Not enough data uploaded!"));
      }
      p.trim();
      DBGLN("Failed to upload firmware: %s", p.c_str());
      msg = String(F("{\"status\": \"error\", \"msg\": \"")) + p + F("\"}");
    }
    AsyncWebServerResponse *response = request->beginResponse(200, "application/json", msg);
    response->addHeader(F("Connection"), F("close"));
    request->send(response);
  } else {
    String message = String(F("{\"status\": \"mismatch\", \"msg\": \"<b>Current target:</b> ")) + (const char *)&target_name[4] + F(".<br>");
    if (target_found.length() != 0) {
      message += String(F("<b>Uploaded image:</b> ")) + target_found + F(".<br/>");
    }
    message += F("<br/>It looks like you are flashing firmware with a different name to the current  firmware.  This sometimes happens because the hardware was flashed from the factory with an early version that has a different name. Or it may have even changed between major releases.");
    message += F("<br/><br/>Please double check you are uploading the correct target, then proceed with 'Flash Anyway'.\"}");
    request->send(200, "application/json", message);
  }
}

static void WebUploadDataHandler(AsyncWebServerRequest *request, const String& filename, size_t index, uint8_t *data, size_t len, bool final) {
  force_update = force_update || request->hasArg("force");
  if (index == 0) {
    #if defined(TARGET_TX) && defined(PLATFORM_ESP32)
      WifiJoystick::StopJoystickService();
    #endif

    size_t filesize = request->header("X-FileSize").toInt();
    DBGLN("Update: '%s' size %u", filename.c_str(), filesize);
    #if defined(PLATFORM_ESP8266)
    Update.runAsync(true);
    uint32_t maxSketchSpace = (ESP.getFreeSketchSpace() - 0x1000) & 0xFFFFF000;
    DBGLN("Free space = %u", maxSketchSpace);
    UNUSED(maxSketchSpace); // for warning
    #endif
    if (!Update.begin(filesize, U_FLASH)) { // pass the size provided
      Update.printError(LOGGING_UART);
    }
    target_seen = false;
    target_found.clear();
    target_complete = false;
    target_pos = 0;
    totalSize = 0;
  }
  if (len) {
    DBGVLN("writing %d", len);
    if (Update.write(data, len) == len) {
      if (force_update || (totalSize == 0 && *data == 0x1F))
        target_seen = true;
      if (!target_seen) {
        for (size_t i=0 ; i<len ;i++) {
          if (!target_complete && (target_pos >= 4 || target_found.length() > 0)) {
            if (target_pos == 4) {
              target_found.clear();
            }
            if (data[i] == 0 || target_found.length() > 50) {
              target_complete = true;
            }
            else {
              target_found += (char)data[i];
            }
          }
          if (data[i] == target_name[target_pos]) {
            ++target_pos;
            if (target_pos >= target_name_size) {
              target_seen = true;
            }
          }
          else {
            target_pos = 0; // Startover
          }
        }
      }
      totalSize += len;
    } else {
      DBGLN("write failed to write %d", len);
    }
  }
}

static void WebUploadForceUpdateHandler(AsyncWebServerRequest *request) {
  target_seen = true;
  if (request->arg("action").equals("confirm")) {
    WebUploadResponseHandler(request);
  } else {
    #if defined(PLATFORM_ESP32)
      Update.abort();
    #endif
    request->send(200, "application/json", F("{\"status\": \"ok\", \"msg\": \"Update cancelled\"}"));
  }
}

#if defined(TARGET_TX) && defined(PLATFORM_ESP32)
static void WebUdpControl(AsyncWebServerRequest *request)
{
  const String &action = request->arg("action");
  if (action.equals("joystick_begin"))
  {
    WifiJoystick::StartSending(request->client()->remoteIP(),
      request->arg("interval").toInt(), request->arg("channels").toInt());
    request->send(200, "text/plain", "ok");
  }
  else if (action.equals("joystick_end"))
  {
    WifiJoystick::StopSending();
    request->send(200, "text/plain", "ok");
  }
}
#endif

static size_t firmwareOffset = 0;
static size_t getFirmwareChunk(uint8_t *data, size_t len, size_t pos)
{
  uint8_t *dst;
  uint8_t alignedBuffer[7];
  if ((uintptr_t)data % 4 != 0)
  {
    // If data is not aligned, read aligned byes using the local buffer and hope the next call will be aligned
    dst = (uint8_t *)((uint32_t)alignedBuffer / 4 * 4);
    len = 4;
  }
  else
  {
    // Otherwise just make sure len is a multiple of 4 and smaller than a sector
    dst = data;
    len = constrain((len / 4) * 4, 4, SPI_FLASH_SEC_SIZE);
  }

  ESP.flashRead(firmwareOffset + pos, (uint32_t *)dst, len);

  // If using local stack buffer, move the 4 bytes into the passed buffer
  // data is known to not be aligned so it is moved byte-by-byte instead of as uint32_t*
  if ((void *)dst != (void *)data)
  {
    for (unsigned b=len; b>0; --b)
      *data++ = *dst++;
  }
  return len;
}

static void WebUpdateGetFirmware(AsyncWebServerRequest *request) {
  #if defined(PLATFORM_ESP32)
  const esp_partition_t *running = esp_ota_get_running_partition();
  if (running) {
      firmwareOffset = running->address;
  }
  #endif
  const size_t firmwareTrailerSize = 4096;  // max number of bytes for the options/hardware layout json
  AsyncWebServerResponse *response = request->beginResponse("application/octet-stream", (size_t)ESP.getSketchSize() + firmwareTrailerSize, &getFirmwareChunk);
  String filename = String(F("attachment; filename=\"")) + (const char *)&target_name[4] + F("_") + FPSTR(VERSION) + F(".bin\"");
  response->addHeader(F("Content-Disposition"), filename);
  request->send(response);
}

static void HandleContinuousWave(AsyncWebServerRequest *request) {
  if (request->hasArg("radio")) {
    SX12XX_Radio_Number_t radio = request->arg("radio").toInt() == 1 ? SX12XX_Radio_1 : SX12XX_Radio_2;

#if defined(RADIO_LR1121) || defined(RADIO_LR2021)
    bool setSubGHz = false;
    setSubGHz = request->arg("subGHz").toInt() == 1;
#endif

    AsyncWebServerResponse *response = request->beginResponse(204);
    response->addHeader(F("Connection"), F("close"));
    request->send(response);

    Radio.TXdoneCallback = [](){};
#if defined(RADIO_SX127X)
    Radio.Begin();
#elif defined(RADIO_SX128X)
    Radio.Begin();
#elif defined(RADIO_LR1121)
    Radio.Begin(FHSSgetMinimumFreq(), FHSSgetMaximumFreq());
#elif defined(RADIO_LR2021)
    Radio.Begin(FHSSconfig->freq_center, FHSSconfigDualBand->freq_center);
#endif

    POWERMGNT::init();
    POWERMGNT::setPower(POWERMGNT::getMinPower());

#if defined(RADIO_LR1121) || defined(RADIO_LR2021)
    Radio.startCWTest(setSubGHz ? FHSSconfig->freq_center : FHSSconfigDualBand->freq_center, radio);
#else
    Radio.startCWTest(FHSSconfig->freq_center, radio);
#if defined(RADIO_SX127X)
    deferExecutionMillis(50, [radio](){ Radio.cwRepeat(radio); });
#endif
#endif
  } else {
    int radios = (GPIO_PIN_NSS_2 == UNDEF_PIN) ? 1 : 2;
    request->send(200, "application/json", String(F("{\"radios\": ")) + radios + F(", \"center\": ") + FHSSconfig->freq_center +
#if defined(RADIO_LR1121) || defined(RADIO_LR2021)
            F(", \"center2\": ") + FHSSconfigDualBand->freq_center +
#endif
            F("}"));
  }
}

static bool initialize()
{
  wifiStarted = false;
  WiFi.disconnect(true);
  WiFi.mode(WIFI_OFF);
  #if defined(PLATFORM_ESP8266)
  WiFi.forceSleepBegin();
  #endif
  registerButtonFunction(ACTION_START_WIFI, [](){
    setWifiUpdateMode();
  });
  return true;
}

static void startWiFi(unsigned long now)
{
  if (wifiStarted) {
    return;
  }

  if (connectionState < FAILURE_STATES) {
    hwTimer::stop();
#if defined(TARGET_RX) && defined(PLATFORM_ESP32)
    disableVTxSpi();
#endif

    // Set transmit power to minimum
    POWERMGNT::setPower(MinPower);

    setWifiUpdateMode();

    DBGLN("Stopping Radio");
    Radio.End();
  }

  DBGLN("Begin Webupdater");

  WiFi.persistent(false);
  WiFi.disconnect();
  WiFi.mode(WIFI_OFF);
  strcpy(station_ssid, firmwareOptions.home_wifi_ssid);
  strcpy(station_password, firmwareOptions.home_wifi_password);
  if (station_ssid[0] == 0) {
    changeTime = now;
    changeMode = WIFI_AP;
  }
  else {
    changeTime = now;
    changeMode = WIFI_STA;
  }
  laststatus = WL_DISCONNECTED;
  wifiStarted = true;
}

static void startMDNS()
{
  if (!MDNS.begin(wifi_hostname))
  {
    DBGLN("Error starting mDNS");
    return;
  }

  String options = F("-DAUTO_WIFI_ON_INTERVAL=");
  options += firmwareOptions.wifi_auto_on_interval == -1 ? String(F("-1")) : String(firmwareOptions.wifi_auto_on_interval / 1000);

  #if defined(TARGET_TX)
  if (firmwareOptions.unlock_higher_power)
  {
    options += F(" -DUNLOCK_HIGHER_POWER");
  }
  options += String(F(" -DTLM_REPORT_INTERVAL_MS=")) + firmwareOptions.tlm_report_interval;
  options += String(F(" -DFAN_MIN_RUNTIME=")) + firmwareOptions.fan_min_runtime;
  #endif

  #if defined(TARGET_RX)
  if (firmwareOptions.lock_on_first_connection)
  {
    options += F(" -DLOCK_ON_FIRST_CONNECTION");
  }
  options += String(F(" -DRCVR_UART_BAUD=")) + firmwareOptions.uart_baud;
  #endif

  String instance = String(wifi_hostname) + "_" + WiFi.macAddress();
  instance.replace(":", "");
  #if defined(PLATFORM_ESP8266)
    // We have to do it differently on ESP8266 as setInstanceName has the side-effect of chainging the hostname!
    MDNS.setInstanceName(wifi_hostname);
    MDNSResponder::hMDNSService service = MDNS.addService(instance.c_str(), FLASH_CSTR("http"), FLASH_CSTR("tcp"), 80);
    MDNS.addServiceTxt(service, FLASH_CSTR("vendor"), FLASH_CSTR("elrs"));
    MDNS.addServiceTxt(service, FLASH_CSTR("target"), (const char *)&target_name[4]);
    MDNS.addServiceTxt(service, FLASH_CSTR("device"), (const char *)device_name);
    MDNS.addServiceTxt(service, FLASH_CSTR("product"), (const char *)product_name);
    MDNS.addServiceTxt(service, FLASH_CSTR("version"), String(FPSTR(VERSION)).c_str());
    MDNS.addServiceTxt(service, FLASH_CSTR("options"), options.c_str());
    MDNS.addServiceTxt(service, FLASH_CSTR("type"), FLASH_CSTR("rx"));
    // If the probe result fails because there is another device on the network with the same name
    // use our unique instance name as the hostname. A better way to do this would be to use
    // MDNSResponder::indexDomain and change wifi_hostname as well.
    MDNS.setHostProbeResultCallback([instance](const char* p_pcDomainName, bool p_bProbeResult) {
      if (!p_bProbeResult) {
        WiFi.hostname(instance);
        MDNS.setInstanceName(instance);
      }
    });
  #else
    MDNS.setInstanceName(instance);
    MDNS.addService("http", "tcp", 80);
    MDNS.addServiceTxt("http", "tcp", "vendor", "elrs");
    MDNS.addServiceTxt("http", "tcp", "target", (const char *)&target_name[4]);
    MDNS.addServiceTxt("http", "tcp", "device", (const char *)device_name);
    MDNS.addServiceTxt("http", "tcp", "product", (const char *)product_name);
    MDNS.addServiceTxt("http", "tcp", "version", String(FPSTR(VERSION)).c_str());
    MDNS.addServiceTxt("http", "tcp", "options", options.c_str());
  #if defined(TARGET_TX)
    MDNS.addServiceTxt("http", "tcp", "type", "tx");
  #else
    MDNS.addServiceTxt("http", "tcp", "type", "rx");
  #endif
  #endif

  #if defined(TARGET_TX) && defined(PLATFORM_ESP32)
    MDNS.addService("elrs", "udp", JOYSTICK_PORT);
    MDNS.addServiceTxt("elrs", "udp", "device", (const char *)device_name);
    MDNS.addServiceTxt("elrs", "udp", "version", String(JOYSTICK_VERSION).c_str());
  #endif
}

static void addCaptivePortalHandlers()
{
    // Windows 11 captive portal workaround
    server.on(FLASH_CSTR("/connecttest.txt"), [](AsyncWebServerRequest *request) {
        request->redirect(F("http://logout.net"));
    });

    // A 404 stops win 10 keep calling this repeatedly and panicking the esp32
    server.on(FLASH_CSTR("/wpad.dat"), [](AsyncWebServerRequest *request) {
        request->send(404);
    });

    // Firefox captive portal call home
    server.on(FLASH_CSTR("/success.txt"), [](AsyncWebServerRequest *request) {
        request->send(200);
    });

    // URIs that should redirect to WebUpdateHandleRoot
    const __FlashStringHelper *rootUris[] = {
        F("/"),                             // Actual root
        F("/generate_204"),                 // Android
        F("/gen_204"),                      // Android
        F("/library/test/success.html"),    // Apple call home
        F("/hotspot-detect.html"),          // Apple call home
        F("/connectivity-check.html"),      // Ubuntu
        F("/check_network_status.txt"),     // Ubuntu
        F("/ncsi.txt"),                     // Windows call home
        F("/canonical.html"),               // Firefox captive portal call home
        F("/fwlink"),                       // Microsoft
        F("/redirect")                      // Microsoft redirect
    };

    for (const __FlashStringHelper *uri : rootUris)
        server.on(String(uri).c_str(), WebUpdateHandleRoot);
}

static void startServices()
{
  if (servicesStarted) {
    #if defined(PLATFORM_ESP32)
      MDNS.end();
      startMDNS();
    #endif
    return;
  }

  for (auto asset : WEB_ASSETS)
  {
      server.on(asset.path, WebUpdateSendContent);
  }
  server.on(FLASH_CSTR("/networks.json"), WebUpdateSendNetworks);
  server.on(FLASH_CSTR("/sethome"), WebUpdateSetHome);
  server.on(FLASH_CSTR("/forget"), WebUpdateForget);
  server.on(FLASH_CSTR("/connect"), WebUpdateConnect);
  server.on(FLASH_CSTR("/config"), HTTP_GET, GetConfiguration);
  server.on(FLASH_CSTR("/access"), WebUpdateAccessPoint);
  server.on(FLASH_CSTR("/firmware.bin"), WebUpdateGetFirmware);

  server.on(FLASH_CSTR("/update"), HTTP_POST, WebUploadResponseHandler, WebUploadDataHandler);
  server.on(FLASH_CSTR("/update"), HTTP_OPTIONS, corsPreflightResponse);
  server.on(FLASH_CSTR("/forceupdate"), WebUploadForceUpdateHandler);
  server.on(FLASH_CSTR("/forceupdate"), HTTP_OPTIONS, corsPreflightResponse);
  server.on(FLASH_CSTR("/cw"), HandleContinuousWave);

  DefaultHeaders::Instance().addHeader(F("Access-Control-Allow-Origin"), F("*"));
  DefaultHeaders::Instance().addHeader(F("Access-Control-Max-Age"), F("600"));
  DefaultHeaders::Instance().addHeader(F("Access-Control-Allow-Methods"), F("POST,GET,OPTIONS"));
  DefaultHeaders::Instance().addHeader(F("Access-Control-Allow-Headers"), F("*"));

  server.on(FLASH_CSTR("/hardware.json"), HTTP_GET | HTTP_POST, getFile, nullptr, putFile);
  server.on(FLASH_CSTR("/options.json"), HTTP_GET, getFile);
  server.on(FLASH_CSTR("/reboot"), HandleReboot);
  server.on(FLASH_CSTR("/reset"), HandleReset);
  #if defined(TARGET_RX)
    server.on(FLASH_CSTR("/gps"), HTTP_GET, GetGpsStatus);
  #endif
  #if defined(TARGET_TX) && defined(PLATFORM_ESP32)
    server.on(FLASH_CSTR("/udpcontrol"), HTTP_POST, WebUdpControl);
  #endif

  server.addHandler(new AsyncCallbackJsonWebHandler(F("/config"), UpdateConfiguration));
  server.addHandler(new AsyncCallbackJsonWebHandler(F("/options.json"), UpdateSettings));
  #if defined(TARGET_RX)
    server.addHandler(new AsyncCallbackJsonWebHandler(F("/voltage-sample"), SampleVoltageSources));
  #endif
  #if defined(TARGET_TX)
    server.addHandler(new AsyncCallbackJsonWebHandler(F("/buttons"), WebUpdateButtonColors));
    auto *handler = new AsyncCallbackJsonWebHandler(F("/import"), ImportConfiguration);
    handler->setMaxContentLength(32768);
    server.addHandler(handler);
  #endif

  #if defined(RADIO_LR1121)
    server.on(FLASH_CSTR("/lr1121"), HTTP_OPTIONS, corsPreflightResponse);
    addLR1121Handlers(server);
  #endif

  addCaptivePortalHandlers();

  server.onNotFound(WebUpdateHandleNotFound);

  server.begin();

  dnsServer.start(DNS_PORT, F("*"), ipAddress);
  dnsServer.setErrorReplyCode(DNSReplyCode::NoError);

  startMDNS();

  #if defined(TARGET_TX) && defined(PLATFORM_ESP32)
    WifiJoystick::StartJoystickService();
  #endif

  servicesStarted = true;
  DBGLN("HTTPUpdateServer ready! Open http://%s.local in your browser", wifi_hostname);
  #if defined(TARGET_RX)
  wifi2tcp.begin();
  #endif
}

static void HandleWebUpdate()
{
  unsigned long now = millis();
  wl_status_t status = WiFi.status();

  if (status != laststatus && wifiMode == WIFI_STA) {
    DBGLN("WiFi status %d", status);
    switch(status) {
      case WL_NO_SSID_AVAIL:
      case WL_CONNECT_FAILED:
      case WL_CONNECTION_LOST:
        changeTime = now;
        changeMode = WIFI_AP;
        break;
      case WL_DISCONNECTED: // try reconnection
        changeTime = now;
        break;
      default:
        break;
    }
    laststatus = status;
  }
  if (status != WL_CONNECTED && wifiMode == WIFI_STA && (now - changeTime) > 30000) {
    changeTime = now;
    changeMode = WIFI_AP;
    DBGLN("Connection failed %d", status);
  }
  if (changeMode != wifiMode && changeMode != WIFI_OFF && (now - changeTime) > 500) {
    switch(changeMode) {
      case WIFI_AP:
        DBGLN("Changing to AP mode");
        WiFi.disconnect();
        wifiMode = WIFI_AP;
        #if defined(PLATFORM_ESP32)
        WiFi.setHostname(wifi_hostname); // hostname must be set before the mode is set to STA
        #endif
        WiFi.mode(wifiMode);
        #if defined(PLATFORM_ESP8266)
        WiFi.setHostname(wifi_hostname); // hostname must be set before the mode is set to STA
        #endif
        changeTime = now;
        #if defined(PLATFORM_ESP8266)
        WiFi.setOutputPower(13.5);
        WiFi.setPhyMode(WIFI_PHY_MODE_11N);
        #elif defined(PLATFORM_ESP32)
        WiFi.setTxPower(WIFI_POWER_19_5dBm);
        #endif
        WiFi.softAPConfig(ipAddress, ipAddress, netMsk);
        WiFi.softAP(wifi_ap_ssid, wifi_ap_password);
        startServices();
        break;
      case WIFI_STA:
        DBGLN("Connecting to network '%s'", station_ssid);
        wifiMode = WIFI_STA;
        #if defined(PLATFORM_ESP32)
        WiFi.setHostname(wifi_hostname); // hostname must be set before the mode is set to STA
        #endif
        WiFi.mode(wifiMode);
        #if defined(PLATFORM_ESP8266)
        WiFi.setHostname(wifi_hostname); // hostname must be set after the mode is set to STA
        #endif
        changeTime = now;
        #if defined(PLATFORM_ESP8266)
        WiFi.setOutputPower(13.5);
        WiFi.setPhyMode(WIFI_PHY_MODE_11N);
        #elif defined(PLATFORM_ESP32)
        WiFi.setTxPower(WIFI_POWER_19_5dBm);
        WiFi.setSortMethod(WIFI_CONNECT_AP_BY_SIGNAL);
        WiFi.setScanMethod(WIFI_ALL_CHANNEL_SCAN);
        #endif
        WiFi.begin(station_ssid, station_password);
        startServices();
      default:
        break;
    }
    #if defined(PLATFORM_ESP8266)
      MDNS.notifyAPChange();
    #endif
    changeMode = WIFI_OFF;
  }

  #if defined(PLATFORM_ESP8266)
  if (scanComplete)
  {
    WiFi.mode(wifiMode);
    scanComplete = false;
  }
  #endif

  if (servicesStarted)
  {
    dnsServer.processNextRequest();
    #if defined(PLATFORM_ESP8266)
      MDNS.update();
    #endif

    #if defined(TARGET_TX) && defined(PLATFORM_ESP32)
      WifiJoystick::Loop(now);
    #endif
  }
}

static int start()
{
  ipAddress.fromString(wifi_ap_address);
  return firmwareOptions.wifi_auto_on_interval;
}

static int event()
{
  if (connectionState == wifiUpdate || connectionState > FAILURE_STATES)
  {
    if (!wifiStarted) {
      startWiFi(millis());
      return DURATION_IMMEDIATELY;
    }
  }
  else if (wifiStarted)
  {
    wifiStarted = false;
    WiFi.disconnect(true);
    WiFi.mode(WIFI_OFF);
    #if defined(PLATFORM_ESP8266)
    WiFi.forceSleepBegin();
    #endif
  }
  return DURATION_IGNORE;
}

static int timeout()
{
  if (wifiStarted)
  {
    HandleWebUpdate();
#if defined(PLATFORM_ESP8266)
    // When in STA mode, a small delay reduces power use from 90mA to 30mA when idle
    // In AP mode, it doesn't seem to make a measurable difference, but does not hurt
    // Only done on 8266 as the ESP32 runs a throttled task
    if (!Update.isRunning())
      delay(1);
    return DURATION_IMMEDIATELY;
#else
    // All the web traffic is async apart from changing modes and MSP2WIFI
    // No need to run balls-to-the-wall; the wifi runs on this core too (0)
    return 2;
#endif
  }

  #if defined(TARGET_TX)
  // if webupdate was requested before or .wifi_auto_on_interval has elapsed but uart is not detected
  // start webupdate, there might be wrong configuration flashed.
  if(firmwareOptions.wifi_auto_on_interval != -1 && webserverPreventAutoStart == false && connectionState < wifiUpdate && !wifiStarted){
    DBGLN("No CRSF ever detected, starting WiFi");
    setWifiUpdateMode();
    return DURATION_IMMEDIATELY;
  }
  #elif defined(TARGET_RX)
  if (firmwareOptions.wifi_auto_on_interval != -1 && !webserverPreventAutoStart && (connectionState == disconnected))
  {
    static bool pastAutoInterval = false;
    // If InBindingMode then wait at least 60 seconds before going into wifi,
    // regardless of if .wifi_auto_on_interval is set to less
    if (!InBindingMode || firmwareOptions.wifi_auto_on_interval >= 60000 || pastAutoInterval)
    {
      setWifiUpdateMode();
      return DURATION_IMMEDIATELY;
    }
    pastAutoInterval = true;
    return (60000 - firmwareOptions.wifi_auto_on_interval);
  }
  #endif
  return DURATION_NEVER;
}

device_t WIFI_device = {
  .initialize = initialize,
  .start = start,
  .event = event,
  .timeout = timeout,
  .subscribe = EVENT_CONNECTION_CHANGED
};
