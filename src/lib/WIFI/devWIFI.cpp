#include "device.h"

#include "deferred.h"

#include <AsyncJson.h>
#include <ArduinoJson.h>
#if defined(PLATFORM_ESP8266)
#include <FS.h>
#else
#include <SPIFFS.h>
#endif

#if defined(PLATFORM_ESP32)
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

#include "ArduinoJson.h"
#include "AsyncJson.h"
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

extern TxConfig config;
extern void setButtonColors(uint8_t b1, uint8_t b2);
#else
extern RxConfig config;
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
#include "crsf2msp.h"
#include "msp2crsf.h"

#include "tcpsocket.h"
TCPSOCKET wifi2tcp(5761); //port 5761 as used by BF configurator
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

void setWifiUpdateMode()
{
  // No need to ExitBindingMode(), the radio will be stopped stopped when start the Wifi service.
  // Need to change this before the mode change event so the LED is updated
  InBindingMode = false;
  connectionState = wifiUpdate;
}

/** Is this an IP? */
static boolean isIp(String str)
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
static String toStringIp(IPAddress ip)
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
  extern const char *wifi_hostname;

  if (!isIp(request->host()) && request->host() != (String(wifi_hostname) + ".local"))
  {
    DBGLN("Request redirected to captive portal");
    request->redirect(String("http://") + toStringIp(request->client()->localIP()));
    return true;
  }
  return false;
}

static struct {
  const char *url;
  const char *contentType;
  const uint8_t* content;
  const size_t size;
} files[] = {
  {"/scan.js", "text/javascript", (uint8_t *)SCAN_JS, sizeof(SCAN_JS)},
  {"/mui.js", "text/javascript", (uint8_t *)MUI_JS, sizeof(MUI_JS)},
  {"/elrs.css", "text/css", (uint8_t *)ELRS_CSS, sizeof(ELRS_CSS)},
  {"/hardware.html", "text/html", (uint8_t *)HARDWARE_HTML, sizeof(HARDWARE_HTML)},
  {"/hardware.js", "text/javascript", (uint8_t *)HARDWARE_JS, sizeof(HARDWARE_JS)},
  {"/cw.html", "text/html", (uint8_t *)CW_HTML, sizeof(CW_HTML)},
  {"/cw.js", "text/javascript", (uint8_t *)CW_JS, sizeof(CW_JS)},
#if defined(RADIO_LR1121)
  {"/lr1121.html", "text/html", (uint8_t *)LR1121_HTML, sizeof(LR1121_HTML)},
  {"/lr1121.js", "text/javascript", (uint8_t *)LR1121_JS, sizeof(LR1121_JS)},
#endif
};

