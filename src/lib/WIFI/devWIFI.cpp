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

#define QUOTE(arg) #arg
#define STR(macro) QUOTE(macro)

#if defined(TARGET_TX)
static const char *myHostname = "elrs_tx";
static const char *ssid = "ExpressLRS TX";
#else
static const char *myHostname = "elrs_rx";
static const char *ssid = "ExpressLRS RX";
#endif
static const char *password = "expresslrs";

static const char *home_wifi_ssid = ""
#ifdef HOME_WIFI_SSID
STR(HOME_WIFI_SSID)
#endif
;
static const char *home_wifi_password = ""
#ifdef HOME_WIFI_PASSWORD
STR(HOME_WIFI_PASSWORD)
#endif
;

static bool wifiStarted = false;
bool webserverPreventAutoStart = false;
extern bool InBindingMode;

static wl_status_t laststatus = WL_IDLE_STATUS;
static volatile WiFiMode_t wifiMode = WIFI_OFF;
static volatile WiFiMode_t changeMode = WIFI_OFF;
static volatile unsigned long changeTime = 0;

static const byte DNS_PORT = 53;
static IPAddress apIP(10, 0, 0, 1);
static IPAddress netMsk(255, 255, 255, 0);
static DNSServer dnsServer;

static AsyncWebServer server(80);
static bool servicesStarted = false;

static bool target_seen = false;
static uint8_t target_pos = 0;
static bool force_update = false;

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
  extern const char *myHostname;

  if (!isIp(request->host()) && request->host() != (String(myHostname) + ".local"))
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

static void WebUpdateSendMode(AsyncWebServerRequest *request)
{
  String s;
  if (wifiMode == WIFI_STA) {
    s = String("{\"mode\":\"STA\",\"ssid\":\"") + config.GetSSID();
  } else {
    s = String("{\"mode\":\"AP\",\"ssid\":\"") + config.GetSSID();
  }
  #if defined(TARGET_RX)
  s += "\",\"modelid\":\"" + String(config.GetModelId());
  #endif
  s += "\"}";
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
  String msg = String("Access Point starting, please connect to access point '") + ssid + "' with password '" + password + "'";
  sendResponse(request, msg, WIFI_AP);
}

static void WebUpdateConnect(AsyncWebServerRequest *request)
{
  DBGLN("Connecting to home network");
  String msg = String("Connecting to network '") + config.GetSSID() + "', connect to http://" +
    myHostname + ".local from a browser on that network";
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
  String msg = String("Home network forgotten, please connect to access point '") + ssid + "' with password '" + password + "'";
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

static void WebUploadResponseHander(AsyncWebServerRequest *request) {
  if (Update.hasError()) {
    StreamString p = StreamString();
    Update.printError(p);
    p.trim();
    DBGLN("Failed to upload firmware: %s", p.c_str());
    request->send(200, "application/json", String("{\"status\": \"error\", \"msg\": \"") + p + "\"}");
  } else {
    if (target_seen) {
      DBGLN("Update complete, rebooting");
      AsyncWebServerResponse *response = request->beginResponse(200, "application/json", "{\"status\": \"ok\", \"msg\": \"Update complete. Please wait for LED to resume blinking before disconnecting power.\"}");
      response->addHeader("Connection", "close");
      request->send(response);
      request->client()->close();
      rebootTime = millis() + 200;
    } else {
      request->send(200, "application/json", "{\"status\": \"error\", \"msg\": \"Wrong firmware uploaded, does not match target type.\"}");
    }
  }
}

static void WebUploadDataHandler(AsyncWebServerRequest *request, const String& filename, size_t index, uint8_t *data, size_t len, bool final) {
  static uint32_t totalSize;
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
    } else {
      #if defined(PLATFORM_ESP32)
        Update.abort();
      #endif
      DBGLN("Wrong firmware uploaded, not %s, update aborted", &target_name[4]);
    }
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

  INFOLN("Begin Webupdater");

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

static void startServices()
{
  if (servicesStarted) {
    return;
  }
  server.on("/", WebUpdateHandleRoot);
  server.on("/main.css", WebUpdateSendCSS);
  server.on("/scan.js", WebUpdateSendJS);
  server.on("/flag.svg", WebUpdateSendFlag);
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

  server.on("/update", HTTP_POST, WebUploadResponseHander, WebUploadDataHandler);

  #if defined(TARGET_RX)
    server.on("/model", WebUpdateModelId);
  #endif

  server.onNotFound(WebUpdateHandleNotFound);

  server.begin();

  dnsServer.start(DNS_PORT, "*", apIP);
  dnsServer.setErrorReplyCode(DNSReplyCode::NoError);

  if (!MDNS.begin(myHostname))
  {
    DBGLN("Error starting mDNS");
    return;
  }
  
  String instance = String(myHostname) + "_" + WiFi.macAddress();
  instance.replace(":", "");
  MDNS.setInstanceName(instance);
  MDNS.addService("http", "tcp", 80);
  MDNS.addServiceTxt("http", "tcp", "vendor", "elrs");
  MDNS.addServiceTxt("http", "tcp", "target", (const char *)&target_name[4]);
  MDNS.addServiceTxt("http", "tcp", "version", VERSION);
  MDNS.addServiceTxt("http", "tcp", "options", String(FPSTR(compile_options)).c_str());
  #if defined(TARGET_RX)
    MDNS.addServiceTxt("http", "tcp", "type", "rx");
  #else
    MDNS.addServiceTxt("http", "tcp", "type", "tx");
  #endif

  servicesStarted = true;
  DBGLN("HTTPUpdateServer ready! Open http://%s.local in your browser", myHostname);
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
        WiFi.mode(wifiMode);
        changeTime = now;
        WiFi.softAPConfig(apIP, apIP, netMsk);
        WiFi.softAP(ssid, password);
        WiFi.scanNetworks(true);
        startServices();
        break;
      case WIFI_STA:
        DBGLN("Connecting to home network '%s'", config.GetSSID());
        wifiMode = WIFI_STA;
        WiFi.mode(wifiMode);
        WiFi.setHostname(myHostname); // hostname must be set after the mode is set to STA
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
    if (!InBindingMode || AUTO_WIFI_ON_INTERVAL >= 60 || pastAutoInterval)
    {
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

#else

device_t WIFI_device = {
  NULL
};

#endif
