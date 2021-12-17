#include "device.h"

#if defined(PLATFORM_ESP8266) || defined(PLATFORM_ESP32)

#if defined(PLATFORM_ESP32)
#include <WiFi.h>
#include <ESPmDNS.h>
#include <Update.h>
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
#include "hwTimer.h"
#include "logging.h"
#include "options.h"
#include "helpers.h"

#include "WebContent.h"

#if defined(Regulatory_Domain_AU_915) || defined(Regulatory_Domain_EU_868) || defined(Regulatory_Domain_IN_866) || defined(Regulatory_Domain_FCC_915) || defined(Regulatory_Domain_AU_433) || defined(Regulatory_Domain_EU_433)
#include "SX127xDriver.h"
extern SX127xDriver Radio;
#endif

#if defined(Regulatory_Domain_ISM_2400)
#include "SX1280Driver.h"
extern SX1280Driver Radio;
#endif

#include "config.h"
#if defined(TARGET_TX)
extern TxConfig config;
#else
extern RxConfig config;
#endif
extern unsigned long rebootTime;

static bool wifiStarted = false;
bool webserverPreventAutoStart = false;
extern bool InBindingMode;

static wl_status_t laststatus = WL_IDLE_STATUS;
static volatile WiFiMode_t wifiMode = WIFI_OFF;
static volatile WiFiMode_t changeMode = WIFI_OFF;
static volatile unsigned long changeTime = 0;

static const byte DNS_PORT = 53;
static IPAddress netMsk(255, 255, 255, 0);
static DNSServer dnsServer;
static IPAddress ipAddress;

static AsyncWebServer server(80);
AsyncWebSocket ws("/ws"); // access at ws://[esp ip]/ws
AsyncEventSource events("/events"); // event source (Server-Sent events)
static bool servicesStarted = false;
static bool wsConnected = false;

static bool target_seen = false;
static uint8_t target_pos = 0;
static String target_found;
static bool target_complete = false;
static bool force_update = false;
static uint32_t totalSize;

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

static void WebUpdateSendCSS(AsyncWebServerRequest *request)
{
  AsyncWebServerResponse *response = request->beginResponse_P(200, "text/css", (uint8_t*)CSS, sizeof(CSS));
  response->addHeader("Content-Encoding", "gzip");
  request->send(response);
}

static void WebUpdateSendJS(AsyncWebServerRequest *request)
{
  AsyncWebServerResponse *response = request->beginResponse_P(200, "text/javascript", (uint8_t*)SCAN_JS, sizeof(SCAN_JS));
  response->addHeader("Content-Encoding", "gzip");
  request->send(response);
}

static void WebUpdateSendConsoleJS(AsyncWebServerRequest *request)
{
  AsyncWebServerResponse *response = request->beginResponse_P(200, "text/javascript", (uint8_t*)CONSOLE_JS, sizeof(CONSOLE_JS));
  response->addHeader("Content-Encoding", "gzip");
  request->send(response);
}

static void WebUpdateSendFlag(AsyncWebServerRequest *request)
{
  AsyncWebServerResponse *response = request->beginResponse_P(200, "image/svg+xml", (uint8_t*)FLAG, sizeof(FLAG));
  response->addHeader("Content-Encoding", "gzip");
  request->send(response);
}

static void WebUpdateHandleRoot(AsyncWebServerRequest *request)
{
  if (captivePortal(request))
  { // If captive portal redirect instead of displaying the page.
    return;
  }
  force_update = request->hasArg("force");
  AsyncWebServerResponse *response = request->beginResponse_P(200, "text/html", (uint8_t*)INDEX_HTML, sizeof(INDEX_HTML));
  response->addHeader("Cache-Control", "no-cache, no-store, must-revalidate");
  response->addHeader("Pragma", "no-cache");
  response->addHeader("Expires", "-1");
  response->addHeader("Content-Encoding", "gzip");
  request->send(response);
}

void WSnotifyAll(const char *msg, int len)
{
  if (wsConnected)
  {
    ws.textAll(msg, len);
  }
}

