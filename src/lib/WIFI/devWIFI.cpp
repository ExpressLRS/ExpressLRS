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

#include "common.h"
#include "POWERMGNT.h"
#include "FHSS.h"
#include "hwTimer.h"
#include "logging.h"
#include "options.h"
#include "helpers.h"
#include "devButton.h"
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

extern unsigned long rebootTime;

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

static const char VERSION[] = {LATEST_VERSION, 0};

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
  if (!isIp(request->host()) && request->host() != (String(wifi_hostname) + ".local"))
  {
    DBGLN("Request redirected to captive portal");
    request->redirect(String("http://") + toStringIp(request->client()->localIP()));
    return true;
  }
  return false;
}

static void WebUpdateSendContent(AsyncWebServerRequest *request)
{
  for (size_t i=0 ; i<WEB_ASSETS_COUNT ; i++) {
    if (request->url().equals(WEB_ASSETS[i].path)) {
      AsyncWebServerResponse *response = request->beginResponse(200, WEB_ASSETS[i].content_type, WEB_ASSETS[i].data, WEB_ASSETS[i].size);
      response->addHeader("Content-Encoding", "gzip");
      request->send(response);
      return;
    }
  }
  request->send(404, "text/plain", "File not found");
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
    request->redirect("/index.html#hardware");
  }
  else
  {
    request->redirect("/index.html");
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
  AsyncWebServerResponse *response = request->beginResponse(200, "application/json", "Kill -9, no more CPU time!");
  response->addHeader("Connection", "close");
  request->send(response);
  rebootTime = millis() + 100;
}

static void HandleReset(AsyncWebServerRequest *request)
{
  if (request->hasArg("hardware")) {
    LittleFS.remove("/hardware.json");
  }
  if (request->hasArg("options")) {
    LittleFS.remove("/options.json");
#if defined(TARGET_RX)
    config.SetModelId(255);
    config.SetForceTlmOff(false);
    config.Commit();
#endif
  }
  if (request->hasArg("lr1121")) {
    LittleFS.remove("/lr1121.txt");
  }
  if (request->hasArg("model") || request->hasArg("config")) {
    config.SetDefaults(true);
  }
  AsyncWebServerResponse *response = request->beginResponse(200, "application/json", "Reset complete, rebooting...");
  response->addHeader("Connection", "close");
  request->send(response);
  rebootTime = millis() + 100;
}

