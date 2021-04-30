#ifdef PLATFORM_ESP8266
#include "ESP8266_WebUpdate.h"

#include <WiFiClient.h>
#include <DNSServer.h>
#include <ESP8266mDNS.h>
#include <ESP8266WebServer.h>
#include <ESP8266HTTPUpdateServer.h>
#include <set>
#include <StreamString.h>

#include "ESP8266_WebContent.h"

#if defined(Regulatory_Domain_AU_915) || defined(Regulatory_Domain_EU_868) || defined(Regulatory_Domain_IN_866) || defined(Regulatory_Domain_FCC_915) || defined(Regulatory_Domain_AU_433) || defined(Regulatory_Domain_EU_433)
#include "SX127xDriver.h"
extern SX127xDriver Radio;
#endif

#if defined(Regulatory_Domain_ISM_2400)
#include "SX1280Driver.h"
extern SX1280Driver Radio;
#endif

#include "ESP8266_hwTimer.h"
extern hwTimer hwTimer;

#include "config.h"
extern RxConfig config;

static bool target_seen = false;
static uint8_t target_pos = 0;

static const char *ssid = "ExpressLRS RX";
static const char *password = "expresslrs";
static const char *myHostname = "elrs_rx";

static WiFiMode_t wifiMode = WIFI_OFF;
static WiFiMode_t changeMode = WIFI_OFF;
static unsigned long changeTime = 0;

static const byte DNS_PORT = 53;
static IPAddress apIP(10, 0, 0, 1);
static IPAddress netMsk(255, 255, 255, 0);
static DNSServer dnsServer;
static ESP8266WebServer server(80);

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

static void WebUpdateAccessPoint(void)
{
  Serial.println("Starting Access Point");
  String msg = String("Access Point starting, please connect to access point '") + ssid + "' with password '" + password + "'";
  server.send(200, "text/plain", msg);
  changeMode = WIFI_AP;
  changeTime = millis();
}

static void WebUpdateConnect(void)
{
  Serial.println("Connecting to home network");
  String msg = String("Connected to network '") + ssid + "', connect to http://elrs_rx.local from a browser on that network";
  server.send(200, "text/plain", msg);
  changeMode = WIFI_STA;
  changeTime = millis();
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
  server.send(200, "text/plain", msg);
  changeMode = WIFI_AP;
  changeTime = millis();
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
  WiFi.disconnect();   //added to start with the wifi off, avoid crashing
  WiFi.mode(WIFI_OFF); //added to start with the wifi off, avoid crashing
  WiFi.setOutputPower(13);
  WiFi.setPhyMode(WIFI_PHY_MODE_11N);
  WiFi.setHostname(myHostname);
  delay(500);
  WiFi.onStationModeGotIP([](const WiFiEventStationModeGotIP&){
    Serial.println("Connected as Wifi station");
  });
  WiFi.onStationModeDisconnected([](const WiFiEventStationModeDisconnected &){
    Serial.println("Access Point enabled");
    wifiMode = WIFI_AP;
    WiFi.mode(wifiMode);
    WiFi.softAPConfig(apIP, apIP, netMsk);
    WiFi.softAP(ssid, password);
    WiFi.scanNetworks(true);
  });
  if (config.GetSSID()[0]==0) {
    Serial.println("Access Point enabled");
    wifiMode = WIFI_AP;
    WiFi.mode(wifiMode);
    WiFi.softAPConfig(apIP, apIP, netMsk);
    WiFi.softAP(ssid, password);
    WiFi.scanNetworks(true);
  } else {
    Serial.printf("Connecting to home network '%s'\n", config.GetSSID());
    wifiMode = WIFI_STA;
    WiFi.mode(wifiMode);
    WiFi.begin(config.GetSSID(), config.GetPassword());
  }
}

