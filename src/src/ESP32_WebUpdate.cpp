#ifdef PLATFORM_ESP32

#include <WiFi.h>
#include <DNSServer.h>
#include <ESPmDNS.h>
#include <WebServer.h>
#include <Update.h>
#include <set>
#include <StreamString.h>

#include "ESP32_WebContent.h"

#if defined(Regulatory_Domain_AU_915) || defined(Regulatory_Domain_EU_868) || defined(Regulatory_Domain_IN_866) || defined(Regulatory_Domain_FCC_915) || defined(Regulatory_Domain_AU_433) || defined(Regulatory_Domain_EU_433)
#include "SX127xDriver.h"
extern SX127xDriver Radio;
#endif

#if defined(Regulatory_Domain_ISM_2400)
#include "SX1280Driver.h"
extern SX1280Driver Radio;
#endif

#include "ESP32_hwTimer.h"
extern hwTimer hwTimer;

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
static WebServer server(80);

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

static bool captivePortal()
{
    if (!isIp(server.hostHeader()) && server.hostHeader() != (String(myHostname) + ".local"))
    {
        Serial.println("Request redirected to captive portal");
        server.sendHeader("Location", String("http://") + toStringIp(server.client().localIP()), true);
        server.send(302, "text/plain", ""); // Empty content inhibits Content-length header so we have to close the socket ourselves.
        server.client().stop();             // Stop is needed because we sent no content length
        return true;
    }
    return false;
}

static void WebUpdateSendCSS()
{
  server.sendHeader("Content-Encoding", "gzip");
  server.send_P(200, "text/css", CSS, sizeof(CSS));
}

static void WebUpdateSendJS()
{
  server.sendHeader("Content-Encoding", "gzip");
  server.send_P(200, "text/javascript", SCAN_JS, sizeof(SCAN_JS));
}

static void WebUpdateSendFlag()
{
  server.sendHeader("Content-Encoding", "gzip");
  server.send_P(200, "image/svg+xml", FLAG, sizeof(FLAG));
}

static void WebUpdateHandleRoot()
{
  if (captivePortal())
  { // If captive portal redirect instead of displaying the page.
    return;
  }
  if (server.hasArg("force"))
    force_update = true;
  server.sendHeader("Cache-Control", "no-cache, no-store, must-revalidate");
  server.sendHeader("Pragma", "no-cache");
  server.sendHeader("Expires", "-1");
  server.sendHeader("Content-Encoding", "gzip");
  server.send_P(200, "text/html", INDEX_HTML, sizeof(INDEX_HTML));
}

static void WebUpdateSendMode()
{
  String s;
  if (wifiMode == WIFI_STA) {
    s = String("{\"mode\":\"STA\",\"ssid\":\"") + config.GetSSID() + "\"}";
  } else {
    s = String("{\"mode\":\"AP\",\"ssid\":\"") + config.GetSSID() + "\"}";
  }
  server.send(200, "application/json", s);
}

static void WebUpdateGetTarget()
{
  String s = String("{\"target\":\"") + (const char *)&target_name[4] + "\",\"version\": \"" + VERSION + "\"}";
  server.send(200, "application/json", s);
}

static void WebUpdateSendNetworks()
{
  int numNetworks = WiFi.scanComplete();
  if (numNetworks >= 0) {
    Serial.printf("Found %d networks\n", numNetworks);
    std::set<String> vs;
    String s="[";
    for(int i=0 ; i<numNetworks ; i++) {
      String w = WiFi.SSID(i);
      Serial.printf("found %s\n", w.c_str());
      if (vs.find(w)==vs.end() && w.length()>0) {
        if (!vs.empty()) s += ",";
        s += "\"" + w + "\"";
        vs.insert(w);
      }
    }
    s+="]";
    server.send(200, "application/json", s);
  } else {
    server.send(204, "application/json", "[]");
  }
}

static void sendResponse(const String &msg, WiFiMode_t mode) {
  server.sendHeader("Connection", "close");
  server.send(200, "text/plain", msg);
  server.client().stop();
  changeTime = millis();
  changeMode = mode;
}

static void WebUpdateAccessPoint(void)
{
  Serial.println("Starting Access Point");
  String msg = String("Access Point starting, please connect to access point '") + ssid + "' with password '" + password + "'";
  sendResponse(msg, WIFI_AP);
}

static void WebUpdateConnect(void)
{
  Serial.println("Connecting to home network");
  String msg = String("Connected to network '") + config.GetSSID() + "', connect to http://elrs_tx.local from a browser on that network";
  sendResponse(msg, WIFI_STA);
}

static void WebUpdateSetHome(void)
{
  String ssid = server.arg("network");
  String password = server.arg("password");

  Serial.printf("Setting home network %s\n", ssid.c_str());
  config.SetSSID(ssid.c_str());
  config.SetPassword(password.c_str());
  config.Commit();
  WebUpdateConnect();
}

static void WebUpdateForget(void)
{
  Serial.println("Forget home network");
  config.SetSSID("");
  config.SetPassword("");
  config.Commit();
  String msg = String("Home network forgotten, please connect to access point '") + ssid + "' with password '" + password + "'";
  sendResponse(msg, WIFI_AP);
}

static void WebUpdateHandleNotFound()
{
    if (captivePortal())
    { // If captive portal redirect instead of displaying the error page.
        return;
    }
    String message = F("File Not Found\n\n");
    message += F("URI: ");
    message += server.uri();
    message += F("\nMethod: ");
    message += (server.method() == HTTP_GET) ? "GET" : "POST";
    message += F("\nArguments: ");
    message += server.args();
    message += F("\n");

    for (uint8_t i = 0; i < server.args(); i++)
    {
        message += String(F(" ")) + server.argName(i) + F(": ") + server.arg(i) + F("\n");
    }
    server.sendHeader("Cache-Control", "no-cache, no-store, must-revalidate");
    server.sendHeader("Pragma", "no-cache");
    server.sendHeader("Expires", "-1");
    server.send(404, "text/plain", message);
}