static void UpdateSettings(AsyncWebServerRequest *request, JsonVariant &json)
{
  if (firmwareOptions.flash_discriminator != json["flash-discriminator"].as<uint32_t>()) {
    request->send(409, "text/plain", "Mismatched device identifier, refresh the page and try again.");
    return;
  }

  File file = LittleFS.open("/options.json", "w");
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
    if (json["options"]["customised"] | false)
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

static void GetConfiguration(AsyncWebServerRequest *request)
{
  const bool exportMode = request->hasArg("export");
  auto *response = new AsyncJsonResponse();
  const auto json = response->getRoot();

  if (!exportMode)
  {
    JsonDocument options;
    deserializeJson(options, getOptions());
    json["options"] = options;
  }

  const auto cfg = json["config"].to<JsonObject>();
  const auto uid = cfg["uid"].to<JsonArray>();
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
    const auto btn = cfg["button-actions"][button].to<JsonObject>();
    if (hardware_int(button == 0 ? HARDWARE_button_led_index : HARDWARE_button2_led_index) != -1) {
      btn["color"] = buttonColor->val.color;
    }
    for (int pos=0 ; pos<button_GetActionCnt() ; pos++)
    {
      const auto action = btn["action"][pos].to<JsonObject>();
      action["is-long-press"] = buttonColor->val.actions[pos].pressType ? true : false;
      action["count"] = buttonColor->val.actions[pos].count;
      action["action"] = buttonColor->val.actions[pos].action;
    }
  }
  if (exportMode)
  {
    cfg["fan-mode"] = config.GetFanMode();
    cfg["power-fan-threshold"] = config.GetPowerFanThreshold();
    cfg["motion-mode"] = config.GetMotionMode();

    const auto vtxAdmin = cfg["vtx-admin"].to<JsonObject>();
    vtxAdmin["band"] = config.GetVtxBand();
    vtxAdmin["channel"] = config.GetVtxChannel();
    vtxAdmin["pitmode"] = config.GetVtxPitmode();
    vtxAdmin["power"] = config.GetVtxPower();

    const auto backpack = cfg["backpack"].to<JsonObject>();
    backpack["disabled"] = config.GetBackpackDisable();
    backpack["dvr-start-delay"] = config.GetDvrStartDelay();
    backpack["dvr-stop-delay"] = config.GetDvrStopDelay();
    backpack["dvr-aux-channel"] = config.GetDvrAux();
    backpack["telemetry-mode"] = config.GetBackpackTlmMode();

    for (int model = 0 ; model < CONFIG_TX_MODEL_CNT ; model++)
    {
      const model_config_t &modelConfig = config.GetModelConfig(model);
      String strModel(model);
      const auto modelJson = cfg["model"][strModel].to<JsonObject>();
      modelJson["packet-rate"] = modelConfig.rate;
      modelJson["telemetry-ratio"] = modelConfig.tlm;
      modelJson["switch-mode"] = modelConfig.switchMode;
      modelJson["link-mode"] = modelConfig.linkMode;
      modelJson["model-match"] = modelConfig.modelMatch;
      modelJson["tx-antenna"] = modelConfig.txAntenna;
      modelJson["ptr-start-chan"] = modelConfig.ptrStartChannel;
      modelJson["ptr-enable-chan"] = modelConfig.ptrEnableChannel;
      const auto power = cfg["power"].to<JsonObject>();
      power["max-power"] = modelConfig.power;
      power["dynamic-power"] = modelConfig.dynamicPower;
      power["boost-channel"] = modelConfig.boostChannel;
    }
  }
#endif /* TARGET_TX */

  if (!exportMode)
  {
    const auto settings = json["settings"].to<JsonObject>();
    #if defined(TARGET_RX)
    cfg["serial-protocol"] = config.GetSerialProtocol();
    #if defined(PLATFORM_ESP32)
    if ((GPIO_PIN_SERIAL1_RX != UNDEF_PIN && GPIO_PIN_SERIAL1_TX != UNDEF_PIN) || GPIO_PIN_PWM_OUTPUTS_COUNT > 0)
    {
      cfg["serial1-protocol"] = config.GetSerial1Protocol();
    }
    #endif
    cfg["sbus-failsafe"] = config.GetFailsafeMode();
    cfg["modelid"] = config.GetModelId();
    cfg["force-tlm"] = config.GetForceTlmOff();
    cfg["vbind"] = config.GetBindStorage();
    for (int ch=0; ch<GPIO_PIN_PWM_OUTPUTS_COUNT; ++ch)
    {
      const auto channel = cfg["pwm"][ch].to<JsonObject>();
      channel["config"] = config.GetPwmChannel(ch)->raw;
      channel["pin"] = GPIO_PIN_PWM_OUTPUTS[ch];
      uint8_t features = 0;
      auto pin = GPIO_PIN_PWM_OUTPUTS[ch];
      if (pin == U0TXD_GPIO_NUM) features |= 1;  // SerialTX supported
      else if (pin == U0RXD_GPIO_NUM) features |= 2;  // SerialRX supported
      else if (pin == GPIO_PIN_SCL) features |= 4;  // I2C SCL supported (only on this pin)
      else if (pin == GPIO_PIN_SDA) features |= 8;  // I2C SCL supported (only on this pin)
      else if (GPIO_PIN_SCL == UNDEF_PIN || GPIO_PIN_SDA == UNDEF_PIN) features |= 12; // Both I2C SCL/SDA supported (on any pin)
      #if defined(PLATFORM_ESP32)
      if (pin != 0) features |= 16; // DShot supported on all pins but GPIO0
      if (pin == GPIO_PIN_SERIAL1_RX) features |= 32;  // SERIAL1 RX supported (only on this pin)
      else if (pin == GPIO_PIN_SERIAL1_TX) features |= 64;  // SERIAL1 TX supported (only on this pin)
      else if ((GPIO_PIN_SERIAL1_RX == UNDEF_PIN || GPIO_PIN_SERIAL1_TX == UNDEF_PIN) &&
               (!(features & 1) && !(features & 2))) features |= 96; // Both Serial1 RX/TX supported (on any pin if not already featured for Serial 1)
      #endif
      channel["features"] = features;
    }
    if (GPIO_PIN_RCSIGNAL_RX != UNDEF_PIN && GPIO_PIN_RCSIGNAL_TX != UNDEF_PIN)
    {
        settings["has_serial_pins"] = true;
    }
    #endif
    settings["product_name"] = product_name;
    settings["lua_name"] = device_name;
    settings["uidtype"] = GetConfigUidType(json);
    settings["ssid"] = station_ssid;
    settings["mode"] = wifiMode == WIFI_STA ? "STA" : "AP";
    settings["wifi_dbm"] = wifi_GetClientRssi();
    settings["custom_hardware"] = hardware_flag(HARDWARE_customised);
    settings["target"] = &target_name[4];
    settings["version"] = VERSION;
    settings["git-commit"] = commit;
#if defined(TARGET_TX)
    settings["module-type"] = "TX";
#endif
#if defined(TARGET_RX)
    settings["module-type"] = "RX";
#endif
#if defined(RADIO_SX128X)
    settings["radio-type"] = "SX128X";
    settings["has_low_band"] = false;
    settings["has_high_band"] = true;
    settings["reg_domain_high"] = FHSSconfig->domain;
#elif defined(RADIO_SX127X)
    settings["radio-type"] = "SX127X";
    settings["has_low_band"] = true;
    settings["has_high_band"] = false;
    settings["reg_domain_low"] = FHSSconfig->domain;
#elif defined(RADIO_LR1121)
    settings["radio-type"] = "LR1121";
    settings["has_low_band"] = POWER_OUTPUT_VALUES_COUNT != 0;
    settings["has_high_band"] = POWER_OUTPUT_VALUES_DUAL_COUNT != 0;
    settings["reg_domain_low"] = FHSSconfig->domain;
    settings["reg_domain_high"] = FHSSconfigDualBand->domain;
#endif
  }

  response->setLength();
  request->send(response);
}