void BeginWebUpdate(void)
{
  hwTimer.stop();

  Radio.RXdoneCallback = NULL;
  Radio.TXdoneCallback = NULL;

  Serial.println("Begin Webupdater");
  Serial.println("Stopping Radio");
  Radio.End();

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

  server.on("/generate_204", WebUpdateHandleRoot); // handle Andriod phones doing shit to detect if there is 'real' internet and possibly dropping conn.
  server.on("/gen_204", WebUpdateHandleRoot);
  server.on("/library/test/success.html", WebUpdateHandleRoot);
  server.on("/hotspot-detect.html", WebUpdateHandleRoot);
  server.on("/connectivity-check.html", WebUpdateHandleRoot);
  server.on("/check_network_status.txt", WebUpdateHandleRoot);
  server.on("/ncsi.txt", WebUpdateHandleRoot);
  server.on("/fwlink", WebUpdateHandleRoot);
  server.onNotFound(WebUpdateHandleNotFound);

  // handler for the /update form POST (once file upload finishes)
  server.on("/update", HTTP_POST, [&](){
          if (target_seen) {
            if (Update.hasError()) {
              StreamString p = StreamString();
              Update.printError(p);
              server.send(200, "text/plain", p.c_str());
            } else {
              server.sendHeader("Connection", "close");
              server.send(200, "text/plain", "Update complete, please wait for LED to start blinking before powering off receiver");
              server.client().stop();
              delay(100);
              ESP.restart();
            }
          } else {
            server.send(200, "text/plain", "Wrong firmware uploaded, does not match Receiver type");
          }
      },
    []() {
      HTTPUpload& upload = server.upload();
      if(upload.status == UPLOAD_FILE_START){
          Serial.setDebugOutput(true);
        WiFiUDP::stopAll();
        Serial.printf("Update: %s\n", upload.filename.c_str());
        uint32_t maxSketchSpace = (ESP.getFreeSketchSpace() - 0x1000) & 0xFFFFF000;
        if (!Update.begin(maxSketchSpace, U_FLASH)){//start with max available size
          Update.printError(Serial);
        }
        target_seen = false;
        target_pos = 0;
      } else if(upload.status == UPLOAD_FILE_WRITE){
        if(Update.write(upload.buf, upload.currentSize) != upload.currentSize){
          Update.printError(Serial);
        }
        if (!target_seen) {
          for (size_t i=0 ; i<upload.currentSize ;i++) {
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
      } else if(upload.status == UPLOAD_FILE_END){
        if (target_seen) {
          if(Update.end(true)){ //true to set the size to the current progress
            Serial.printf("Update Success: %u\nRebooting...\n", upload.totalSize);
          } else {
            Update.printError(Serial);
          }
        } else {
          Serial.printf("Wrong firmware uploaded, not %s, update aborted\n", &target_name[4]);
        }
        Serial.setDebugOutput(false);
      } else if(upload.status == UPLOAD_FILE_ABORTED){
        Serial.println("Update was aborted");
      }
      delay(0);
    });

  server.begin();
  Serial.printf("HTTPUpdateServer ready! Open http://%s.local in your browser\n", myHostname);

  dnsServer.start(DNS_PORT, "*", apIP);
  dnsServer.setErrorReplyCode(DNSReplyCode::NoError);

  if (!MDNS.begin(myHostname))
  {
    Serial.println("Error starting mDNS");
    return;
  }
  MDNS.addService("http", "tcp", 80);

  server.begin();
  Serial.printf("HTTPUpdateServer ready! Open http://%s.local in your browser\n", myHostname);
}

void HandleWebUpdate(void)
{
  if (changeMode != wifiMode && changeMode != WIFI_OFF && changeTime > (millis() - 500)) {
    switch(changeMode) {
      case WIFI_AP:
        WiFi.disconnect();
        break;
      case WIFI_STA:
        WiFi.mode(WIFI_STA);
        wifiMode = WIFI_STA;
        WiFi.begin(config.GetSSID(), config.GetPassword());
      default:
        break;
    }
    changeMode = WIFI_OFF;
  }
  dnsServer.processNextRequest();
  server.handleClient();
  MDNS.update();
  yield();
}

#endif