static void startWifi() {
  WiFi.persistent(false);
  WiFi.disconnect();
  WiFi.mode(WIFI_OFF);
  WiFi.setTxPower(WIFI_POWER_13dBm);
  WiFi.setHostname(myHostname);
  WiFi.softAPConfig(apIP, apIP, netMsk);
  WiFi.softAP(ssid, password);
  WiFi.scanNetworks(true);
  if (config.GetSSID()[0]==0 && home_wifi_ssid[0]!=0) {
    config.SetSSID(home_wifi_ssid);
    config.SetPassword(home_wifi_password);
  }
  if (config.GetSSID()[0]==0) {
    changeTime = millis();
    changeMode = WIFI_AP;
  } else {
    Serial.printf("Connecting to home network '%s'\n", config.GetSSID());
    changeTime = millis();
    changeMode = WIFI_STA;
  }
  laststatus = WL_DISCONNECTED;
}

void BeginWebUpdate()
{
    hwTimer.stop();
    Radio.End();
    crsf.End();

    Serial.println("Begin Webupdater");
    Serial.println("Stopping Radio");

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
    server.onNotFound(WebUpdateHandleNotFound);

    server.on(
        "/update", HTTP_POST, []() {
          if (target_seen) {
            if (Update.hasError()) {
              StreamString p = StreamString();
              Update.printError(p);
              server.send(200, "application/json", String("{\"status\": \"error\", \"msg\": \"") + p + "\"}");
            } else {
              server.sendHeader("Connection", "close");
              server.send(200, "application/json", "{\"status\": \"ok\", \"msg\": \"Update complete, please wait 10 seconds before powering of the module\"}");
              server.client().stop();
              delay(100);
              ESP.restart();
            }
          } else {
            server.send(200, "application/json", "{\"status\": \"error\", \"msg\": \"Wrong firmware uploaded, does not match Transmitter module type\"}");
          }
      },
    []() {
      HTTPUpload& upload = server.upload();
      if (upload.status == UPLOAD_FILE_START) {
        Serial.setDebugOutput(true);
        Serial.printf("Update: %s\n", upload.filename.c_str());
        if (!Update.begin()) { //start with max available size
          Update.printError(Serial);
        }
        target_seen = false;
        target_pos = 0;
      } else if (upload.status == UPLOAD_FILE_WRITE) {
        if (Update.write(upload.buf, upload.currentSize) != upload.currentSize) {
          Update.printError(Serial);
        }
        if (force_update)
          target_seen = true;
        if (!target_seen) {
          for (int i=0 ; i<upload.currentSize ;i++) {
            if (upload.buf[i] == target_name[target_pos]) {
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
      } else if (upload.status == UPLOAD_FILE_END) {
        if (target_seen) {
          if (Update.end(true)) { //true to set the size to the current progress
            Serial.printf("Upload Success: %ubytes\nPlease wait for LED to resume blinking before disconnecting power\n", upload.totalSize);
          } else {
            Update.printError(Serial);
          }
        } else {
          Update.abort();
          Serial.printf("Wrong firmware uploaded, not %s, update aborted\n", &target_name[4]);
        }
        Serial.setDebugOutput(false);
      } else if(upload.status == UPLOAD_FILE_ABORTED){
        Update.abort();
        Serial.println("Update was aborted");
      } });

    dnsServer.start(DNS_PORT, "*", apIP);
    dnsServer.setErrorReplyCode(DNSReplyCode::NoError);

    if (!MDNS.begin(myHostname))
    {
      Serial.println("Error starting mDNS");
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
  if (status != laststatus && wifiMode == WIFI_STA) {
        Serial.printf("WiFi status %d\n", status);
        switch(status) {
          case WL_NO_SSID_AVAIL:
          case WL_CONNECT_FAILED:
          case WL_CONNECTION_LOST:
            changeTime = millis();
            changeMode = WIFI_AP;
            break;
          case WL_DISCONNECTED: // try reconnection
            changeTime = millis();
            break;
          default:
            break;
        }
        laststatus = status;
  }
  if (status != WL_CONNECTED && wifiMode == WIFI_STA && (changeTime+30000) < millis()) {
    changeTime = millis();
    changeMode = WIFI_AP;
    Serial.printf("Connection failed %d %ld\n", status, changeTime);
  }
  if (changeMode != wifiMode && changeMode != WIFI_MODE_NULL && (changeTime+500) < millis()) {
    switch(changeMode) {
      case WIFI_AP:
        Serial.println("Changing to AP mode");
        WiFi.disconnect();
        wifiMode = WIFI_AP;
        WiFi.mode(wifiMode);
        WiFi.softAPConfig(apIP, apIP, netMsk);
        WiFi.softAP(ssid, password);
        WiFi.scanNetworks(true);
        break;
      case WIFI_STA:
        Serial.printf("Connecting to home network '%s'\n", config.GetSSID());
        WiFi.mode(WIFI_STA);
        wifiMode = WIFI_STA;
        changeTime = millis();
        WiFi.begin(config.GetSSID(), config.GetPassword());
      default:
        break;
    }
    changeMode = WIFI_MODE_NULL;
  }
  dnsServer.processNextRequest();
  server.handleClient();
  yield();
}

#endif