#if defined(TARGET_TX)
static void UpdateConfiguration(AsyncWebServerRequest *request, JsonVariant &json)
{
  if (json["button-actions"].is<JsonVariant>()) {
    const JsonArray &array = json["button-actions"].as<JsonArray>();
    for (size_t button=0 ; button<array.size() ; button++)
    {
      tx_button_color_t action;
      for (int pos=0 ; pos<button_GetActionCnt() ; pos++)
      {
        action.val.actions[pos].pressType = array[button]["action"][pos]["is-long-press"];
        action.val.actions[pos].count = array[button]["action"][pos]["count"];
        action.val.actions[pos].action = array[button]["action"][pos]["action"];
      }
      action.val.color = array[button]["color"];
      config.SetButtonActions(button, &action);
    }
  }
  config.Commit();
  request->send(200, "text/plain", "Import/update complete");
}

static void ImportConfiguration(AsyncWebServerRequest *request, JsonVariant &json)
{
  if (json["config"].is<JsonVariant>())
  {
    json = json["config"];
  }

  if (json["fan-mode"].is<JsonVariant>()) config.SetFanMode(json["fan-mode"]);
  if (json["power-fan-threshold"].is<JsonVariant>()) config.SetPowerFanThreshold(json["power-fan-threshold"]);
  if (json["motion-mode"].is<JsonVariant>()) config.SetMotionMode(json["motion-mode"]);

  if (json["vtx-admin"].is<JsonObject>())
  {
    const auto vtxAdmin = json["vtx-admin"].as<JsonObject>();
    if (vtxAdmin["band"].is<JsonVariant>()) config.SetVtxBand(vtxAdmin["band"]);
    if (vtxAdmin["channel"].is<JsonVariant>()) config.SetVtxChannel(vtxAdmin["channel"]);
    if (vtxAdmin["pitmode"].is<JsonVariant>()) config.SetVtxPitmode(vtxAdmin["pitmode"]);
    if (vtxAdmin["power"].is<JsonVariant>()) config.SetVtxPower(vtxAdmin["power"]);
  }

  if (json["backpack"].is<JsonVariant>())
  {
    const auto backpack = json["backpack"].as<JsonObject>();
    if (backpack["disabled"].is<JsonVariant>()) config.SetBackpackDisable(backpack["disabled"]);
    if (backpack["dvr-start-delay"].is<JsonVariant>()) config.SetDvrStartDelay(backpack["dvr-start-delay"]);
    if (backpack["dvr-stop-delay"].is<JsonVariant>()) config.SetDvrStopDelay(backpack["dvr-stop-delay"]);
    if (backpack["dvr-aux-channel"].is<JsonVariant>()) config.SetDvrAux(backpack["dvr-aux-channel"]);
    if (backpack["telemetry-mode"].is<JsonVariant>()) config.SetBackpackTlmMode(backpack["telemetry-mode"]);
  }

  if (json["model"].is<JsonVariant>())
  {
    for(JsonPair kv : json["model"].as<JsonObject>())
    {
      const uint8_t model = atoi(kv.key().c_str());
      const auto modelJson = kv.value().as<JsonObject>();

      config.SetModelId(model);
      if (modelJson["packet-rate"].is<JsonVariant>()) config.SetRate(modelJson["packet-rate"]);
      if (modelJson["telemetry-ratio"].is<JsonVariant>()) config.SetTlm(modelJson["telemetry-ratio"]);
      if (modelJson["switch-mode"].is<JsonVariant>()) config.SetSwitchMode(modelJson["switch-mode"]);
      if (modelJson["link-mode"].is<JsonVariant>()) config.SetLinkMode(modelJson["link-mode"]);
      if (modelJson["model-match"].is<JsonVariant>()) config.SetModelMatch(modelJson["model-match"]);
      if (modelJson["tx-antenna"].is<JsonVariant>()) config.SetAntennaMode(modelJson["tx-antenna"]);
      if (modelJson["ptr-start-chan"].is<JsonVariant>()) config.SetPTRStartChannel(modelJson["ptr-start-chan"]);
      if (modelJson["ptr-enable-chan"].is<JsonVariant>()) config.SetPTREnableChannel(modelJson["ptr-enable-chan"]);
      if (modelJson["power"].is<JsonVariant>())
      {
        if (modelJson["power"]["max-power"].is<JsonVariant>()) config.SetPower(modelJson["power"]["max-power"]);
        if (modelJson["power"]["dynamic-power"].is<JsonVariant>()) config.SetDynamicPower(modelJson["power"]["dynamic-power"]);
        if (modelJson["power"]["boost-channel"].is<JsonVariant>()) config.SetBoostChannel(modelJson["power"]["boost-channel"]);
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
  const auto juid = json["uid"].as<JsonArray>();
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
  uint8_t protocol = json["serial-protocol"] | 0;
  config.SetSerialProtocol((eSerialProtocol)protocol);

#if defined(PLATFORM_ESP32)
  uint8_t protocol1 = json["serial1-protocol"] | 0;
  config.SetSerial1Protocol((eSerial1Protocol)protocol1);
#endif

  uint8_t failsafe = json["sbus-failsafe"] | 0;
  config.SetFailsafeMode((eFailsafeMode)failsafe);

  long modelid = json["modelid"] | 255;
  if (modelid < 0 || modelid > 63) modelid = 255;
  config.SetModelId((uint8_t)modelid);

  long forceTlm = json["force-tlm"] | false;
  config.SetForceTlmOff(forceTlm != 0);

  config.SetBindStorage((rx_config_bindstorage_t)(json["vbind"] | 0));
  JsonUidToConfig(json);

  JsonArray pwm = json["pwm"].as<JsonArray>();
  for(uint32_t channel = 0 ; channel < pwm.size() ; channel++)
  {
    uint32_t val = pwm[channel];
    //DBGLN("PWMch(%u)=%u", channel, val);
    config.SetPwmChannelRaw(channel, val);
  }

  config.Commit();
  request->send(200, "text/plain", "Configuration updated");
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
    request->send(204, "application/json", "[]");
  }
}

static void sendResponse(AsyncWebServerRequest *request, const String &msg, WiFiMode_t mode) {
  AsyncWebServerResponse *response = request->beginResponse(200, "text/plain", msg);
  response->addHeader("Connection", "close");
  request->send(response);
  changeTime = millis();
  changeMode = mode;
}

static void WebUpdateAccessPoint(AsyncWebServerRequest *request)
{
  DBGLN("Starting Access Point");
  String msg = String("Access Point starting, please connect to access point '") + wifi_ap_ssid + "' with password '" + wifi_ap_password + "'";
  sendResponse(request, msg, WIFI_AP);
}

static void WebUpdateConnect(AsyncWebServerRequest *request)
{
  DBGLN("Connecting to network");
  String msg = String("Connecting to network '") + station_ssid + "', connect to http://" +
    wifi_hostname + ".local from a browser on that network";
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
  String msg = String("Home network forgotten, please connect to access point '") + wifi_ap_ssid + "' with password '" + wifi_ap_password + "'";
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
  message += (request->method() == HTTP_GET) ? "GET" : "POST";
  message += F("\nArguments: ");
  message += request->args();
  message += F("\n");

  for (uint8_t i = 0; i < request->args(); i++)
  {
    message += String(F(" ")) + request->argName(i) + F(": ") + request->arg(i) + F("\n");
  }
  AsyncWebServerResponse *response = request->beginResponse(404, "text/plain", message);
  response->addHeader("Cache-Control", "no-cache, no-store, must-revalidate");
  response->addHeader("Pragma", "no-cache");
  response->addHeader("Expires", "-1");
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
      msg = String("{\"status\": \"ok\", \"msg\": \"Update complete. ");
      #if defined(TARGET_RX)
        msg += "Please wait for the LED to resume blinking before disconnecting power.\"}";
      #else
        msg += "Please wait for a few seconds while the device reboots.\"}";
      #endif
      rebootTime = millis() + 200;
    } else {
      StreamString p = StreamString();
      if (Update.hasError()) {
        Update.printError(p);
      } else {
        p.println("Not enough data uploaded!");
      }
      p.trim();
      DBGLN("Failed to upload firmware: %s", p.c_str());
      msg = String("{\"status\": \"error\", \"msg\": \"") + p + "\"}";
    }
    AsyncWebServerResponse *response = request->beginResponse(200, "application/json", msg);
    response->addHeader("Connection", "close");
    request->send(response);
  } else {
    String message = String("{\"status\": \"mismatch\", \"msg\": \"<b>Current target:</b> ") + (const char *)&target_name[4] + ".<br>";
    if (target_found.length() != 0) {
      message += "<b>Uploaded image:</b> " + target_found + ".<br/>";
    }
    message += "<br/>It looks like you are flashing firmware with a different name to the current  firmware.  This sometimes happens because the hardware was flashed from the factory with an early version that has a different name. Or it may have even changed between major releases.";
    message += "<br/><br/>Please double check you are uploading the correct target, then proceed with 'Flash Anyway'.\"}";
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
    request->send(200, "application/json", "{\"status\": \"ok\", \"msg\": \"Update cancelled\"}");
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
  String filename = String("attachment; filename=\"") + (const char *)&target_name[4] + "_" + VERSION + ".bin\"";
  response->addHeader("Content-Disposition", filename);
  request->send(response);
}

static void HandleContinuousWave(AsyncWebServerRequest *request) {
  if (request->hasArg("radio")) {
    SX12XX_Radio_Number_t radio = request->arg("radio").toInt() == 1 ? SX12XX_Radio_1 : SX12XX_Radio_2;

#if defined(RADIO_LR1121)
    bool setSubGHz = false;
    setSubGHz = request->arg("subGHz").toInt() == 1;
#endif

    AsyncWebServerResponse *response = request->beginResponse(204);
    response->addHeader("Connection", "close");
    request->send(response);

    Radio.TXdoneCallback = [](){};
    Radio.Begin(FHSSgetMinimumFreq(), FHSSgetMaximumFreq());

    POWERMGNT::init();
    POWERMGNT::setPower(POWERMGNT::getMinPower());

#if defined(RADIO_LR1121)
    Radio.startCWTest(setSubGHz ? FHSSconfig->freq_center : FHSSconfigDualBand->freq_center, radio);
#else
    Radio.startCWTest(FHSSconfig->freq_center, radio);
#if defined(RADIO_SX127X)
    deferExecutionMillis(50, [radio](){ Radio.cwRepeat(radio); });
#endif
#endif
  } else {
    int radios = (GPIO_PIN_NSS_2 == UNDEF_PIN) ? 1 : 2;
    request->send(200, "application/json", String("{\"radios\": ") + radios + ", \"center\": "+ FHSSconfig->freq_center +
#if defined(RADIO_LR1121)
            ", \"center2\": "+ FHSSconfigDualBand->freq_center +
#endif
            "}");
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

  String options = "-DAUTO_WIFI_ON_INTERVAL=" + (firmwareOptions.wifi_auto_on_interval == -1 ? "-1" : String(firmwareOptions.wifi_auto_on_interval / 1000));

  #if defined(TARGET_TX)
  if (firmwareOptions.unlock_higher_power)
  {
    options += " -DUNLOCK_HIGHER_POWER";
  }
  options += " -DTLM_REPORT_INTERVAL_MS=" + String(firmwareOptions.tlm_report_interval);
  options += " -DFAN_MIN_RUNTIME=" + String(firmwareOptions.fan_min_runtime);
  #endif

  #if defined(TARGET_RX)
  if (firmwareOptions.lock_on_first_connection)
  {
    options += " -DLOCK_ON_FIRST_CONNECTION";
  }
  options += " -DRCVR_UART_BAUD=" + String(firmwareOptions.uart_baud);
  #endif

  String instance = String(wifi_hostname) + "_" + WiFi.macAddress();
  instance.replace(":", "");
  #if defined(PLATFORM_ESP8266)
    // We have to do it differently on ESP8266 as setInstanceName has the side-effect of chainging the hostname!
    MDNS.setInstanceName(wifi_hostname);
    MDNSResponder::hMDNSService service = MDNS.addService(instance.c_str(), "http", "tcp", 80);
    MDNS.addServiceTxt(service, "vendor", "elrs");
    MDNS.addServiceTxt(service, "target", (const char *)&target_name[4]);
    MDNS.addServiceTxt(service, "device", (const char *)device_name);
    MDNS.addServiceTxt(service, "product", (const char *)product_name);
    MDNS.addServiceTxt(service, "version", VERSION);
    MDNS.addServiceTxt(service, "options", options.c_str());
    MDNS.addServiceTxt(service, "type", "rx");
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
    MDNS.addServiceTxt("http", "tcp", "version", VERSION);
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
    server.on("/connecttest.txt", [](AsyncWebServerRequest *request) {
        request->redirect("http://logout.net");
    });

    // A 404 stops win 10 keep calling this repeatedly and panicking the esp32
    server.on("/wpad.dat", [](AsyncWebServerRequest *request) {
        request->send(404);
    });

    // Firefox captive portal call home
    server.on("/success.txt", [](AsyncWebServerRequest *request) {
        request->send(200);
    });

    // URIs that should redirect to WebUpdateHandleRoot
    const char* rootUris[] = {
        "/",                             // Actual root
        "/generate_204",                 // Android
        "/gen_204",                      // Android
        "/library/test/success.html",    // Apple call home
        "/hotspot-detect.html",          // Apple call home
        "/connectivity-check.html",      // Ubuntu
        "/check_network_status.txt",     // Ubuntu
        "/ncsi.txt",                     // Windows call home
        "/canonical.html",               // Firefox captive portal call home
        "/fwlink",                       // Microsoft
        "/redirect"                      // Microsoft redirect
    };

    for (const char* uri : rootUris)
        server.on(uri, WebUpdateHandleRoot);
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
  server.on("/networks.json", WebUpdateSendNetworks);
  server.on("/sethome", WebUpdateSetHome);
  server.on("/forget", WebUpdateForget);
  server.on("/connect", WebUpdateConnect);
  server.on("/config", HTTP_GET, GetConfiguration);
  server.on("/access", WebUpdateAccessPoint);
  server.on("/firmware.bin", WebUpdateGetFirmware);

  server.on("/update", HTTP_POST, WebUploadResponseHandler, WebUploadDataHandler);
  server.on("/update", HTTP_OPTIONS, corsPreflightResponse);
  server.on("/forceupdate", WebUploadForceUpdateHandler);
  server.on("/forceupdate", HTTP_OPTIONS, corsPreflightResponse);
  server.on("/cw", HandleContinuousWave);

  DefaultHeaders::Instance().addHeader("Access-Control-Allow-Origin", "*");
  DefaultHeaders::Instance().addHeader("Access-Control-Max-Age", "600");
  DefaultHeaders::Instance().addHeader("Access-Control-Allow-Methods", "POST,GET,OPTIONS");
  DefaultHeaders::Instance().addHeader("Access-Control-Allow-Headers", "*");

  server.on("/hardware.json", HTTP_GET | HTTP_POST, getFile, nullptr, putFile);
  server.on("/options.json", HTTP_GET, getFile);
  server.on("/reboot", HandleReboot);
  server.on("/reset", HandleReset);
  #if defined(TARGET_TX) && defined(PLATFORM_ESP32)
    server.on("/udpcontrol", HTTP_POST, WebUdpControl);
  #endif

  server.addHandler(new AsyncCallbackJsonWebHandler("/config", UpdateConfiguration));
  server.addHandler(new AsyncCallbackJsonWebHandler("/options.json", UpdateSettings));
  #if defined(TARGET_TX)
    server.addHandler(new AsyncCallbackJsonWebHandler("/buttons", WebUpdateButtonColors));
    auto *handler = new AsyncCallbackJsonWebHandler("/import", ImportConfiguration);
    handler->setMaxContentLength(32768);
    server.addHandler(handler);
  #endif

  #if defined(RADIO_LR1121)
    server.on("/lr1121", HTTP_OPTIONS, corsPreflightResponse);
    addLR1121Handlers(server);
  #endif

  addCaptivePortalHandlers();

  server.onNotFound(WebUpdateHandleNotFound);

  server.begin();

  dnsServer.start(DNS_PORT, "*", ipAddress);
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