void WSonEvent(AsyncWebSocket *server, AsyncWebSocketClient *client, AwsEventType type, void *arg, uint8_t *data, size_t len)
{
  if (type == WS_EVT_CONNECT)
  {
    client->ping(); // client connected
    DBGLN("ping!: ws[%s][%u]\n", server->url(), client->id());
  }
  else if (type == WS_EVT_DISCONNECT)
  {
    wsConnected = false;
  }
  else if (type == WS_EVT_ERROR)
  {
    // error was received from the other end
    DBGLN("ws[%s][%u] error(%u): %s\n", server->url(), client->id(), *((uint16_t *)arg), (char *)data);
  }
  else if (type == WS_EVT_PONG)
  {
    wsConnected = true;
    // pong message was received (in response to a ping request maybe)
    Serial.printf("ws[%s][%u] pong[%u]: %s\n", server->url(), client->id(), len, (len) ? (char *)data : "");
    DBGLN("WS Client Connected");
  }
}

#if defined(GPIO_PIN_PWM_OUTPUTS)
constexpr uint8_t SERVO_PINS[] = GPIO_PIN_PWM_OUTPUTS;
constexpr uint8_t SERVO_COUNT = ARRAY_SIZE(SERVO_PINS);

static String WebGetPwmStr()
{
  // Output is raw integers, the Javascript side needs to parse it
  // ,"pwm":[49664,50688,51200] = 3 channels, 0=512, 1=512, 2=0
  String pwmStr(",\"pwm\":[");
  for (uint8_t ch=0; ch<SERVO_COUNT; ++ch)
  {
    if (ch > 0)
      pwmStr.concat(',');
    pwmStr.concat(config.GetPwmChannel(ch)->raw);
  }
  pwmStr.concat(']');

  return pwmStr;
}

static void WebUpdatePwm(AsyncWebServerRequest *request)
{
  String pwmStr = request->arg("pwm");
  if (pwmStr.isEmpty())
  {
    request->send(400, "text/plain", "Empty pwm parameter");
    return;
  }

  // parse out the integers representing the PWM values
  // strtok will modify the string as it parses
  char *token = strtok((char *)pwmStr.c_str(), ",");
  uint8_t channel = 0;
  while (token != nullptr && channel < SERVO_COUNT)
  {
    uint16_t val = atoi(token);
    DBGLN("PWMch(%u)=%u", channel, val);
    config.SetPwmChannelRaw(channel, val);
    ++channel;
    token = strtok(nullptr, ",");
  }
  config.Commit();
  request->send(200, "text/plain", "PWM outputs updated");
}
#endif

static void WebUpdateSendMode(AsyncWebServerRequest *request)
{
  String s = String("{\"ssid\":\"") + config.GetSSID() + "\",\"mode\":\"";
  if (wifiMode == WIFI_STA) {
    s += "STA\"";
  } else {
    s += "AP\"";
  }
  #if defined(TARGET_RX)
  s += ",\"modelid\":" + String(config.GetModelId());
  #endif
  #if defined(GPIO_PIN_PWM_OUTPUTS)
  s += WebGetPwmStr();
  #endif
  s += "}";
  request->send(200, "application/json", s);
}

static void WebUpdateGetTarget(AsyncWebServerRequest *request)
{
  String s = String("{\"target\":\"") + (const char *)&target_name[4] + "\",\"version\": \"" + VERSION + "\"}";
  request->send(200, "application/json", s);
}