static void WebUpdateSendContent(AsyncWebServerRequest *request)
{
  for (size_t i=0 ; i<ARRAY_SIZE(files) ; i++) {
    if (request->url().equals(files[i].url)) {
      AsyncWebServerResponse *response = request->beginResponse_P(200, files[i].contentType, files[i].content, files[i].size);
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
  AsyncWebServerResponse *response;
  if (connectionState == hardwareUndefined)
  {
    response = request->beginResponse_P(200, "text/html", (uint8_t*)HARDWARE_HTML, sizeof(HARDWARE_HTML));
  }
  else
  {
    response = request->beginResponse_P(200, "text/html", (uint8_t*)INDEX_HTML, sizeof(INDEX_HTML));
  }
  response->addHeader("Content-Encoding", "gzip");
  response->addHeader("Cache-Control", "no-cache, no-store, must-revalidate");
  response->addHeader("Pragma", "no-cache");
  response->addHeader("Expires", "-1");
  request->send(response);
}

static void putFile(AsyncWebServerRequest *request, uint8_t *data, size_t len, size_t index, size_t total)
{
  static File file;
  static size_t bytes;
  if (!file || request->url() != file.name()) {
    file = SPIFFS.open(request->url(), "w");
    bytes = 0;
  }
  file.write(data, len);
  bytes += len;
  if (bytes == total) {
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
    request->send(SPIFFS, request->url().c_str(), "text/plain", true);
  }
}

static void HandleReboot(AsyncWebServerRequest *request)
{
  AsyncWebServerResponse *response = request->beginResponse(200, "application/json", "Kill -9, no more CPU time!");
  response->addHeader("Connection", "close");
  request->send(response);
  request->client()->close();
  rebootTime = millis() + 100;
}

static void HandleReset(AsyncWebServerRequest *request)
{
  if (request->hasArg("hardware")) {
    SPIFFS.remove("/hardware.json");
  }
  if (request->hasArg("options")) {
    SPIFFS.remove("/options.json");
  }
  if (request->hasArg("model") || request->hasArg("config")) {
    config.SetDefaults(true);
  }
  AsyncWebServerResponse *response = request->beginResponse(200, "application/json", "Reset complete, rebooting...");
  response->addHeader("Connection", "close");
  request->send(response);
  request->client()->close();
  rebootTime = millis() + 100;
}

static void UpdateSettings(AsyncWebServerRequest *request, JsonVariant &json)
{
  if (firmwareOptions.flash_discriminator != json["flash-discriminator"].as<uint32_t>()) {
    request->send(409, "text/plain", "Mismatched device identifier, refresh the page and try again.");
    return;
  }

  File file = SPIFFS.open("/options.json", "w");
  serializeJson(json, file);
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

static void GetConfiguration(AsyncWebServerRequest *request)
{
  bool exportMode = request->hasArg("export");
  AsyncJsonResponse *response = new AsyncJsonResponse();
  JsonObject json = response->getRoot();

  if (!exportMode)
  {
    JsonDocument options;
    deserializeJson(options, getOptions());
    json["options"] = options;
  }

  JsonArray uid = json["config"]["uid"].to<JsonArray>();
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
    if (hardware_int(button == 0 ? HARDWARE_button_led_index : HARDWARE_button2_led_index) != -1) {
      json["config"]["button-actions"][button]["color"] = buttonColor->val.color;
    }
    for (int pos=0 ; pos<button_GetActionCnt() ; pos++)
    {
      json["config"]["button-actions"][button]["action"][pos]["is-long-press"] = buttonColor->val.actions[pos].pressType ? true : false;
      json["config"]["button-actions"][button]["action"][pos]["count"] = buttonColor->val.actions[pos].count;
      json["config"]["button-actions"][button]["action"][pos]["action"] = buttonColor->val.actions[pos].action;
    }
  }
  if (exportMode)
  {
    json["config"]["fan-mode"] = config.GetFanMode();
    json["config"]["power-fan-threshold"] = config.GetPowerFanThreshold();

    json["config"]["motion-mode"] = config.GetMotionMode();

    json["config"]["vtx-admin"]["band"] = config.GetVtxBand();
    json["config"]["vtx-admin"]["channel"] = config.GetVtxChannel();
    json["config"]["vtx-admin"]["pitmode"] = config.GetVtxPitmode();
    json["config"]["vtx-admin"]["power"] = config.GetVtxPower();
    json["config"]["backpack"]["dvr-start-delay"] = config.GetDvrStartDelay();
    json["config"]["backpack"]["dvr-stop-delay"] = config.GetDvrStopDelay();
    json["config"]["backpack"]["dvr-aux-channel"] = config.GetDvrAux();

    for (int model = 0 ; model < CONFIG_TX_MODEL_CNT ; model++)
    {
      const model_config_t &modelConfig = config.GetModelConfig(model);
      String strModel(model);
      JsonObject modelJson = json["config"]["model"][strModel].to<JsonObject>();
      modelJson["packet-rate"] = modelConfig.rate;
      modelJson["telemetry-ratio"] = modelConfig.tlm;
      modelJson["switch-mode"] = modelConfig.switchMode;
      modelJson["power"]["max-power"] = modelConfig.power;
      modelJson["power"]["dynamic-power"] = modelConfig.dynamicPower;
      modelJson["power"]["boost-channel"] = modelConfig.boostChannel;
      modelJson["model-match"] = modelConfig.modelMatch;
      modelJson["tx-antenna"] = modelConfig.txAntenna;
    }
  }
#endif /* TARGET_TX */

  if (!exportMode)
  {
    json["config"]["ssid"] = station_ssid;
    json["config"]["mode"] = wifiMode == WIFI_STA ? "STA" : "AP";
    #if defined(TARGET_RX)
    json["config"]["serial-protocol"] = config.GetSerialProtocol();
#if defined(PLATFORM_ESP32)
    json["config"]["serial1-protocol"] = config.GetSerial1Protocol();
#endif
    json["config"]["sbus-failsafe"] = config.GetFailsafeMode();
    json["config"]["modelid"] = config.GetModelId();
    json["config"]["force-tlm"] = config.GetForceTlmOff();
    json["config"]["vbind"] = config.GetBindStorage();
    for (int ch=0; ch<GPIO_PIN_PWM_OUTPUTS_COUNT; ++ch)
    {
      json["config"]["pwm"][ch]["config"] = config.GetPwmChannel(ch)->raw;
      json["config"]["pwm"][ch]["pin"] = GPIO_PIN_PWM_OUTPUTS[ch];
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
      json["config"]["pwm"][ch]["features"] = features;
    }
    #endif
    json["config"]["product_name"] = product_name;
    json["config"]["lua_name"] = device_name;
    json["config"]["reg_domain"] = FHSSgetRegulatoryDomain();
    json["config"]["uidtype"] = GetConfigUidType(json);
  }

  response->setLength();
  request->send(response);
}

#if defined(TARGET_TX)
static void UpdateConfiguration(AsyncWebServerRequest *request, JsonVariant &json)
{
  if (json.containsKey("button-actions")) {
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
  if (json.containsKey("config"))
  {
    json = json["config"];
  }

  if (json.containsKey("fan-mode")) config.SetFanMode(json["fan-mode"]);
  if (json.containsKey("power-fan-threshold")) config.SetPowerFanThreshold(json["power-fan-threshold"]);
  if (json.containsKey("motion-mode")) config.SetMotionMode(json["motion-mode"]);

  if (json.containsKey("vtx-admin"))
  {
    if (json["vtx-admin"].containsKey("band")) config.SetVtxBand(json["vtx-admin"]["band"]);
    if (json["vtx-admin"].containsKey("channel")) config.SetVtxChannel(json["vtx-admin"]["channel"]);
    if (json["vtx-admin"].containsKey("pitmode")) config.SetVtxPitmode(json["vtx-admin"]["pitmode"]);
    if (json["vtx-admin"].containsKey("power")) config.SetVtxPower(json["vtx-admin"]["power"]);
  }

  if (json.containsKey("backpack"))
  {
    if (json["backpack"].containsKey("dvr-start-delay")) config.SetDvrStartDelay(json["backpack"]["dvr-start-delay"]);
    if (json["backpack"].containsKey("dvr-stop-delay")) config.SetDvrStopDelay(json["backpack"]["dvr-stop-delay"]);
    if (json["backpack"].containsKey("dvr-aux-channel")) config.SetDvrAux(json["backpack"]["dvr-aux-channel"]);
  }

  if (json.containsKey("model"))
  {
    for(JsonPair kv : json["model"].as<JsonObject>())
    {
      uint8_t model = atoi(kv.key().c_str());
      JsonObject modelJson = kv.value();

      config.SetModelId(model);
      if (modelJson.containsKey("packet-rate")) config.SetRate(modelJson["packet-rate"]);
      if (modelJson.containsKey("telemetry-ratio")) config.SetTlm(modelJson["telemetry-ratio"]);
      if (modelJson.containsKey("switch-mode")) config.SetSwitchMode(modelJson["switch-mode"]);
      if (modelJson.containsKey("power"))
      {
        if (modelJson["power"].containsKey("max-power")) config.SetPower(modelJson["power"]["max-power"]);
        if (modelJson["power"].containsKey("dynamic-power")) config.SetDynamicPower(modelJson["power"]["dynamic-power"]);
        if (modelJson["power"].containsKey("boost-channel")) config.SetBoostChannel(modelJson["power"]["boost-channel"]);
      }
      if (modelJson.containsKey("model-match")) config.SetModelMatch(modelJson["model-match"]);
      // if (modelJson.containsKey("tx-antenna")) config.SetTxAntenna(modelJson["tx-antenna"]);
      // have to commmit after each model is updated
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
  JsonArray juid = json["uid"].as<JsonArray>();
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

  long forceTlm = json["force-tlm"] | 0;
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

static void WebUpdateGetTarget(AsyncWebServerRequest *request)
{
  JsonDocument json;
  json["target"] = &target_name[4];
  json["version"] = VERSION;
  json["product_name"] = product_name;
  json["lua_name"] = device_name;
  json["reg_domain"] = FHSSgetRegulatoryDomain();
  json["git-commit"] = commit;
#if defined(TARGET_TX)
  json["module-type"] = "TX";
#endif
#if defined(TARGET_RX)
  json["module-type"] = "RX";
#endif
#if defined(RADIO_SX128X)
  json["radio-type"] = "SX128X";
  json["has-sub-ghz"] = false;
#endif
#if defined(RADIO_SX127X)
  json["radio-type"] = "SX127X";
  json["has-sub-ghz"] = true;
#endif
#if defined(RADIO_LR1121)
  json["radio-type"] = "LR1121";
  json["has-sub-ghz"] = true;
#endif

  AsyncResponseStream *response = request->beginResponseStream("application/json");
  serializeJson(json, *response);
  request->send(response);
}

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
  request->client()->close();
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

  DBGLN("Setting network %s", ssid.c_str());
  strcpy(station_ssid, ssid.c_str());
  strcpy(station_password, password.c_str());
  if (request->hasArg("save")) {
    strlcpy(firmwareOptions.home_wifi_ssid, ssid.c_str(), sizeof(firmwareOptions.home_wifi_ssid));
    strlcpy(firmwareOptions.home_wifi_password, password.c_str(), sizeof(firmwareOptions.home_wifi_password));
    saveOptions();
  }
  WebUpdateConnect(request);
}

static void WebUpdateForget(AsyncWebServerRequest *request)
{
  DBGLN("Forget network");
  firmwareOptions.home_wifi_ssid[0] = 0;
  firmwareOptions.home_wifi_password[0] = 0;
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
    request->client()->close();
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
    request->client()->close();

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

static void initialize()
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
    // windows 11 captive portal workaround
    server.on("/connecttest.txt", [](AsyncWebServerRequest *request) { request->redirect("http://logout.net"); });
    // A 404 stops win 10 keep calling this repeatedly and panicking the esp32
    server.on("/wpad.dat", [](AsyncWebServerRequest *request) { request->send(404); });

    server.on("/generate_204", WebUpdateHandleRoot); // Android
    server.on("/gen_204", WebUpdateHandleRoot); // Android
    server.on("/library/test/success.html", WebUpdateHandleRoot); // apple call home
    server.on("/hotspot-detect.html", WebUpdateHandleRoot); // apple call home
    server.on("/connectivity-check.html", WebUpdateHandleRoot); // ubuntu
    server.on("/check_network_status.txt", WebUpdateHandleRoot); // ubuntu
    server.on("/ncsi.txt", WebUpdateHandleRoot); // windows call home
    server.on("/canonical.html", WebUpdateHandleRoot); // firefox captive portal call home
    server.on("/fwlink", WebUpdateHandleRoot);
    server.on("/redirect", WebUpdateHandleRoot); // microsoft redirect
    server.on("/success.txt", [](AsyncWebServerRequest *request) { request->send(200); }); // firefox captive portal call home
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

  server.on("/", WebUpdateHandleRoot);
  server.on("/elrs.css", WebUpdateSendContent);
  server.on("/mui.js", WebUpdateSendContent);
  server.on("/scan.js", WebUpdateSendContent);
  server.on("/networks.json", WebUpdateSendNetworks);
  server.on("/sethome", WebUpdateSetHome);
  server.on("/forget", WebUpdateForget);
  server.on("/connect", WebUpdateConnect);
  server.on("/config", HTTP_GET, GetConfiguration);
  server.on("/access", WebUpdateAccessPoint);
  server.on("/target", WebUpdateGetTarget);
  server.on("/firmware.bin", WebUpdateGetFirmware);

  server.on("/update", HTTP_POST, WebUploadResponseHandler, WebUploadDataHandler);
  server.on("/update", HTTP_OPTIONS, corsPreflightResponse);
  server.on("/forceupdate", WebUploadForceUpdateHandler);
  server.on("/forceupdate", HTTP_OPTIONS, corsPreflightResponse);
  server.on("/cw.html", WebUpdateSendContent);
  server.on("/cw.js", WebUpdateSendContent);
  server.on("/cw", HandleContinuousWave);

  DefaultHeaders::Instance().addHeader("Access-Control-Allow-Origin", "*");
  DefaultHeaders::Instance().addHeader("Access-Control-Max-Age", "600");
  DefaultHeaders::Instance().addHeader("Access-Control-Allow-Methods", "POST,GET,OPTIONS");
  DefaultHeaders::Instance().addHeader("Access-Control-Allow-Headers", "*");

  server.on("/hardware.html", WebUpdateSendContent);
  server.on("/hardware.js", WebUpdateSendContent);
  server.on("/hardware.json", getFile).onBody(putFile);
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
    server.addHandler(new AsyncCallbackJsonWebHandler("/import", ImportConfiguration, 32768U));
  #endif

  #if defined(RADIO_LR1121)
    server.on("/lr1121.html", WebUpdateSendContent);
    server.on("/lr1121.js", WebUpdateSendContent);
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

void HandleMSP2WIFI()
{
  #if defined(TARGET_RX)
  // check is there is any data to write out
  if (crsf2msp.FIFOout.peekSize() > 0)
  {
    const uint16_t len = crsf2msp.FIFOout.popSize();
    uint8_t data[len];
    crsf2msp.FIFOout.popBytes(data, len);
    wifi2tcp.write(data, len);
  }

  // check if there is any data to read in
  const uint16_t bytesReady = wifi2tcp.bytesReady();
  if (bytesReady > 0)
  {
    uint8_t data[bytesReady];
    wifi2tcp.read(data);
    msp2crsf.parse(data, bytesReady);
  }

  wifi2tcp.handle();
  #endif
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
    HandleMSP2WIFI();
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
  .timeout = timeout
};
