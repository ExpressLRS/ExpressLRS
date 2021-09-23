#ifdef PLATFORM_ESP32

#include <WiFi.h>
#include <DNSServer.h>
#include <ESPmDNS.h>
#include <ESPAsyncWebServer.h>
#include <Update.h>
#include <set>
#include <StreamString.h>

#include "ESP32_WebContent.h"
#include "logging.h"

#if defined(Regulatory_Domain_AU_915) || defined(Regulatory_Domain_EU_868) || defined(Regulatory_Domain_IN_866) || defined(Regulatory_Domain_FCC_915) || defined(Regulatory_Domain_AU_433) || defined(Regulatory_Domain_EU_433)
#include "SX127xDriver.h"
extern SX127xDriver Radio;
#endif

#if defined(Regulatory_Domain_ISM_2400)
#include "SX1280Driver.h"
extern SX1280Driver Radio;
#endif

#include "CRSF.h"
extern CRSF crsf;

#include "options.h"
#include "config.h"
extern TxConfig config;

static bool target_seen = false;
static uint8_t target_pos = 0;
static bool force_update = false;

#define QUOTE(arg) #arg
#define STR(macro) QUOTE(macro)

static const char *ssid = "ExpressLRS TX Module"; // The name of the Wi-Fi network that will be created
static const char *password = "expresslrs";       // The password required to connect to it, leave blank for an open network
static const char *myHostname = "elrs_tx";
static const char *home_wifi_ssid = ""
#ifdef HOME_WIFI_SSID
STR(HOME_WIFI_SSID)
#endif
;
static const char *home_wifi_password = STR(HOME_WIFI_PASSWORD);

static wl_status_t laststatus;
static volatile wifi_mode_t wifiMode = WIFI_MODE_NULL;
static volatile wifi_mode_t changeMode = WIFI_MODE_NULL;
static volatile unsigned long changeTime = 0;

static const byte DNS_PORT = 53;
static IPAddress apIP(10, 0, 0, 1);
static IPAddress netMsk(255, 255, 255, 0);
static DNSServer dnsServer;
static AsyncWebServer server(80);

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
  if (request->hasArg("force"))
    force_update = true;
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
    s = String("{\"mode\":\"STA\",\"ssid\":\"") + config.GetSSID() + "\"}";
  } else {
    s = String("{\"mode\":\"AP\",\"ssid\":\"") + config.GetSSID() + "\"}";
  }
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
  String msg = String("Connected to network '") + config.GetSSID() + "', connect to http://elrs_tx.local from a browser on that network";
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
  if (target_seen) {
    if (Update.hasError()) {
      StreamString p = StreamString();
      Update.printError(p);
      request->send(200, "application/json", String("{\"status\": \"error\", \"msg\": \"") + p + "\"}");
    } else {
      AsyncWebServerResponse *response = request->beginResponse(200, "application/json", "{\"status\": \"ok\", \"msg\": \"Update complete, please wait 10 seconds before powering off the module\"}");
      response->addHeader("Connection", "close");
      request->send(response);
      request->client()->close();
      delay(100);
      ESP.restart();
    }
  } else {
    request->send(200, "application/json", "{\"status\": \"error\", \"msg\": \"Wrong firmware uploaded, does not match Transmitter module type\"}");
  }
}

static void WebUploadDataHandler(AsyncWebServerRequest *request, const String& filename, size_t index, uint8_t *data, size_t len, bool final) {
  static uint32_t totalSize;
  if (index == 0) {
    Serial.setDebugOutput(true);
    DBGLN("Update: %s", filename.c_str());
    if (!Update.begin()) { //start with max available size
      Update.printError(Serial);
    }
    target_seen = false;
    target_pos = 0;
    totalSize = 0;
  }
  if (Update.write(data, len)) {
    Update.printError(Serial);
  }
  if (force_update)
    target_seen = true;
  if (!target_seen) {
    for (int i=0 ; i<len ;i++) {
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
  if (final) {
    if (target_seen) {
      if (Update.end(true)) { //true to set the size to the current progress
        DBGLN("Upload Success: %ubytes\nPlease wait for LED to resume blinking before disconnecting power", totalSize);
      } else {
        Update.printError(Serial);
      }
    } else {
      Update.abort();
      DBGLN("Wrong firmware uploaded, not %s, update aborted", &target_name[4]);
    }
    Serial.setDebugOutput(false);
  } 
}

static void startWifi() {
  WiFi.persistent(false);
  WiFi.disconnect();
  WiFi.mode(WIFI_OFF);
  WiFi.setTxPower(WIFI_POWER_13dBm);
  WiFi.setHostname(myHostname);
  if (config.GetSSID()[0]==0 && home_wifi_ssid[0]!=0) {
    config.SetSSID(home_wifi_ssid);
    config.SetPassword(home_wifi_password);
  }
  if (config.GetSSID()[0]==0) {
    DBGLN("Changing to AP mode");
    changeTime = millis();
    changeMode = WIFI_AP;
    wifiMode = WIFI_AP;
    WiFi.mode(wifiMode);
    WiFi.softAPConfig(apIP, apIP, netMsk);
    WiFi.softAP(ssid, password);
    WiFi.scanNetworks(true);
  } else {
    DBGLN("Connecting to home network '%s'", config.GetSSID());
    changeTime = millis();
    changeMode = WIFI_STA;
    wifiMode = WIFI_STA;
    WiFi.mode(wifiMode);
    WiFi.begin(config.GetSSID(), config.GetPassword());
  }
  laststatus = WL_DISCONNECTED;
}

void BeginWebUpdate()
{
  DBGLN("Stopping Radio");
  Radio.End();

  INFOLN("Begin Webupdater");
  startWifi();

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

  server.onNotFound(WebUpdateHandleNotFound);

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
  MDNS.addServiceTxt("http", "tcp", "type", "tx");
  MDNS.addServiceTxt("http", "tcp", "target", (const char *)&target_name[4]);
  MDNS.addServiceTxt("http", "tcp", "version", VERSION);
  MDNS.addServiceTxt("http", "tcp", "options", String(FPSTR(compile_options)).c_str());

  server.begin();
}

void HandleWebUpdate()
{
  wl_status_t status = WiFi.status();
  unsigned long now = millis();
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
  if (status != WL_CONNECTED && wifiMode == WIFI_STA && (changeTime+30000) < now) {
    changeTime = now;
    changeMode = WIFI_AP;
    DBGLN("Connection failed %d", status);
  }
  if (changeMode != wifiMode && changeMode != WIFI_MODE_NULL && (changeTime+500) < now) {
    switch(changeMode) {
      case WIFI_AP:
        DBGLN("Changing to AP mode");
        WiFi.disconnect();
        wifiMode = WIFI_AP;
        WiFi.mode(wifiMode);
        WiFi.softAPConfig(apIP, apIP, netMsk);
        WiFi.softAP(ssid, password);
        WiFi.scanNetworks(true);
        break;
      case WIFI_STA:
        DBGLN("Connecting to home network '%s'", config.GetSSID());
        WiFi.mode(WIFI_STA);
        wifiMode = WIFI_STA;
        changeTime = now;
        WiFi.begin(config.GetSSID(), config.GetPassword());
      default:
        break;
    }
    changeMode = WIFI_MODE_NULL;
  }
  dnsServer.processNextRequest();
}

#endif