static void WebUpdateSendNetworks(AsyncWebServerRequest *request)
{
  int numNetworks = WiFi.scanComplete();
  if (numNetworks >= 0) {
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
  DBGLN("Connecting to home network");
  String msg = String("Connecting to network '") + config.GetSSID() + "', connect to http://" +
    wifi_hostname + ".local from a browser on that network";
  sendResponse(request, msg, WIFI_STA);
}

static void WebUpdateSetHome(AsyncWebServerRequest *request)
{
  String ssid = request->arg("network");
  String password = request->arg("password");

  DBGLN("Setting home network %s", ssid.c_str());
  config.SetSSID(ssid.c_str());
  config.SetPassword(password.c_str());
  config.Commit();
  WebUpdateConnect(request);
}

static void WebUpdateForget(AsyncWebServerRequest *request)
{
  DBGLN("Forget home network");
  config.SetSSID("");
  config.SetPassword("");
  config.Commit();
  String msg = String("Home network forgotten, please connect to access point '") + wifi_ap_ssid + "' with password '" + wifi_ap_password + "'";
  sendResponse(request, msg, WIFI_AP);
}

#if defined(TARGET_RX)
static void WebUpdateModelId(AsyncWebServerRequest *request)
{
  long modelid = request->arg("modelid").toInt();
  if (modelid < 0 || modelid > 63) modelid = 255;
  DBGLN("Setting model match id %u", (uint8_t)modelid);
  config.SetModelId((uint8_t)modelid);
  config.Commit();

  AsyncWebServerResponse *response = request->beginResponse(200, "text/plain", "Model Match updated, rebooting receiver");
  response->addHeader("Connection", "close");
  request->send(response);
  request->client()->close();
  rebootTime = millis() + 100;
}
#endif

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

static void WebUploadResponseHandler(AsyncWebServerRequest *request) {
  if (Update.hasError()) {
    StreamString p = StreamString();
    Update.printError(p);
    p.trim();
    DBGLN("Failed to upload firmware: %s", p.c_str());
    AsyncWebServerResponse *response = request->beginResponse(200, "application/json", String("{\"status\": \"error\", \"msg\": \"") + p + "\"}");
    response->addHeader("Connection", "close");
    request->send(response);
    request->client()->close();
  } else {
    if (target_seen) {
      DBGLN("Update complete, rebooting");
      AsyncWebServerResponse *response = request->beginResponse(200, "application/json", "{\"status\": \"ok\", \"msg\": \"Update complete. Please wait for LED to resume blinking before disconnecting power.\"}");
      response->addHeader("Connection", "close");
      request->send(response);
      request->client()->close();
      rebootTime = millis() + 200;
    } else {
      String message = String("{\"status\": \"mismatch\", \"msg\": \"<b>Current target:</b> ") + (const char *)&target_name[4] + ".<br>";
      if (target_found.length() != 0) {
        message += "<b>Uploaded image:</b> " + target_found + ".<br/>";
      }
      message += "<br/>Flashing the wrong firmware may lock or damage your device.\"}";
      request->send(200, "application/json", message);
    }
  }
}

static void WebUploadDataHandler(AsyncWebServerRequest *request, const String& filename, size_t index, uint8_t *data, size_t len, bool final) {
  if (index == 0) {
    DBGLN("Update: %s", filename.c_str());
    #if defined(PLATFORM_ESP8266)
    Update.runAsync(true);
    uint32_t maxSketchSpace = (ESP.getFreeSketchSpace() - 0x1000) & 0xFFFFF000;
    DBGLN("Free space = %u", maxSketchSpace);
    if (!Update.begin(maxSketchSpace, U_FLASH)){//start with max available size
    #else
    if (!Update.begin()) { //start with max available size
    #endif
      Update.printError(Serial);
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
    }
  }
  if (final && !Update.getError()) {
    DBGVLN("finish");
    if (target_seen) {
      if (Update.end(true)) { //true to set the size to the current progress
        DBGLN("Upload Success: %ubytes\nPlease wait for LED to resume blinking before disconnecting power", totalSize);
      } else {
        Update.printError(Serial);
      }
    }
  }
}

static void WebUploadForceUpdateHandler(AsyncWebServerRequest *request) {
  target_seen = true;
  if (request->arg("action").equals("confirm")) {
    if (Update.end(true)) { //true to set the size to the current progress
      DBGLN("Upload Success: %ubytes\nPlease wait for LED to turn resume blinking before disconnecting power", totalSize);
    } else {
      Update.printError(Serial);
    }
    WebUploadResponseHandler(request);
  } else {
    #if defined(PLATFORM_ESP32)
      Update.abort();
    #endif
    request->send(200, "application/json", "{\"status\": \"ok\", \"msg\": \"Update cancelled\"}");
  }
}

static void wifiOff()
{
  wifiStarted = false;
  WiFi.disconnect(true);
  WiFi.mode(WIFI_OFF);
  #if defined(PLATFORM_ESP8266)
  WiFi.forceSleepBegin();
  #endif
}

static void startWiFi(unsigned long now)
{
  if (wifiStarted) {
    return;
  }
  hwTimer::stop();
  // Set transmit power to minimum
  POWERMGNT::setPower(MinPower);
  if (connectionState < FAILURE_STATES) {
    connectionState = wifiUpdate;
  }

  DBGLN("Stopping Radio");
  Radio.End();

  DBGLN("Begin Webupdater");

  WiFi.persistent(false);
  WiFi.disconnect();
  WiFi.mode(WIFI_OFF);
  #if defined(PLATFORM_ESP8266)
    WiFi.setOutputPower(13);
    WiFi.setPhyMode(WIFI_PHY_MODE_11N);
  #elif defined(PLATFORM_ESP32)
    WiFi.setTxPower(WIFI_POWER_13dBm);
  #endif
  if (config.GetSSID()[0]==0 && home_wifi_ssid[0]!=0) {
    config.SetSSID(home_wifi_ssid);
    config.SetPassword(home_wifi_password);
    config.Commit();
  }
  if (config.GetSSID()[0]==0) {
    changeTime = now;
    changeMode = WIFI_AP;
  } else {
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

  String instance = String(wifi_hostname) + "_" + WiFi.macAddress();
  instance.replace(":", "");
  #ifdef PLATFORM_ESP8266
    // We have to do it differently on ESP8266 as setInstanceName has the side-effect of chainging the hostname!
    MDNS.setInstanceName(wifi_hostname);
    MDNSResponder::hMDNSService service = MDNS.addService(instance.c_str(), "http", "tcp", 80);
    MDNS.addServiceTxt(service, "vendor", "elrs");
    MDNS.addServiceTxt(service, "target", (const char *)&target_name[4]);
    MDNS.addServiceTxt(service, "version", VERSION);
    MDNS.addServiceTxt(service, "options", String(FPSTR(compile_options)).c_str());
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
    MDNS.addServiceTxt("http", "tcp", "device", device_name);
    MDNS.addServiceTxt("http", "tcp", "version", VERSION);
    MDNS.addServiceTxt("http", "tcp", "options", String(FPSTR(compile_options)).c_str());
    MDNS.addServiceTxt("http", "tcp", "type", "tx");
  #endif
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
  server.on("/main.css", WebUpdateSendCSS);
  server.on("/scan.js", WebUpdateSendJS);
  server.on("/console.js", WebUpdateSendConsoleJS);
  server.on("/logo.svg", WebUpdateSendFlag);
  server.on("/mode.json", WebUpdateSendMode);
  server.on("/networks.json", WebUpdateSendNetworks);
  server.on("/sethome", WebUpdateSetHome);
  server.on("/forget", WebUpdateForget);
  server.on("/connect", WebUpdateConnect);
  server.on("/access", WebUpdateAccessPoint);
  server.on("/target", WebUpdateGetTarget);

  server.on("/generate_204", WebUpdateHandleRoot); // handle Andriod phones doing shit to detect if there is 'real' internet and possibly dropping conn.
  server.on("/gen_204", WebUpdateHandleRoot);
  server.on("/library/test/success.html", WebUpdateHandleRoot);
  server.on("/hotspot-detect.html", WebUpdateHandleRoot);
  server.on("/connectivity-check.html", WebUpdateHandleRoot);
  server.on("/check_network_status.txt", WebUpdateHandleRoot);
  server.on("/ncsi.txt", WebUpdateHandleRoot);
  server.on("/fwlink", WebUpdateHandleRoot);

  server.on("/update", HTTP_POST, WebUploadResponseHandler, WebUploadDataHandler);
  server.on("/forceupdate", WebUploadForceUpdateHandler);

  #if defined(TARGET_RX)
    server.on("/model", WebUpdateModelId);
  #endif
  #if defined(GPIO_PIN_PWM_OUTPUTS)
    server.on("/pwm", WebUpdatePwm);
  #endif

  ws.onEvent(WSonEvent);
  server.addHandler(&ws);

  server.onNotFound(WebUpdateHandleNotFound);

  server.begin();

  dnsServer.start(DNS_PORT, "*", ipAddress);
  dnsServer.setErrorReplyCode(DNSReplyCode::NoError);

  startMDNS();

  servicesStarted = true;
  DBGLN("HTTPUpdateServer ready! Open http://%s.local in your browser", wifi_hostname);
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
        #if defined(PLATFORM_ESP8266)
          WiFi.mode(WIFI_AP_STA);
        #else
          WiFi.mode(WIFI_AP);
        #endif
        changeTime = now;
        WiFi.softAPConfig(ipAddress, ipAddress, netMsk);
        WiFi.softAP(wifi_ap_ssid, wifi_ap_password);
        WiFi.scanNetworks(true);
        startServices();
        break;
      case WIFI_STA:
        DBGLN("Connecting to home network '%s'", config.GetSSID());
        wifiMode = WIFI_STA;
        WiFi.mode(wifiMode);
        WiFi.setHostname(wifi_hostname); // hostname must be set after the mode is set to STA
        changeTime = now;
        WiFi.begin(config.GetSSID(), config.GetPassword());
        startServices();
      default:
        break;
    }
    #if defined(PLATFORM_ESP8266)
      MDNS.notifyAPChange();
    #endif
    changeMode = WIFI_OFF;
  }

  if (servicesStarted)
  {
    dnsServer.processNextRequest();
    #if defined(PLATFORM_ESP8266)
      MDNS.update();
    #endif
    // When in STA mode, a small delay reduces power use from 90mA to 30mA when idle
    // In AP mode, it doesn't seem to make a measurable difference, but does not hurt
    if (!Update.isRunning())
      delay(1);
  }
}

static int start()
{
  ipAddress.fromString(wifi_ap_address);

  #ifdef AUTO_WIFI_ON_INTERVAL
    return AUTO_WIFI_ON_INTERVAL * 1000;
  #else
    return DURATION_NEVER;
  #endif
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
  return DURATION_IGNORE;
}

static int timeout()
{
  if (wifiStarted)
  {
    HandleWebUpdate();
    return DURATION_IMMEDIATELY;
  }

  #if defined(TARGET_TX) && defined(AUTO_WIFI_ON_INTERVAL)
  //if webupdate was requested before or AUTO_WIFI_ON_INTERVAL has been elapsed but uart is not detected
  //start webupdate, there might be wrong configuration flashed.
  if(webserverPreventAutoStart == false && connectionState < wifiUpdate && !wifiStarted){
    DBGLN("No CRSF ever detected, starting WiFi");
    connectionState = wifiUpdate;
    return DURATION_IMMEDIATELY;
  }
  #elif defined(TARGET_RX) && defined(AUTO_WIFI_ON_INTERVAL)
  if (!webserverPreventAutoStart && (connectionState == disconnected))
  {
    static bool pastAutoInterval = false;
    // If InBindingMode then wait at least 60 seconds before going into wifi,
    // regardless of if AUTO_WIFI_ON_INTERVAL is set to less
    if (!InBindingMode || AUTO_WIFI_ON_INTERVAL >= 60 || pastAutoInterval)
    {
      // No need to ExitBindingMode(), the radio is about to be stopped. Need
      // to change this before the mode change event so the LED is updated
      InBindingMode = false;
      connectionState = wifiUpdate;
      return DURATION_IMMEDIATELY;
    }
    pastAutoInterval = true;
    return (60 - AUTO_WIFI_ON_INTERVAL) * 1000;
  }
  #endif
  return DURATION_NEVER;
}

device_t WIFI_device = {
  .initialize = wifiOff,
  .start = start,
  .event = event,
  .timeout = timeout
};

#endif
